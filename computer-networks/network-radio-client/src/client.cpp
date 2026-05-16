#include "client.hpp"
#include "logger.hpp"
#include "connection.hpp"
#include "io.hpp"
#include "protocol.hpp"
#include "exceptions.hpp" // Added our new exceptions header

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

#define USER_INPUT_MAX  4056

void RadioClient::handle_sending_http_data() {
    SendingData* info = std::get_if<SendingData>(&this->state_data); 
    ssize_t len = this->connection.write(this->network_data_buffer + info->bytes_sent, info->request_len - info->bytes_sent);
    
    if (len > 0) {
        info->bytes_sent += len;

        if (info->bytes_sent == info->request_len) {
            this->state = RadioState::REACIVING_HTTP; 
            HttpResponse response;
            init_http_response(response);
            this->state_data = HttpParsingData({0, response, ""});

        }
    }
}

void RadioClient::handle_reading() {
    // No try-catch here. Allow exceptions to bubble up to run()
    ssize_t bytes_read = connection.read(network_data_buffer, MAX_BUFFER_SIZE);
    
    if (bytes_read <= 0) return; 

    if (state == RadioState::REACIVING_HTTP) {
        auto* http_data = std::get_if<HttpParsingData>(&state_data);
        if (!http_data) return; 
        
        http_data->input_buffer.append(network_data_buffer, bytes_read);
        size_t pos;

        while ((pos =  http_data->input_buffer.find("\r\n")) != std::string::npos) {
            std::string line =  http_data->input_buffer.substr(0, pos);
            http_data->input_buffer.erase(0, pos + 2); 
            
            // Log incoming HTTP response lines
            LOGI("%s", line.c_str());
            
            if (line.empty()) {
                HttpResponse response = http_data->response;
                handle_http(response);
                if(state == RadioState::STREAMING_AUDIO){
                    size_t leftover_bytes =  http_data->input_buffer.size();
                    size_t offset = 0;
                    
                    // Feed the leftover bytes in safe chunks so we don't overflow the buffer!
                    while (offset < leftover_bytes) {
                        // Assuming audio_data_buffer is sized to MAX_BUFFER_SIZE
                        size_t chunk = std::min(leftover_bytes - offset, (size_t)MAX_BUFFER_SIZE);
                        
                        memcpy(stream_data.audio_data_buffer,  http_data->input_buffer.data() + offset, chunk);
                        stream_data.bytes_to_process = chunk;
                        
                        process_stream_data();
                        
                        offset += chunk;
                    }
                }
                http_data->input_buffer.clear();
                return;
            }
            parse_http_response_line(line, http_data->current_line, http_data->response);
            http_data->current_line++;
        }
    } 
    // Handle Audio and ICY Metadata Parsing
    else if (state == RadioState::STREAMING_AUDIO) {
        memcpy(stream_data.audio_data_buffer, network_data_buffer, bytes_read);
        stream_data.bytes_to_process = bytes_read;
        process_stream_data();
    }
}

void RadioClient::handle_http(HttpResponse& response) {
    // 1. Success (200 OK)
    if (response.status_code == 200) {
        this->state = RadioState::STREAMING_AUDIO;
        
        stream_data.state = StreamState::READING_AUDIO;
        stream_data.bytes_to_process = 0;
        stream_data.meta_bytes_remaining = 0;
        
        // Directly map the parsed integer from your struct
        stream_data.icy_metaint = response.icy_metaint;
        stream_data.bytes_until_metadata = response.icy_metaint;

        // Lock in the new state data
        this->state_data = std::monostate{};
        return;
    }

    // 2. Redirection (3xx)
    if (response.status_code >= 300 && response.status_code < 400) {
        if (response.location.empty()) {
            LOGE("Received HTTP %d redirect but no 'Location' provided.", response.status_code);
            this->state = RadioState::SHUTDOWN;
            return;
        }

        // Store the cookie if the server gave us one
        if (!response.cookie.empty()) {
            this->cookie = response.cookie;
        }

        // TODO: You will need a URL parser here to extract the new 
        // scheme, host, path, and port from `response.location`.
        parse_url(response.location, this->scheme, this->host, this->path, this->port);

        // Close the current socket so the run() loop establishes a new connection
        this->connection.close_connection();

        // Reset state so the client reconnects to the new host
        this->state = RadioState::NOT_CONNECTED;
        this->state_data = std::monostate{};
        return;
    }

    // 3. Client or Server Errors (4xx / 5xx)
    if (response.status_code >= 400) {
        this->connection.close_connection();
        LOGE("Received HTTP Error %d", response.status_code);
        throw HttpException(response.status_code, "Received HTTP Error " + std::to_string(response.status_code));
    }
}

bool RadioClient::handle_user_input() {
    ssize_t n = read(STDIN_FILENO, network_data_buffer, sizeof(network_data_buffer));

    if (n > 0) {
        user_input_buffer.append(network_data_buffer, n);

        size_t pos;
        while ((pos = user_input_buffer.find('\n')) != std::string::npos) {
            std::string line = user_input_buffer.substr(0, pos);
            user_input_buffer.erase(0, pos + 1);

            if (line == "quit") {
                LOGI("Quit command received. Shutting down...");
                connection.close_connection();
                return true;
            }
        }
        if(user_input_buffer.size() > 4){
            user_input_buffer = "";
        }
    }
    else if (n == 0) {
        LOGD("stdin EOF");
    }
    else {
        if (errno != EINTR) {
            int err = errno;
            LOGE("stdin read error: %s", strerror(err));
            throw NetworkError(err, "stdin read error");
        }
    }
    return false;
}

