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
    READING_HTTP,
    SERVER_CLOSING,
    STREAMING_AUDIO,
    USER_QUIT,
};

enum class RadioOutput {
    USER_QUIT,
    SERVER_CLOSED,
};

struct SendingData {
    char* request_str;
    size_t bytes_sent = 0;
    size_t request_len;
};

struct HttpParsingData {
    char* current_line;
    HttpResponse response;
};

struct AudioStreamData {
    HttpResponse response;
    long long bytes_until_metadata = 0;
    bool expecting_metadata = false;
};

class RadioClient {
public:
    explicit RadioClient(Options opt)
        : options(std::move(opt)),
          state(RadioState::NOT_CONNECTED),
          connection(),
          state_data(std::monostate{}) {}

    void run();

private:
    Options options;
    RadioState state;
    Connection connection;
    int socket_fd = -1;

    char network_data_buffer[MAX_BUFFER_SIZE];

    std::string input_buffer = "";

    std::variant<std::monostate, SendingData, HttpParsingData, AudioStreamData> state_data;

    ssize_t handle_recv(size_t n);

    ssize_t handle_write(size_t n, size_t start);

    void handle_send_http();

    void handle_read_http();

    void handle_streaming();

    void handle_user_input();
};