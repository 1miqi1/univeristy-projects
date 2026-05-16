#pragma once

#include <variant>
#include <string>
#include <vector>
#include "args.hpp"
#include "protocol.hpp"
#include "connection.hpp"

#define MAX_BUFFER_SIZE 65536
#define INPUT_BUFFER_SIZE 64


enum class RadioState {
    NOT_CONNECTED,
    SENDING_HTTP,
    REACIVING_HTTP,
    STREAMING_AUDIO,
    SHUTDOWN,
};


struct SendingData {
    size_t bytes_sent = 0;
    size_t request_len;
};

struct HttpParsingData {
    int current_line = 0;
    HttpResponse response;
    std::string input_buffer;
};

enum class StreamState {
    READING_AUDIO,
    READING_META_LENGTH,  
    READING_META_PAYLOAD  
};

struct AudioStreamData {
    long long icy_metaint = 0;
    long long bytes_until_metadata = 0;
    std::string meta_buffer;
    
    // State machine additions
    StreamState state = StreamState::READING_AUDIO;
    size_t meta_bytes_remaining = 0; 
    size_t bytes_to_process = 0;
    char audio_data_buffer[MAX_BUFFER_SIZE];
};

struct Redirecting {
    std::string scheme;
    std::string host;
    std::string path;
    std::string cookie;
};

class RadioClient {
public:
    RadioClient(bool multiplex,
                int timeout_ms,
                int family_pref,
                std::string scheme,
                std::string host,
                std::string path,
                uint16_t port)
        : state(RadioState::NOT_CONNECTED),
          connection(),
          socket_fd(-1),
          multiplex(multiplex),
          timeout_ms(timeout_ms),
          family_pref(family_pref),
          scheme(std::move(scheme)),
          host(std::move(host)),
          path(std::move(path)),
          port(port)
    {}

    void run();

private:
    RadioState state;
    Connection connection;
    int socket_fd;
    
    bool multiplex;
    int timeout_ms;
    int family_pref;

    std::string scheme;
    std::string host;
    std::string path;
    std::string cookie = {};
    std::uint16_t port;

    char network_data_buffer[MAX_BUFFER_SIZE];

    std::string user_input_buffer = "";

    AudioStreamData stream_data;
    std::variant<std::monostate, SendingData, HttpParsingData> state_data;



    void handle_sending_audio_data();

    void handle_sending_http_data();

    void process_stream_data();

    void handle_reading();

    void handle_http(HttpResponse& response);

    bool handle_user_input();
};