void RadioClient::process_stream_data() {
    // Ensure we get a reference to the state, not a copy!

    if (stream_data.bytes_to_process == 0) return;

    // If there is no metadata interval, treat the entire buffer as audio
    if (stream_data.icy_metaint <= 0) {
        handle_audio(stream_data.audio_data_buffer, stream_data.bytes_to_process);
        stream_data.bytes_to_process = 0; // Buffer consumed
        return;
    }

    size_t offset = 0;

    // Loop until we have processed all the bytes available in the current buffer
    while (offset < stream_data.bytes_to_process) {
        size_t available = stream_data.bytes_to_process - offset;

        switch (stream_data.state) {
            
            case StreamState::READING_AUDIO: {
                size_t audio_chunk = std::min(static_cast<size_t>(stream_data.bytes_until_metadata), available);
                
                handle_audio(stream_data.audio_data_buffer + offset, audio_chunk);
                
                offset += audio_chunk;
                stream_data.bytes_until_metadata -= audio_chunk;

                if (stream_data.bytes_until_metadata == 0) {
                    stream_data.state = StreamState::READING_META_LENGTH;
                }
                break;
            }

            case StreamState::READING_META_LENGTH: {
                uint8_t length_byte = stream_data.audio_data_buffer[offset];
                offset += 1;
                
                size_t payload_len = static_cast<size_t>(length_byte) * 16;
                
                if (payload_len == 0) {
                    stream_data.state = StreamState::READING_AUDIO;
                    stream_data.bytes_until_metadata = stream_data.icy_metaint;
                } else {
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
                    handle_metadata(stream_data.meta_buffer);
                    stream_data.meta_buffer.clear();
                    stream_data.state = StreamState::READING_AUDIO;
                    stream_data.bytes_until_metadata = stream_data.icy_metaint;
                }
                break;
            }
        }
    }
    
    // Reset to 0 since we have successfully processed all bytes in this chunk
    stream_data.bytes_to_process = 0;
}

void RadioClient::run() {
    this->state = RadioState::NOT_CONNECTED;
    struct pollfd pfds[2];

    pfds[0].fd = STDIN_FILENO;
 
    while (true) { 
        try {
            // 1. Setup / Re-setup Connection
            if (this->state == RadioState::NOT_CONNECTED || this->connection.get_ms_until_timeout(this->timeout_ms) == 0 ) {
                this->connection.resolve_name(this->scheme, this->host, std::to_string(this->port), this->family_pref);
                this->connection.connect();
                this->state = RadioState::SENDING_HTTP;
                size_t message_len = create_http_request(this->network_data_buffer, this->host, this->path, this->multiplex, this->cookie);
                
                // Parse the request buffer into lines so LOGI prints it properly formatted
                std::string req_str(this->network_data_buffer, message_len);
                size_t req_pos = 0, next_pos;
                while ((next_pos = req_str.find("\r\n", req_pos)) != std::string::npos) {
                    LOGI("%s", req_str.substr(req_pos, next_pos - req_pos).c_str());
                    req_pos = next_pos + 2;
                }
                LOGI(""); // Add one extra newline spacer manually to match the layout
                
                this->state_data = SendingData{0, message_len};
                pfds[1].fd = this->connection.get_sockfd();
            }

            // 2. Poll Configuration
            pfds[0].events = POLLIN;
            if (this->state == RadioState::SENDING_HTTP) {
                pfds[1].events = POLLIN | POLLOUT; 
            } else {
                pfds[1].events = POLLIN;
            }

            int ret = poll(pfds, 2, this->connection.get_ms_until_timeout(this->timeout_ms));

            if (ret == 0) {
                if (this->state != RadioState::NOT_CONNECTED) {
                    LOGI("Connection timed out. Closing socket to reconnect...");
                    this->connection.close_connection();
                    this->state = RadioState::NOT_CONNECTED; 
                }
                continue;

            } else if (ret < 0) {
                if (errno == EINTR) continue;
                int err = errno;
                LOGE("poll error: %s", strerror(err));
                throw NetworkError(err, "poll error");
            }

            // 3. User Input
            if ((pfds[0].revents & POLLIN) && handle_user_input()) {
                return; 
            }

            // 4. Network IO
            if (this->state != RadioState::SHUTDOWN && pfds[1].revents & (POLLIN | POLLOUT | POLLHUP | POLLERR)) {
                
                if (pfds[1].revents & POLLERR) {
                    LOGE("Network socket error (POLLERR)");
                    throw ConnectionException("Network socket error (POLLERR)");
                }

                if (this->state == RadioState::SENDING_HTTP && (pfds[1].revents & POLLOUT)) {
                    this->handle_sending_http_data(); 
                } 
                else if (pfds[1].revents & POLLIN) {
                    this->handle_reading();
                    while(this->connection.pending() > 0){
                        this->handle_reading();
                    }
                }

                if (pfds[1].revents & POLLHUP) {
                    throw ServerDisconnectedException();
                }
            }

        } 
        catch (const ServerDisconnectedException&) {
            LOGI("Server disconnected gracefully.");
            process_stream_data();
            this->connection.close_connection();
            return; 
        } 
        catch (const std::exception& e) {
            LOGE("Fatal Error: %s", e.what());
            this->connection.close_connection();
            exit(1); 
        } 
        catch (...) {
            LOGE("Unknown fatal exception occurred.");
            this->connection.close_connection();
            exit(1);
        }
    }
}