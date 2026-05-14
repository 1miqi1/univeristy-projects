#include "connection.hpp"
#include "logger.hpp"
#include "exceptions.hpp" // Added the exceptions header
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <openssl/err.h>

namespace {
    static std::string get_timestamp() {
        char buf[20];
        time_t now = time(nullptr);
        struct tm *tm_info = localtime(&now);
        strftime(buf, sizeof(buf), "%Y.%m.%d %H.%M.%S", tm_info);
        return std::string(buf);
    }

    // Helper to drain the OpenSSL error queue into a readable string
    static std::string get_openssl_error() {
        std::string err_msg;
        unsigned long err_code;
        while ((err_code = ERR_get_error()) != 0) {
            char buf[256];
            ERR_error_string_n(err_code, buf, sizeof(buf));
            err_msg += std::string(buf) + " | ";
        }
        return err_msg.empty() ? "Unknown OpenSSL error" : err_msg;
    }
}

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
}

void Connection::resolve_name(const std::string& scheme, const std::string& host, const std::string& port, int family_pref) {
    close_connection();

    this->scheme = scheme;
    this->host = host;
    this->port = port;
    this->family_pref = family_pref;

    this->use_tls = scheme == "https";

    LOGD("\n%s", get_timestamp().c_str());

    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = family_pref;
    hints.ai_socktype = SOCK_STREAM;

    LOGI("Resolving name: %s", host.c_str());
    int s = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    
    if (s != 0) {
        // getaddrinfo doesn't set errno, it returns its own error codes. 
        // We throw the string under our unified ConnectionException.
        throw ConnectionException("getaddrinfo failed for " + host + ": " + std::string(gai_strerror(s)));
    }

    this->list_of_connections = result;
    this->current_connection = result;
}

void Connection::connect() {
    if (!this->list_of_connections) {
        // Catch-all for improper usage/bad request state
        throw InvalidRequestException("No addresses available to attempt connection. Did you call resolve_name?");
    }

    bool successfully_connected = false;
    std::string last_error_msg = "Unknown error";

    for (struct addrinfo *p = this->list_of_connections; p != nullptr; p = p->ai_next) {
        char ip_buf[INET6_ADDRSTRLEN];
        getnameinfo(p->ai_addr, p->ai_addrlen, ip_buf, sizeof(ip_buf), nullptr, 0, NI_NUMERICHOST);
        
        LOGI("Attempting connection to %s...", ip_buf);

        // 1. Create Socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            last_error_msg = std::string("socket() failed: ") + strerror(errno);
            LOGW("Non-critical Error: %s", last_error_msg.c_str());
            continue; // Try next address
        }

        // 2. Perform Blocking Connect
        if (::connect(sockfd, p->ai_addr, p->ai_addrlen) != 0) {
            last_error_msg = std::string("connect() failed to ") + ip_buf + ": " + strerror(errno);
            LOGW("Non-critical Error: %s", last_error_msg.c_str());
            close(sockfd);
            sockfd = -1;
            continue; // Try next address
        }

        // 3. Perform Blocking TLS Handshake
        if (use_tls) {
            if (!ssl_ctx) init_tls();
            ssl = SSL_new(ssl_ctx);
            if (!ssl) {
                close(sockfd);
                sockfd = -1;
                throw ConnectionException("SSL_new failed: " + get_openssl_error());
            }
            SSL_set_fd(ssl, sockfd);

            int ret = SSL_connect(ssl);
            if (ret <= 0) {
                last_error_msg = "SSL handshake failed to " + std::string(ip_buf) + ": " + get_openssl_error();
                LOGW("Non-critical Error: %s", last_error_msg.c_str());
                SSL_free(ssl);
                ssl = nullptr;
                close(sockfd);
                sockfd = -1;
                continue; // Try next address
            }
            LOGI("TLS handshake completed successfully");
        }

        // 4. IMPORTANT: Switch to NON-BLOCKING for the poll loop
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            close_connection();
            // Wrap system errno in our NetworkError
            throw NetworkError(errno, "Failed to set O_NONBLOCK");
        }

        successfully_connected = true;
        this->current_connection = p; // Save the successful connection
        LOGI("Successfully connected to %s", ip_buf);
        break; // Connection successful, exit the loop
    }

    if (!successfully_connected) {
        throw ConnectionException("Could not establish connection to " + host + ". Last error: " + last_error_msg);
    }

    this->state = ConnectionState::CONNECTED;
    this->ever_connected = true;
    last_activity = std::chrono::steady_clock::now();
}

void Connection::reconnect() {
    LOGI("Restarting connection process to %s:%s...", host.c_str(), port.c_str());
    resolve_name(this->scheme, this->host, this->port, this->family_pref);
    connect(); 
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

    if (this->sockfd != -1) {
        LOGD("Closing socket descriptor %d", this->sockfd);
        close(this->sockfd);
        this->sockfd = -1;
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

        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0; // No data yet (Non-blocking)
        }
        
        if (err == SSL_ERROR_ZERO_RETURN) {
            throw ServerDisconnectedException();
        } 

        // Any other SSL error
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

        // Properly mapped to our unified NetworkError wrap
        throw NetworkError(errno, "recv failed");
    }
}

ssize_t Connection::write(const void *buf, size_t len) {
    if (this->state != ConnectionState::CONNECTED) return -1;

    if (use_tls) {
        ssize_t n = SSL_write(ssl, buf, len);
        if (n > 0) return n;

        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) return 0;
        
        throw ConnectionException("SSL_write failed: " + get_openssl_error());
    } else {
        ssize_t n = send(sockfd, buf, len, 0);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            // Properly mapped to our unified NetworkError wrap
            throw NetworkError(errno, "send failed");
        }
        return n;
    }
}

ssize_t Connection::pending() {
    return (use_tls && ssl) ? SSL_pending(ssl) : 0;
}