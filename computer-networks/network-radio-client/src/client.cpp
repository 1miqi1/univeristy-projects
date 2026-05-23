
#include "client.hpp"
#include "logger.hpp"
#include "connection.hpp"
#include "io.hpp"
#include "http.hpp"
#include "exceptions.hpp"

#include <iostream>
#include <string>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <poll.h>
#include <unistd.h>
#include <stdexcept>
#include <string_view>
#include <algorithm>
#include <fcntl.h>
#include <vector>

namespace {
    // Protocol Constants
    constexpr std::string_view CRLF = "\r\n";
    constexpr std::string_view CMD_QUIT = "quit\n";
    constexpr size_t ICY_METADATA_MULTIPLIER = 16; // ICY protocol dictates len byte * 16

    // HTTP Status Boundaries
    constexpr int HTTP_STATUS_OK = 200;
    constexpr int HTTP_STATUS_REDIRECT_MIN = 300;
    constexpr int HTTP_STATUS_ERROR_MIN = 400;

    // Poll structure indices
    constexpr int POLL_STDIN_IDX = 0;
    constexpr int POLL_NET_IDX = 1;
    constexpr int NUM_POLL_FDS = 2;
    constexpr int MAX_USER_INPUT = 4096;
}

void RadioClient::handle_sending_http_data() {
    auto* info = std::get_if<SendingData>(&state_data); 
    if (!info) return;

    ssize_t len = connection.write(network_data_buffer + info->bytes_sent, info->request_len - info->bytes_sent);
    
    if (len > 0) {
        info->bytes_sent += static_cast<size_t>(len);
        LOGD("Sent %zd bytes of HTTP request (%zu/%zu total)", len, info->bytes_sent, info->request_len);

        if (info->bytes_sent == info->request_len) {
            LOGD("Finished sending HTTP request. Transitioning to RECEIVING_HTTP.");
            state = RadioState::RECEIVING_HTTP; 
            
            HttpResponse response;
            init_http_response(response);
            state_data = HttpParsingData{0, response, ""};
        }
    }
}

ssize_t RadioClient::handle_reading(size_t len) {
    size_t to_read = (len > 0) ? std::min(len, MAX_BUFFER_SIZE) : MAX_BUFFER_SIZE;
    ssize_t bytes_read = connection.read(network_data_buffer, to_read);

    if (bytes_read <= 0) return bytes_read; 

    if (state == RadioState::RECEIVING_HTTP) {
        LOGD("Read %zd bytes of HTTP response data.", bytes_read);
        auto* http_data = std::get_if<HttpParsingData>(&state_data);
        
        http_data->input_buffer.append(network_data_buffer, bytes_read);
        size_t pos;

        // Upgraded to handle both \r\n and just \n
        while ((pos = http_data->input_buffer.find("\n")) != std::string::npos) {
            size_t line_len = pos;
            if (pos > 0 && http_data->input_buffer[pos - 1] == '\r') {
                line_len--; // Ignore the \r if it exists
            }

            std::string line = http_data->input_buffer.substr(0, line_len);
            http_data->input_buffer.erase(0, pos + 1); 
            
            LOGI("%s", line.c_str()); // Keep INFO for printing headers
            
            if (line.empty()) {
                LOGD("End of HTTP headers detected.");
                handle_http(http_data->response);
                return bytes_read;
            }
            
            parse_http_response_line(line, http_data->current_line, http_data->response);
            http_data->current_line++;
        }
    } 
    else if (state == RadioState::STREAMING_AUDIO) {
        // Omitting LOGD here so we don't flood the terminal 50x a second
        memcpy(stream_data.audio_data_buffer, network_data_buffer, bytes_read);
        stream_data.bytes_to_process = static_cast<size_t>(bytes_read);
        process_stream_data();
    }

    return bytes_read;
}

