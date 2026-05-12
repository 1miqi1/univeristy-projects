#include "connection.hpp"
#include "logger.hpp"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <ctime>
#include <cstdlib> // Added for exit()

namespace {
// Helper to get timestamp in the format: YYYY.MM.DD HH.MM.SS
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

void Connection::init_tls() {
    if (!use_tls) return;

    LOGD("Initializing OpenSSL library...");
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        syserr("Critical Error: SSL_CTX_new failed"); // Level 2;
    }
}

void Connection::resolve_name(const std::string& host, const std::string& port, int family_pref) {
    // Clean up any existing address lists and sockets before re-resolving
    close_connection();

    this->host = host;
    this->port = port;
    this->family_pref = family_pref;

    LOGD("\n%s", get_timestamp().c_str());

    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = family_pref;
    hints.ai_socktype = SOCK_STREAM;

    LOGI("Resolving name: %s", host.c_str()); // Level 1
    int s = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    
    if (s != 0) {
        syserr("Critical Error: getaddrinfo failed: %s", gai_strerror(s)); // Level 2
    }

    this->list_of_connections = result;
    this->current_connection = result;
    this->state = ConnectionState::CHOOSING_NEW_SOCKET;
}

void Connection::connect() {
    if (!this->current_connection) {
        fatal("Critical Error: No addresses available to attempt connection."); // Level 2
    }

    char ip_buf[INET6_ADDRSTRLEN];
    getnameinfo(this->current_connection->ai_addr, this->current_connection->ai_addrlen, 
                ip_buf, sizeof(ip_buf), nullptr, 0, NI_NUMERICHOST);

    // Phase 1: Initialize the socket and start the non-blocking connect call
    if (this->state == ConnectionState::CHOOSING_NEW_SOCKET) {
        sockfd = socket(this->current_connection->ai_family, 
                        this->current_connection->ai_socktype, 
                        this->current_connection->ai_protocol);
        
        if (sockfd == -1) {
            LOGW("Non-critical Error: socket() failed: %s", strerror(errno)); // Level 3
            this->try_next_address();
            return;
        }

        // Set the socket to non-blocking mode
        if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
            syserr("Critical Error: fcntl O_NONBLOCK failed: %s", strerror(errno)); // Level 2
        }

        LOGI("Connecting to %s", ip_buf); // Level 1
        if (::connect(sockfd, this->current_connection->ai_addr, this->current_connection->ai_addrlen) == 0) {
            LOGI("Connected immediately to %s", ip_buf); // Level 1
            this->state = use_tls ? ConnectionState::TLS_HANDSHAKING : ConnectionState::CONNECTED;
            if (!use_tls) this->ever_connected = true;
        } else {
            if (errno == EINPROGRESS) {
                LOGD("Connection in progress (EINPROGRESS) for %s", ip_buf); // Level 4
                this->state = ConnectionState::TRYING_TO_CONNECT;
                return; // CRITICAL: Return to the poll loop!
            } else {
                LOGW("Non-critical Error: Immediate connect failure to %s: %s", ip_buf, strerror(errno)); // Level 3
                this->try_next_address();
                return;
            }
        }
    }

    // Phase 2: Check the result of the connect attempt (after poll() signals POLLOUT)
    if (this->state == ConnectionState::TRYING_TO_CONNECT) {
        int error = 0;
        socklen_t len = sizeof(error);
        
        // Retrieve the pending error from the socket
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            LOGW("Non-critical Error: getsockopt SO_ERROR failed: %s", strerror(errno)); // Level 3
        }

        if (error == 0) {
            LOGI("Successfully connected to %s", ip_buf); // Level 1
            if (use_tls) {
                this->state = ConnectionState::TLS_HANDSHAKING;
            } else {
                this->state = ConnectionState::CONNECTED;
                this->ever_connected = true;
                return; 
            }
        } else {
            LOGW("Non-critical Error: Failed to connect to %s: %s", ip_buf, strerror(error)); // Level 3
            this->try_next_address();
            return;
        }
    }

    // Phase 3: Non-blocking TLS Handshake
    if (this->state == ConnectionState::TLS_HANDSHAKING || 
        this->state == ConnectionState::TLS_WANT_READ || 
        this->state == ConnectionState::TLS_WANT_WRITE) {

        if (!ssl) {
            if (!ssl_ctx) init_tls();
            ssl = SSL_new(ssl_ctx);
            if (!ssl) {
                syserr("Critical Error: SSL_new failed"); // Level 2
            }
            SSL_set_fd(ssl, sockfd);
        }

        int ret = SSL_connect(ssl);
        
        if (ret == 1) {
            LOGI("TLS handshake completed successfully"); // Level 1
            this->state = ConnectionState::CONNECTED;
            this->ever_connected = true;
        } else {
            int err = SSL_get_error(ssl, ret);
            if (err == SSL_ERROR_WANT_READ) {
                LOGD("TLS handshake wants read (POLLIN)"); // Level 4
                this->state = ConnectionState::TLS_WANT_READ;
                return; 
            } else if (err == SSL_ERROR_WANT_WRITE) {
                LOGD("TLS handshake wants write (POLLOUT)"); // Level 4
                this->state = ConnectionState::TLS_WANT_WRITE; 
                return;
            } else {
                LOGW("Non-critical Error: SSL handshake failed, error code: %d", err); // Level 3
                this->try_next_address();
                return;
            }
        }
    }
}

