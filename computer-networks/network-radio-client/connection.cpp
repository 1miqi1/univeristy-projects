#include "connection.hpp"
#include "logger.hpp"
#include "exceptions.hpp"

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <openssl/err.h>

namespace {

constexpr size_t TIMESTAMP_BUF_SIZE = 32;
constexpr size_t OPENSSL_ERR_BUF_SIZE = 256;
constexpr int INVALID_SOCKET = -1;

/**
 * @brief Helper to format the current local time into a standard string layout.
 */
static std::string get_timestamp() {
    char buf[TIMESTAMP_BUF_SIZE];
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    strftime(buf, sizeof(buf), "%Y.%m.%d %H.%M.%S", tm_info);
    return std::string(buf);
}

/**
 * @brief Helper to drain the OpenSSL error queue into a readable string.
 */
static std::string get_openssl_error() {
    std::string err_msg;
    unsigned long err_code;
    
    while ((err_code = ERR_get_error()) != 0) {
        char buf[OPENSSL_ERR_BUF_SIZE];
        ERR_error_string_n(err_code, buf, sizeof(buf));
        err_msg += std::string(buf) + " | ";
    }
    
    return err_msg.empty() ? "Unknown OpenSSL error" : err_msg;
}

} // anonymous namespace

Connection::~Connection() {
    close_connection();
}

long long Connection::get_ms_until_timeout(int timeout_ms) {
    auto now = std::chrono::steady_clock::now();
    auto deadline = this->last_activity + std::chrono::milliseconds(timeout_ms);

    if (now >= deadline) {
        return 0;
    }

    auto remaining = deadline - now;
    return std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
}

void Connection::init_tls() {
    if (!use_tls || ssl_ctx) return;

    LOGD("Initializing OpenSSL library...");
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        throw ConnectionException("SSL_CTX_new failed initializing context: " + get_openssl_error());
    }

    SSL_CTX_set_default_verify_paths(ssl_ctx);  
}

void Connection::resolve_name(const std::string& scheme, std::string& host, const std::string& port, int family_pref) {
    close_connection(); 

    this->scheme = scheme;
    if (host.size() > 0 && host[0] == '[' && host[host.size() - 1] == ']') {
        host = host.substr(1, host.size() - 2);
    }
    this->host = host;
    this->port = port;
    this->family_pref = family_pref;

    this->use_tls = (scheme == "https");

    LOGI("%s", get_timestamp().c_str());

    struct addrinfo hints;
    struct addrinfo *result = nullptr;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = family_pref;
    hints.ai_socktype = SOCK_STREAM;

    LOGI("resolving name: %s", host.c_str());
    int s = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    
    if (s != 0) {
        throw ConnectionException("getaddrinfo failed for " + host + ": " + std::string(gai_strerror(s)));
    }

    if (this->family_pref == AF_UNSPEC && result != nullptr) {
        this->family_pref = result->ai_family;
    }

    this->list_of_connections = result;
    this->current_connection = result;
}

void Connection::set_socket_non_blocking() {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        std::string msg = "Getting socket flags failed: " + std::string(strerror(errno));
        throw NetworkError(errno, msg);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::string msg = "Setting socket to non-blocking failed: " + std::string(strerror(errno));
        throw NetworkError(errno, msg);
    }
}


bool Connection::attempt_tls_handshake(struct addrinfo *p, const char* ip_buf, const char* service, std::string& last_error_msg) {
    if (!ssl_ctx) init_tls();
    ssl = SSL_new(ssl_ctx);
    
    if (!ssl) {
        last_error_msg = "SSL_new failed: " + get_openssl_error();
        close(sockfd);
        sockfd = INVALID_SOCKET;
        return false;
    }
    
    SSL_set_fd(ssl, sockfd);
    SSL_set_tlsext_host_name(ssl, this->host.c_str());

    int ret = SSL_connect(ssl);
    if (ret <= 0) {
        std::string addr_port = (p->ai_family == AF_INET6)
                                ? std::string("[") + ip_buf + "]:" + service
                                : std::string(ip_buf) + ":" + service;
        last_error_msg = "SSL handshake failed to " + addr_port + ": " + get_openssl_error();
        LOGW(last_error_msg.c_str());
        
        SSL_free(ssl);
        ssl = nullptr;
        close(sockfd);
        sockfd = INVALID_SOCKET;
        return false;
    }
    
    LOGD("TLS handshake completed successfully");
    return true;
}