void RadioClient::handle_http(HttpResponse& response) {
    validate_http_response(response);
    LOGD("Validating HTTP response. Status Code: %d", response.status_code);

    // 1. Success (200 OK)
    if (response.status_code == HTTP_STATUS_OK) {
        LOGD("HTTP 200 OK. Setting up audio stream. ICY MetaInt: %d", response.icy_metaint);
        state = RadioState::STREAMING_AUDIO;
        stream_data.state = StreamState::READING_AUDIO;
        stream_data.bytes_to_process = 0;
        stream_data.meta_bytes_remaining = 0;
        
        stream_data.icy_metaint = response.icy_metaint;
        stream_data.bytes_until_metadata = response.icy_metaint;

        auto* http_data = std::get_if<HttpParsingData>(&state_data);
        size_t leftover_bytes = http_data->input_buffer.size();
        
        if (leftover_bytes > 0) {
            LOGD("Processing %zu leftover bytes from HTTP body as initial audio.", leftover_bytes);
        }

        size_t offset = 0;
        while (offset < leftover_bytes) {
            size_t chunk = std::min(leftover_bytes - offset, MAX_BUFFER_SIZE);
            memcpy(stream_data.audio_data_buffer, http_data->input_buffer.data() + offset, chunk);
            
            stream_data.bytes_to_process = chunk;
            process_stream_data();
            
            offset += chunk;
        }

        state_data = std::monostate{};
        return;
    }

    // 2. Redirection (3xx)
    if (response.status_code >= HTTP_STATUS_REDIRECT_MIN && response.status_code < HTTP_STATUS_ERROR_MIN) {
        if (response.location.empty()) {
            LOGE("Received HTTP %d redirect but no 'Location' provided.", response.status_code);
            state = RadioState::SHUTDOWN;
            return;
        }

        LOGD("HTTP Redirecting to: %s", response.location.c_str());

        for (const auto& cookie_header : response.set_cookies) {
            merge_cookie(client_cookies, cookie_header, host);
        }

        parse_url(response.location, scheme, host, path, port);
        connection.close_connection();

        state = RadioState::NOT_CONNECTED;
        state_data = std::monostate{};
        return;
    }

    // 3. Client or Server Errors (4xx / 5xx)
    if (response.status_code >= HTTP_STATUS_ERROR_MIN) {
        connection.close_connection();
        LOGE("Received HTTP Error %d", response.status_code);
        throw HttpException(response.status_code, "Received HTTP Error " + std::to_string(response.status_code));
    }
}

int RadioClient::handle_user_input() {
    size_t available_space = sizeof(user_input_data.user_input_buffer) - user_input_data.buffer_size;
    ssize_t n = read(STDIN_FILENO, user_input_data.user_input_buffer + user_input_data.buffer_size, available_space);

    if (n > 0) {
        LOGD("Read %zd bytes from user input (stdin).", n);
        user_input_data.buffer_size += n; 

        std::string_view view(user_input_data.user_input_buffer, user_input_data.buffer_size);
        if (view.find(CMD_QUIT) != std::string_view::npos) {
            LOGD("Quit command matched. Shutting down.");
            connection.close_connection();
            return 1; // 1 indicates the user wants to quit
        }

        if (user_input_data.buffer_size > CMD_QUIT.size()) {
            char temp[CMD_QUIT.size()];
            size_t start_idx = user_input_data.buffer_size - CMD_QUIT.size();
            
            for (size_t i = 0; i < CMD_QUIT.size(); ++i) temp[i] = user_input_data.user_input_buffer[start_idx + i];
            for (size_t i = 0; i < CMD_QUIT.size(); ++i) user_input_data.user_input_buffer[i] = temp[i];
            
            user_input_data.buffer_size = CMD_QUIT.size();
        }
    }
    else if (n == 0) {
        LOGD("stdin EOF - Disabling STDIN polling");
        return -1; // -1 indicates EOF
    }
    else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
        int err = errno;
        LOGW("stdin read error: %s", strerror(err));
    }
    
    return 0; // 0 indicates normal operation
}

