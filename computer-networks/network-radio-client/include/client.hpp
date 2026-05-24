#pragma once

#include "args.hpp"
#include "http.hpp"
#include "connection.hpp"

#include <variant>
#include <string>
#include <vector>
#include <cstdint>
#include <poll.h>

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
    READING_AUDIO,       ///< Currently reading raw audio data
    READING_META_LENGTH, ///< Reading the 1-byte metadata length indicator
    READING_META_PAYLOAD ///< Reading the actual metadata string payload
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
 * @brief Helper structure for user input.
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
     * @param multiplex True to request ICY metadata multiplexed with the audio stream.
     * @param timeout_ms Network timeout duration in milliseconds.
     * @param family_pref IP family preference.
     * @param scheme The network protocol scheme.
     * @param host The target hostname or IP address.
     * @param path The URL path including query parameters.
     * @param port The network port to connect to.
     * @param url The original full URL string.
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
    RadioState state;          ///< Current high-level phase of the client.
    Connection connection;     ///< Wrapper for TCP/TLS socket connection.
    int socket_fd;             ///< Cached file descriptor for poll().

    // Configuration & Options
    bool multiplex;            ///< Request ICY metadata interleaving.
    int timeout_ms;            ///< Network operation timeout in milliseconds.
    int family_pref;           ///< Preferred IP address family.

    // Target URL Coordinates
    std::string scheme;        ///< Protocol scheme ("http" or "https").
    std::string host;          ///< Target server hostname or IP.
    std::string path;          ///< URL path and query string.
    std::vector<HttpCookie> client_cookies; ///< Session cookies for HTTP request.
    std::uint16_t port;        ///< Target network port.

    // Source URL
    std::string url;           ///< The original URL string provided.

    // I/O Buffers
    char network_data_buffer[MAX_BUFFER_SIZE]; ///< Primary buffer for network I/O.
    UserInput user_input_data;                ///< Buffer for stdin characters.

    // Streaming & Sub-States
    AudioStreamData stream_data; ///< State and buffers for ICY stream processing.
    std::variant<std::monostate, SendingData, HttpParsingData> state_data; ///< Data for specific client states.

    /**
     * @brief Manages non-blocking transmission of outbound audio data.
     */
    void handle_sending_audio_data();

    /**
     * @brief Manages non-blocking transmission of the initial HTTP GET request.
     */
    void handle_sending_http_data();

    /**
     * @brief Parses and demultiplexes raw bytes into audio and metadata.
     */
    void process_stream_data();

    /**
     * @brief Reads bytes from the network socket into the internal buffer.
     * @param len Maximum bytes to read.
     * @return Number of bytes read.
     */
    ssize_t handle_reading(size_t len);

    /**
     * @brief Processes a parsed HTTP response (e.g., handles redirects or errors).
     * @param response The parsed HTTP response object.
     */
    void handle_http(HttpResponse& response);

    /**
     * @brief Reads commands from standard input.
     * @return 1 to exit, -1 on failure, 0 otherwise.
     */
    int handle_user_input();

    /**
     * @brief Configures standard input for non-blocking mode.
     */
    void init_stdin();

    /**
     * @brief Triggers connection logic if state is NOT_CONNECTED.
     */
    void setup_connection_if_needed(struct pollfd* pfds);

    /**
     * @brief Executes the poll system call to wait for I/O events.
     * @return true if successful, false if interrupted by signal.
     */
    bool execute_poll(struct pollfd* pfds);

    /**
     * @brief Processes input from stdin if available.
     * @return true if exit requested.
     */
    bool process_user_input(struct pollfd* pfds);

    /**
     * @brief Dispatches network events (POLLIN, POLLOUT, etc.) to appropriate handlers.
     */
    void process_network_io(struct pollfd* pfds);

    /**
     * @brief Processes audio data within the stream processing loop.
     */
    void process_audio_state(size_t& offset, size_t available);

    /**
     * @brief Processes the metadata length byte in the stream processing loop.
     */
    void process_meta_length_state(size_t& offset);

    /**
     * @brief Processes metadata payload bytes in the stream processing loop.
     */
    void process_meta_payload_state(size_t& offset, size_t available);
};
