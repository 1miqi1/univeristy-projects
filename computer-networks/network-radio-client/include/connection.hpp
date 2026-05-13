#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <vector>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <chrono>
#include <unistd.h>
#include <stdexcept>



/**
 * States used by the Connection state machine.
 */
enum class ConnectionState {
    IDLE,
    CONNECTED,
    SERVER_CLOSED_CONNECTION
};

class Connection {
public:
    /**
     * @param use_tls Initially determines if SSL should be prepared.
     */
    Connection(bool use_tls = false) 
        : scheme("http"),
          family_pref(AF_UNSPEC),
          use_tls(use_tls), 
          sockfd(-1), 
          list_of_connections(nullptr), 
          current_connection(nullptr), 
          ssl(nullptr), 
          ssl_ctx(nullptr), 
          state(ConnectionState::IDLE), 
          ever_connected(false),
          last_activity(std::chrono::steady_clock::now()) {} 

    ~Connection();

    // Core Connection Methods
    void init_tls();
    void resolve_name(const std::string& scheme, const std::string& host, 
                      const std::string& port, int family_pref);
    void connect();
    void try_next_address();
    void reconnect();
    void close_connection();

    // I/O Methods (Non-blocking after successful connect)
    ssize_t read(void *buf, size_t len);
    ssize_t write(const void *buf, size_t len);
    ssize_t pending();

    // Time handling
    // Returns the absolute time point of the deadline
    long long get_ms_until_timeout(int timeout_ms);

    // Getters 
    int get_sockfd() const { return sockfd; }
    ConnectionState get_state() const { return state; }
    bool has_ever_connected() const { return ever_connected; }

private:
    // Connection Info
    std::string scheme; 
    std::string host;
    std::string port;
    int family_pref;
    bool use_tls;

    // Socket & Network members
    int sockfd;
    struct addrinfo *list_of_connections;
    struct addrinfo *current_connection;

    // OpenSSL members
    SSL *ssl;
    SSL_CTX *ssl_ctx;

    // State tracking
    ConnectionState state;
    bool ever_connected;
    std::chrono::steady_clock::time_point last_activity;

    // Prevent copying
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
}; // Added missing semicolon here

#endif // CONNECTION_HPP