void RadioClient::process_stream_data() {
    LOGD("processing %zd bytes of HTTP response data.", stream_data.bytes_to_process);
    if (stream_data.bytes_to_process == 0) return;

    if (stream_data.icy_metaint <= 0) {
        LOGD("Audio %s bytes of HTTP response data.", stream_data.audio_data_buffer);
        handle_audio(stream_data.audio_data_buffer, stream_data.bytes_to_process);
        stream_data.bytes_to_process = 0;
        return;
    }

    size_t offset = 0;

    while (offset < stream_data.bytes_to_process) {
        size_t available = stream_data.bytes_to_process - offset;

        switch (stream_data.state) {
            case StreamState::READING_AUDIO: {
                LOGD("Audio %zd bytes of HTTP response data.", stream_data.bytes_to_process);
                size_t audio_chunk = std::min(static_cast<size_t>(stream_data.bytes_until_metadata), available);
                handle_audio(stream_data.audio_data_buffer + offset, audio_chunk);
                
                offset += audio_chunk;
                stream_data.bytes_until_metadata -= audio_chunk;

                if (stream_data.bytes_until_metadata == 0) {
                    LOGD("Reached end of audio chunk. Awaiting metadata length byte.");
                    stream_data.state = StreamState::READING_META_LENGTH;
                }
                break;
            }

            case StreamState::READING_META_LENGTH: {
                uint8_t length_byte = stream_data.audio_data_buffer[offset];
                offset += 1;
                
                size_t payload_len = static_cast<size_t>(length_byte) * ICY_METADATA_MULTIPLIER;
                
                if (payload_len == 0) {
                    stream_data.state = StreamState::READING_AUDIO;
                    stream_data.bytes_until_metadata = stream_data.icy_metaint;
                } else {
                    LOGD("Metadata length byte %d -> payload is %zu bytes.", length_byte, payload_len);
                    stream_data.meta_bytes_remaining = payload_len;
                    stream_data.state = StreamState::READING_META_PAYLOAD;
                }
                break;
            }

            case StreamState::READING_META_PAYLOAD: {
                size_t meta_chunk = std::min(stream_data.meta_bytes_remaining, available);
                stream_data.meta_buffer.append(
                    reinterpret_cast<const char*>(stream_data.audio_data_buffer + offset), 
                    meta_chunk
                );
                
                stream_data.meta_bytes_remaining -= meta_chunk;
                offset += meta_chunk;

                if (stream_data.meta_bytes_remaining == 0) {
                    LOGD("Metadata block fully received. Returning to READING_AUDIO.");
                    while (!stream_data.meta_buffer.empty() && stream_data.meta_buffer.back() == '\0') {
                        stream_data.meta_buffer.pop_back();
                    }
                    stream_data.meta_buffer.append("\n");
                    stream_data.state = StreamState::READING_AUDIO;
                    stream_data.bytes_until_metadata = stream_data.icy_metaint;
                    
                    handle_metadata(stream_data.meta_buffer);
                    stream_data.meta_buffer.clear();
                }
                break;
            }
        }
    }
    
    stream_data.bytes_to_process = 0;
}

