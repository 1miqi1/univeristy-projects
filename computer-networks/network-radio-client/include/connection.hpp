#pragma once

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
 * @brief States used by the Connection state machine.
 */
enum class ConnectionState {
    IDLE,
    CONNECTED,
    SERVER_CLOSED_CONNECTION
};

/**
 * @brief Manages a network connection, handling both raw TCP and TLS/SSL encapsulation.
 */
class Connection {
public:
    /**
     * @brief Constructs a new Connection object.
     * 
     * @param use_tls Initially determines if SSL context should be prepared.
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
          last_activity(std::chrono::steady_clock::now()) 
    {} 

    ~Connection();

    // Prevent copying to avoid accidental double-closes of sockets/SSL contexts
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    /**
     * @brief Initializes the OpenSSL library context if TLS is required.
     */
    void init_tls();

    /**
     * @brief Resolves a hostname and prepares the internal list of address structures.
     * 
     * @param scheme      The protocol scheme ("http" or "https").
     * @param host        The target hostname or IP address.
     * @param port        The target port number as a string.
     * @param family_pref Address family preference (e.g., AF_INET, AF_INET6, AF_UNSPEC).
     * @throws ConnectionException if name resolution fails.
     */
    void resolve_name(const std::string& scheme, const std::string& host, 
                      const std::string& port, int family_pref);

    /**
     * @brief Iterates through resolved addresses and attempts to connect and perform TLS handshake.
     * 
     * @throws ConnectionException or InvalidRequestException on failure.
     */
    void connect();

    /**
     * @brief Gracefully tears down TLS and closes the underlying socket.
     */
    void close_connection();

    /**
     * @brief Performs a non-blocking read from the connection.
     * 
     * @param buf Pointer to the destination buffer.
     * @param len Maximum number of bytes to read.
     * @return ssize_t Bytes read, 0 if no data is currently available, throws on error.
     */
    ssize_t read(void *buf, size_t len);

    /**
     * @brief Performs a non-blocking write to the connection.
     * 
     * @param buf Pointer to the source data buffer.
     * @param len Number of bytes to write.
     * @return ssize_t Bytes written, 0 if socket isn't ready, throws on error.
     */
    ssize_t write(const void *buf, size_t len);

    /**
     * @brief Checks for pending bytes inside the TLS/SSL internal buffers.
     * 
     * @return ssize_t Number of pending bytes (always 0 for raw TCP).
     */
    ssize_t pending();

    /**
     * @brief Calculates remaining time before the connection times out based on inactivity.
     * 
     * @param timeout_ms Total allowed inactivity time in milliseconds.
     * @return long long Remaining milliseconds, or 0 if timeout exceeded.
     */
    long long get_ms_until_timeout(int timeout_ms);

    /**
     * @brief Retrieves the raw underlying file descriptor for use in poll()/select().
     * 
     * @return int The socket file descriptor.
     */
    int get_sockfd() const { return sockfd; }

private:
    // --- Connection Coordinates ---
    std::string scheme;       ///< Protocol scheme ("http" or "https")
    std::string host;         ///< Target hostname or IP
    std::string port;         ///< Target port
    int family_pref;          ///< Address family preference (e.g., AF_INET)
    bool use_tls;             ///< True if TLS/SSL encryption is active

    // --- OS Network Socket Members ---
    int sockfd;                           ///< Underlying OS socket file descriptor
    struct addrinfo *list_of_connections; ///< Linked list of resolved addresses
    struct addrinfo *current_connection;  ///< Pointer to the currently active address structure

    // --- OpenSSL Context Members ---
    SSL *ssl;                 ///< Active SSL connection object
    SSL_CTX *ssl_ctx;         ///< Global SSL context for this connection

    // --- Internal State Tracking ---
    ConnectionState state;    ///< Current state of the connection lifecycle
    bool ever_connected;      ///< Flag indicating if a connection was ever successfully established
    std::chrono::steady_clock::time_point last_activity; ///< Timestamp of the last read/write operation

    /**
     * @brief Helper function to format raw IP buffers into human-readable strings.
     * 
     * @param ip_buf The raw IP character buffer.
     * @return std::string The formatted string representation.
     */
    std::string get_ip_string(char* ip_buf);
};