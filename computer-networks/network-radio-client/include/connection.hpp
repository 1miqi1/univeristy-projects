#pragma once

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>



/**
 * @enum ConnectionState
 * @brief Represents the current phase of the non-blocking connection process.
 */
enum class ConnectionState {
    IDLE,
    CHOOSING_NEW_SOCKET,
    TRYING_TO_CONNECT,
    SERVER_CLOSED_CONNECTION,
    CONNECTED,
    TLS_WANT_READ,
    TLS_WANT_WRITE,
    TLS_HANDSHAKING,
    FAILED
};

/**
 * @class Connection
 * @brief Manages a TCP/TLS network connection using non-blocking I/O.
 *        Designed to work with poll().
 */
class Connection {
public:
    Connection() = default;
    ~Connection();

    void resolve_name(const std::string& host,
                      const std::string& port,
                      int family_pref = AF_UNSPEC);

    void connect();
    void reconnect();
    void close_connection();

    // =========================
    // Poll-friendly interface
    // =========================

    int get_sockfd() const { return sockfd; }
    ConnectionState get_state() const { return state; }
    bool is_connected() const { return state == ConnectionState::CONNECTED; }

    // Unified I/O (TCP or TLS)
    int read(void *buf, size_t len);
    int write(const void *buf, size_t len);

    // IMPORTANT for poll + SSL buffering
    int pending();

    bool is_tls_enabled() const { return use_tls; }

    void enable_tls(bool v) { use_tls = v; }

private:
    void try_next_address();
    void init_tls();

    int sockfd = -1;

    std::string host;
    std::string port;
    int family_pref = AF_UNSPEC;

    bool ever_connected = false;

    // TLS
    SSL *ssl = nullptr;
    SSL_CTX *ssl_ctx = nullptr;
    bool use_tls = false;

    ConnectionState state = ConnectionState::IDLE;

    struct addrinfo *list_of_connections = nullptr;
    struct addrinfo *current_connection = nullptr;
};