void Connection::try_next_address() {
    LOGD("Attempting next resolved address..."); // Level 4

    // Clean up SSL object if we failed mid-handshake
    if (ssl) {
        SSL_free(ssl);
        ssl = nullptr;
    }

    // Close current socket before trying the next IP
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }

    if (this->current_connection && this->current_connection->ai_next != nullptr) {
        this->current_connection = this->current_connection->ai_next;
        this->state = ConnectionState::CHOOSING_NEW_SOCKET;
        this->connect(); 
    } else {
        fatal("Critical Error: Could not establish connection to any resolved address for %s", host.c_str()); // Level 2
    }
}

void Connection::reconnect() {
    LOGI("Restarting connection process to %s:%s...", host.c_str(), port.c_str()); // Level 1
    resolve_name(this->host, this->port, this->family_pref);
}

void Connection::close_connection() {
    // Free the linked list allocated by getaddrinfo
    if (this->list_of_connections) {
        freeaddrinfo(this->list_of_connections);
        this->list_of_connections = nullptr;
        this->current_connection = nullptr;
    }
    
    // Close the file descriptor
    if (this->sockfd != -1) {
        LOGD("Closing socket descriptor %d", this->sockfd); // Level 4
        close(this->sockfd);
        this->sockfd = -1;
    }
    this->state = ConnectionState::IDLE;

    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }

    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }
}

int Connection::read(void *buf, size_t len) {
    if (this->state != ConnectionState::CONNECTED) return -1;

    if (use_tls) {
        int n = SSL_read(ssl, buf, len);
        if (n > 0) return n;

        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ) {
            LOGD("SSL_read would block (WANT_READ)"); // Level 4
            this->state = ConnectionState::CONNECTED; 
            return 0; 
        }
        if (err == SSL_ERROR_WANT_WRITE) {
            LOGD("SSL_read requires writing (WANT_WRITE)"); // Level 4
            this->state = ConnectionState::TLS_WANT_WRITE; 
            return 0; 
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            LOGI("Server closed TLS connection"); // Level 1
            this->state = ConnectionState::SERVER_CLOSED_CONNECTION;
            return 0;
        }
        
        syserr("Error: SSL_read failed with error code: %d", err); // Level 3
    } else {
        ssize_t n = recv(sockfd, buf, len, 0);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOGD("recv would block (EAGAIN/EWOULDBLOCK)"); // Level 4
                return 0;
            }
            syserr("Error: Recv failed: %s", strerror(errno)); // Level 3
        }
        if (n == 0) {
            LOGI("Server closed connection"); // Level 1
            this->state = ConnectionState::SERVER_CLOSED_CONNECTION;
            return 0;
        }
        return (int)n;
    }
}

int Connection::write(const void *buf, size_t len) {
    if (this->state != ConnectionState::CONNECTED) return -1;

    if (use_tls) {
        int n = SSL_write(ssl, buf, len);
        if (n > 0) {
            LOGD("TLS sent %d bytes", n); // Level 4
            return n;
        }

        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_WRITE) {
            LOGD("SSL_write would block (WANT_WRITE)"); // Level 4
            this->state = ConnectionState::CONNECTED; 
            return 0;
        }
        if (err == SSL_ERROR_WANT_READ) {
            LOGD("SSL_write requires reading (WANT_READ)"); // Level 4
            this->state = ConnectionState::TLS_WANT_READ;
            return 0;
        }
        
        syserr("Error: SSL_write failed with error code: %d", err); // Level 3
        return -1;
    } else {
        ssize_t n = send(sockfd, buf, len, 0);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOGD("send would block (EAGAIN/EWOULDBLOCK)"); // Level 4
                return 0;
            }
            syserr("Error: send failed: %s", strerror(errno)); // Level 3
        }
        LOGD("Sent %zd bytes", n); // Level 4
        return (int)n;
    }
}

int Connection::pending() {
    if (use_tls && ssl)
        return SSL_pending(ssl);

    return 0;
}