void RadioClient::run() {
    LOGD("Initializing client run loop.");
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) {
        LOGW("Getting stdin flags failed: %s", strerror(errno));
    } else {
        if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
            LOGW("Setting stdin to non-blocking failed: %s", strerror(errno));
        }
    }

    state = RadioState::NOT_CONNECTED;
    struct pollfd pfds[NUM_POLL_FDS];
    pfds[POLL_STDIN_IDX].fd = STDIN_FILENO;
 
    while (true) { 
        try {
            // 1. Setup / Re-setup Connection
            if (state == RadioState::NOT_CONNECTED) {
                LOGD("State: NOT_CONNECTED. Resolving host and connecting to: %s:%d", host.c_str(), port);
                connection.resolve_name(scheme, host, std::to_string(port), family_pref);
                connection.connect();
                state = RadioState::SENDING_HTTP;
                LOGD("Connection established. Formatting HTTP request...");
                
                size_t message_len = create_http_request(network_data_buffer, host, path, multiplex, client_cookies);
                
                std::string req_str(network_data_buffer, message_len);
                size_t req_pos = 0, next_pos;
                while ((next_pos = req_str.find(CRLF, req_pos)) != std::string::npos) {
                    LOGI("%s", req_str.substr(req_pos, next_pos - req_pos).c_str());
                    req_pos = next_pos + CRLF.size();
                }
                LOGI(""); // Extra newline spacer
                
                state_data = SendingData{0, message_len};
                pfds[POLL_NET_IDX].fd = connection.get_sockfd();
            }

            // 2. Poll Configuration
            if(pfds[POLL_STDIN_IDX].fd != -1) {
                pfds[POLL_STDIN_IDX].events = POLLIN;
            }
            if (state == RadioState::SENDING_HTTP) {
                pfds[POLL_NET_IDX].events = POLLIN | POLLOUT; 
            } else {
                pfds[POLL_NET_IDX].events = POLLIN;
            }

            int ret = poll(pfds, NUM_POLL_FDS, connection.get_ms_until_timeout(timeout_ms));

            if (connection.get_ms_until_timeout(timeout_ms) == 0 ) {
                LOGI("data receiving timeout.");
                connection.close_connection();

                state = RadioState::NOT_CONNECTED;
                client_cookies.clear();
                scheme.clear(); host.clear(); path.clear(); port = 0;
                parse_url(url, scheme, host, path, port);

                state_data = std::monostate{};
                user_input_data.buffer_size = 0;
            } else if (ret < 0) {
                if (errno == EINTR) continue;
                int err = errno;
                LOGE("poll error: %s", strerror(err));
                throw NetworkError(err, "poll error");
            }

            // 3. User Input
            if (pfds[POLL_STDIN_IDX].fd != -1 && (pfds[POLL_STDIN_IDX].revents & POLLIN)) {
                int input_status = handle_user_input();
                if (input_status == 1) {
                    return; 
                } else if (input_status == -1) {
                    pfds[POLL_STDIN_IDX].fd = -1; 
                }
            }

            // 4. Network IO
            if (state != RadioState::SHUTDOWN && (pfds[POLL_NET_IDX].revents & (POLLIN | POLLOUT | POLLHUP | POLLERR))) {
                
                uint16_t re = pfds[POLL_NET_IDX].revents;

                if (re & POLLERR) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    getsockopt(connection.get_sockfd(), SOL_SOCKET, SO_ERROR, &err, &len);

                    if(err == ECONNRESET){
                        LOGD("ECONNRESET detected. Throwing disconnected exception.");
                        throw ServerDisconnectedException();
                    } else {
                        LOGE("Fatal socket error: %s", strerror(err));
                        throw NetworkError(err, "socket error");
                    }
                }

                if (pfds[POLL_NET_IDX].revents & POLLHUP) {
                    LOGD("POLLHUP detected. Server hung up.");
                    throw ServerDisconnectedException();
                }

                if (state == RadioState::SENDING_HTTP && (pfds[POLL_NET_IDX].revents & POLLOUT)) {
                    handle_sending_http_data(); 
                } 
                else if (pfds[POLL_NET_IDX].revents & POLLIN) {
                    handle_reading(MAX_BUFFER_SIZE);
                }
            }
        } 
        catch (const ServerDisconnectedException&) {
            LOGI("Server disconnected gracefully.");
            process_stream_data();
            connection.close_connection();
            return; 
        } 
        catch (const std::exception& e) {
            LOGE("Fatal Error: %s", e.what());
            connection.close_connection();
            exit(EXIT_FAILURE); 
        } 
        catch (...) {
            LOGE("Unknown fatal exception occurred.");
            connection.close_connection();
            exit(EXIT_FAILURE);
        }
    }
}

