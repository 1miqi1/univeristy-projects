#include "connection.hpp"
#include "logger.hpp"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <ctime>
#include <cstdlib>
#include <unistd.h> // For close()

namespace {
static std::string get_timestamp() {
    char buf[20];
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    strftime(buf, sizeof(buf), "%Y.%m.%d %H.%M.%S", tm_info);
    return std::string(buf);
}
}

Connection::~Connection() {
    close_connection();
}

long long Connection::get_ms_until_timeout(int timeout_ms){
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
        syserr("Critical Error: SSL_CTX_new failed"); 
    }
}

    void Connection::resolve_name(const std::string& sheme, const std::string& host, const std::string& port, int family_pref) {
        close_connection();

        this->scheme = sheme;
        this->host = host;
        this->port = port;
        this->family_pref = family_pref;

        this->use_tls = sheme == "https";

        LOGD("\n%s", get_timestamp().c_str());

        struct addrinfo hints;
        struct addrinfo *result;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = family_pref;
        hints.ai_socktype = SOCK_STREAM;

        LOGI("Resolving name: %s", host.c_str());
        int s = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
        
        if (s != 0) {
            syserr("Critical Error: getaddrinfo failed: %s", gai_strerror(s));
        }

        this->list_of_connections = result;
        this->current_connection = result;
    }

void Connection::connect() {
    if (!this->current_connection) {
        fatal("Critical Error: No addresses available to attempt connection.");
    }

    char ip_buf[INET6_ADDRSTRLEN];
    getnameinfo(this->current_connection->ai_addr, this->current_connection->ai_addrlen, 
                ip_buf, sizeof(ip_buf), nullptr, 0, NI_NUMERICHOST);

    LOGI("Connecting to %s (blocking)...", ip_buf);

    // 1. Create Socket
    sockfd = socket(this->current_connection->ai_family, 
                    this->current_connection->ai_socktype, 
                    this->current_connection->ai_protocol);
    
    if (sockfd == -1) {
        LOGW("Non-critical Error: socket() failed: %s", strerror(errno));
        this->try_next_address();
        return;
    }

    // 2. Perform Blocking Connect
    if (::connect(sockfd, this->current_connection->ai_addr, this->current_connection->ai_addrlen) != 0) {
        LOGW("Non-critical Error: Connect failure to %s: %s", ip_buf, strerror(errno));
        this->try_next_address();
        return;
    }

    // 3. Perform Blocking TLS Handshake
    if (use_tls) {
        if (!ssl_ctx) init_tls();
        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            syserr("Critical Error: SSL_new failed");
        }
        SSL_set_fd(ssl, sockfd);

        int ret = SSL_connect(ssl);
        if (ret <= 0) {
            int err = SSL_get_error(ssl, ret);
            LOGW("Non-critical Error: SSL handshake failed to %s, error code: %d", ip_buf, err);
            this->try_next_address();
            return;
        }
        LOGI("TLS handshake completed successfully");
    }

    // 4. IMPORTANT: Switch to NON-BLOCKING for the poll loop
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        syserr("Critical Error: Failed to set O_NONBLOCK: %s", strerror(errno));
    }

    LOGI("Successfully connected to %s", ip_buf);
    this->state = ConnectionState::CONNECTED;
    this->ever_connected = true;
    last_activity = std::chrono::steady_clock::now();

}

void Connection::try_next_address() {
    if (ssl) { SSL_free(ssl); ssl = nullptr; }
    if (sockfd != -1) { close(sockfd); sockfd = -1; }

    if (this->current_connection && this->current_connection->ai_next != nullptr) {
        LOGD("Attempting next resolved address...");
        this->current_connection = this->current_connection->ai_next;
        this->connect(); // Recursive call to the new blocking connect
    } else {
        fatal("Critical Error: Could not establish connection to any address for %s", host.c_str());
    }
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
        if (n > 0) return n;

        if(n <= 0){
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
            
            if (err == SSL_ERROR_ZERO_RETURN) {
                LOGI("Server closed TLS connection");
                this->state = ConnectionState::SERVER_CLOSED_CONNECTION;
                return 0;
            } 
            return -1;
        }
    } else {
        n = recv(sockfd, buf, len, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (n == 0) {
            LOGI("Server closed connection");
            this->state = ConnectionState::SERVER_CLOSED_CONNECTION;
            return 0;
        }
    }

    if (n > 0) {
        last_activity = std::chrono::steady_clock::now(); // Update timer
    }

    return n;
}

ssize_t Connection::write(const void *buf, size_t len) {
    if (this->state != ConnectionState::CONNECTED) return -1;

    if (use_tls) {
        ssize_t n = SSL_write(ssl, buf, len);
        if (n > 0) return n;

        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) return 0;
        return -1;
    } else {
        ssize_t n = send(sockfd, buf, len, 0);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        return n;
    }
}

ssize_t Connection::pending() {
    return (use_tls && ssl) ? SSL_pending(ssl) : 0;
}