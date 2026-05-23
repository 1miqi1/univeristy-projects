#pragma once

#include "args.hpp"
#include "http.hpp"
#include "connection.hpp"

#include <variant>
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Maximum size for internal buffers used for network and user input I/O.
 */
constexpr size_t MAX_BUFFER_SIZE = 65536;

/**
 * @brief Represents the high-level connection state of the radio client.
 */
enum class RadioState {
    NOT_CONNECTED,
    SENDING_HTTP,
    RECEIVING_HTTP,
    STREAMING_AUDIO,
    SHUTDOWN,
};

/**
 * @brief Tracks the progress of sending an HTTP request.
 */
struct SendingData {
    size_t bytes_sent = 0;   ///< Number of bytes successfully sent so far
    size_t request_len = 0;  ///< Total size of the HTTP request
};

/**
 * @brief Tracks the state while parsing an incoming HTTP response.
 */
struct HttpParsingData {
    int current_line = 0;      ///< Current header line index (0 = status line)
    HttpResponse response;     ///< Parsed HTTP response data
    std::string input_buffer;  ///< Buffer accumulating incomplete header lines
};

/**
 * @brief Defines the states for the ICY audio/metadata multiplexing stream.
 */
enum class StreamState {
    READING_AUDIO,         ///< Currently reading raw audio data
    READING_META_LENGTH,   ///< Reading the 1-byte metadata length indicator
    READING_META_PAYLOAD   ///< Reading the actual metadata string payload
};

/**
 * @brief Holds all data related to the active audio/metadata stream.
 */
struct AudioStreamData {
    long long icy_metaint = 0;          ///< Interval in bytes between metadata blocks
    long long bytes_until_metadata = 0; ///< Countdown of audio bytes until next metadata
    std::string meta_buffer;            ///< Accumulates metadata payload
    
    StreamState state = StreamState::READING_AUDIO;
    size_t meta_bytes_remaining = 0;    ///< Remaining metadata bytes to read in current block
    size_t bytes_to_process = 0;        ///< Valid bytes currently sitting in audio_data_buffer
    
    char audio_data_buffer[MAX_BUFFER_SIZE];
};

/**
 * @brief Helper structure containing parsed redirect coordinates.
 */
struct Redirecting {
    std::string scheme;     ///< Protocol scheme for the redirect ("http" or "https")
    std::string host;       ///< Target server hostname or IP address for the redirect
    std::string path;       ///< URL path and query string for the redirect
    std::string cookie;     ///< Session cookie to pass along with the redirect request
};

/**
 * @brief Helper structure user input 
 */
struct UserInput {
    char user_input_buffer[MAX_BUFFER_SIZE]; ///< Buffer to store raw characters read from standard input
    size_t buffer_size = 0;                  ///< Current number of valid bytes in the user input buffer
};

/**
 * @brief Core client class responsible for network I/O, state management, and stream parsing.
 */
class RadioClient {
public:
    /**
     * @brief Constructs a new RadioClient.
     * 
     * @param multiplex   True to request ICY metadata multiplexed with the audio stream.
     * @param timeout_ms  Network timeout duration in milliseconds for operations.
     * @param family_pref IP family preference (e.g., AF_INET for IPv4, AF_INET6 for IPv6, or AF_UNSPEC).
     * @param scheme      The network protocol scheme (e.g., "http", "https").
     * @param host        The target hostname or IP address of the radio server.
     * @param path        The URL path including query parameters (e.g., "/stream").
     * @param port        The network port to connect to (e.g., 80, 443, 8000).
     * @param url         The original full URL string provided for the connection.
     */
    RadioClient(bool multiplex,
            int timeout_ms,
            int family_pref,
            std::string scheme,
            std::string host,
            std::string path,
            uint16_t port,
            std::string url)
    : state(RadioState::NOT_CONNECTED),
        connection(),
        socket_fd(-1),
        multiplex(multiplex),
        timeout_ms(timeout_ms),
        family_pref(family_pref),
        scheme(std::move(scheme)),
        host(std::move(host)),
        path(std::move(path)),
        port(port),
        url(std::move(url)) 
    {}

    /**
     * @brief Starts the main application event loop.
     */
    void run();

private:
    RadioState state;           ///< The current high-level phase of the client (e.g., SENDING_HTTP, STREAMING_AUDIO)
    Connection connection;      ///< The wrapper object managing the underlying TCP/TLS socket connection
    int socket_fd;              ///< Cached file descriptor of the active socket, used primarily for poll()

    // --- Configuration & Options ---
    bool multiplex;             ///< If true, requests ICY metadata to be interleaved with the audio stream
    int timeout_ms;             ///< Network operation timeout limit in milliseconds
    int family_pref;            ///< Preferred IP address family (e.g., AF_INET for IPv4, AF_UNSPEC for any)

    // --- Target URL Coordinates ---
    std::string scheme;         ///< Protocol scheme ("http" or "https")
    std::string host;           ///< Target server hostname or IP address
    std::string path;           ///< URL path and query string (e.g., "/stream")
    std::vector<HttpCookie> client_cookies;    ///< session cookie to send with the HTTP request
    std::uint16_t port;         ///< Target network port (e.g., 80, 443, 8000)

    // --- Source URL ---
    std::string url;            ///< The original URL string provided to the client for the connection

    // --- I/O Buffers ---
    char network_data_buffer[MAX_BUFFER_SIZE]; ///< Primary buffer for raw bytes read from or written to the network
    UserInput user_input_data;                 ///< Buffer accumulating characters typed by the user via stdin

    // --- Streaming & Sub-States ---
    AudioStreamData stream_data; ///< State machine and buffers specifically for demultiplexing audio and ICY metadata
    std::variant<std::monostate, SendingData, HttpParsingData> state_data; ///< Data for different states

    /**
     * @brief Manages the non-blocking transmission of outbound audio data (if applicable).
     */
    void handle_sending_audio_data();

    /**
     * @brief Handles the non-blocking transmission of the initial HTTP GET request.
     *        Transitions the client to the RECEIVING_HTTP state once the entire 
     *        request has been flushed to the network socket.
     */
    void handle_sending_http_data();

    /**
     * @brief The core stream demultiplexer. 
     *        Reads from the internal buffer and uses a state machine to separate 
     *        raw audio bytes from ICY metadata blocks. Dispatches audio to stdout 
     *        and metadata to stderr.
     */
    void process_stream_data();

    /**
     * @brief Reads raw bytes from the network socket into the internal buffer.
     *        Routes the received data either to the HTTP header parser (if connecting)
     *        or to the audio stream processor (if streaming).
     * @return number of bytes read
     */
    ssize_t handle_reading(size_t len);

    /**
     * @brief Processes a completely parsed HTTP response.
     *        Handles 200 OK (initiating stream), 3xx Redirects (parsing the new URL 
     *        and triggering a reconnect), and throws on 4xx/5xx errors.
     * 
     * @param response The parsed HTTP response object containing status codes and headers.
     */
    void handle_http(HttpResponse& response);

    /**
     * @brief Non-blocking read and execution of commands from standard input (stdin).
     *        Currently supports the "quit" command for graceful shutdown.
     * 
     * @return true if the client should gracefully shut down; false to continue running.
     */
    int handle_user_input();
};