bool Connection::attempt_raw_connection(struct addrinfo *p, const char* ip_buf, const char* service, std::string& last_error_msg) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == INVALID_SOCKET) {
        last_error_msg = std::string("socket() failed: ") + strerror(errno);
        LOGW(last_error_msg.c_str());
        return false;
    }

    if (::connect(sockfd, p->ai_addr, p->ai_addrlen) != 0) {
        std::string addr_port = (p->ai_family == AF_INET6)
                                ? std::string("[") + ip_buf + "]:" + service
                                : std::string(ip_buf) + ":" + service;

        last_error_msg = "connect() failed to " + addr_port + " : " + strerror(errno);
        LOGW(last_error_msg.c_str());
        
        close(sockfd);
        sockfd = INVALID_SOCKET;
        return false;
    }
    
    return true;
}


void Connection::connect() {
    if (!this->list_of_connections) {
        throw InvalidRequestException("No addresses available to attempt connection. Did you call resolve_name?");
    }

    bool successfully_connected = false;
    std::string last_error_msg = "Unknown error";

    for (struct addrinfo *p = this->list_of_connections; p != nullptr; p = p->ai_next) {
        if (p->ai_family != family_pref) {
            continue;
        }

        char ip_buf[INET6_ADDRSTRLEN];
        char service[NI_MAXSERV];

        int rc = getnameinfo(
            p->ai_addr, p->ai_addrlen,
            ip_buf, sizeof(ip_buf),
            service, sizeof(service),
            NI_NUMERICHOST | NI_NUMERICSERV
        );
        (void)rc;

        const char* fmt = (p->ai_family == AF_INET6)
                            ? "connecting to server [%s]:%s"
                            : "connecting to server %s:%s";

        LOGI(fmt, ip_buf, service);

        // 1. Create Socket and Perform Blocking Connect
        if (!attempt_raw_connection(p, ip_buf, service, last_error_msg)) {
            continue;
        }

        // 2. TLS handshake (if enabled)
        if (use_tls && !attempt_tls_handshake(p, ip_buf, service, last_error_msg)) {
            continue;
        }

        // 3. Set to Non-Blocking
        set_socket_non_blocking();

        successfully_connected = true;
        this->current_connection = p;
        break;
    }

    if (!successfully_connected) {
        throw ConnectionException("Could not establish connection to " + this->host + ". Last error: " + last_error_msg);
    }

    this->state = ConnectionState::CONNECTED;
    this->ever_connected = true;
    last_activity = std::chrono::steady_clock::now();
}

void Connection::close_connection() {
    if (this->list_of_connections) {
        freeaddrinfo(this->list_of_connections);
        this->list_of_connections = nullptr;
        this->current_connection = nullptr;
    }
    
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }

    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }

    if (this->sockfd != INVALID_SOCKET) {
        LOGD("Closing socket descriptor %d", this->sockfd);
        close(this->sockfd);
        this->sockfd = INVALID_SOCKET;
    }
    
    this->state = ConnectionState::IDLE;
}

ssize_t Connection::read(void *buf, size_t len) {
    if (this->state != ConnectionState::CONNECTED) return -1;

    ssize_t n;

    if (use_tls) {
        n = SSL_read(ssl, buf, len);
        
        if (n > 0) {
            last_activity = std::chrono::steady_clock::now();
            return n;
        }

        int err = SSL_get_error(ssl, static_cast<int>(n));
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0; // No data yet (Non-blocking)
        }
        
        if (err == SSL_ERROR_ZERO_RETURN) {
            throw ServerDisconnectedException();
        } 

        throw ConnectionException("SSL_read failed: " + get_openssl_error());

    } else {
        n = recv(sockfd, buf, len, 0);
        
        if (n > 0) {
            last_activity = std::chrono::steady_clock::now();
            return n;
        }
        
        if (n == 0) {
            throw ServerDisconnectedException();
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data yet
        }

        if (errno == ECONNRESET) {
            throw ServerDisconnectedException(); 
        }

        throw NetworkError(errno, "recv failed");
    }
}

ssize_t Connection::write(const void *buf, size_t len) {
    if (this->state != ConnectionState::CONNECTED) return -1;

    if (use_tls) {
        ssize_t n = SSL_write(ssl, buf, len);
        if (n > 0){
            return n;
        } 

        int err = SSL_get_error(ssl, static_cast<int>(n));

        if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
            return 0;
        } 
        
        throw ConnectionException("SSL_write failed: " + get_openssl_error());
    } else {
        ssize_t n = send(sockfd, buf, len, MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { 
                return 0;
            }

            if (errno == ECONNRESET) {
                throw ServerDisconnectedException(); 
            }

            throw NetworkError(errno, "send failed");
        }
        return n;
    }
}

ssize_t Connection::pending() {
    return (use_tls && ssl) ? SSL_pending(ssl) : 0;
}