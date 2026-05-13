#include "client.hpp"
#include "logger.hpp"
#include "connection.hpp"
#include "io.hpp"
#include "protocol.hpp"

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
    SendingData info = std::get<SendingData>(this->state_data); 
    ssize_t len = this->connection.write(this->network_data_buffer + info.bytes_sent, info.request_len - info.bytes_sent);
    
    if (len > 0) {
        info.bytes_sent += len;
        this->state_data = info; 

        if (info.bytes_sent == info.request_len) {
            this->state = RadioState::REACIVING_HTTP; 
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
        
        input_buffer.append(network_data_buffer, bytes_read);
        static int line_count = 0; 
        size_t pos;

        while ((pos = input_buffer.find("\r\n")) != std::string::npos) {
            std::string line = input_buffer.substr(0, pos);
            input_buffer.erase(0, pos + 2); 
            
            if (line.empty()) {
                HttpResponse response = http_data->response;
                handle_http(response);
                if(state == RadioState::STREAMING_AUDIO){
                    size_t leftover_bytes = input_buffer.size();
                    memcpy(audio_data_buffer + audio_buffer_len, input_buffer.data(), leftover_bytes);
                    auto* data = std::get_if<AudioStreamData>(&state_data);
                    data->bytes_to_process = leftover_bytes;
                    handle_sending_audio_data();
                }
                input_buffer.clear();
                return;
            }
            parse_http_response_line(line.c_str(), line.length(), line_count, &http_data->response);
            line_count++;
        }
    } 
    // Handle Audio and ICY Metadata Parsing
    else if (state == RadioState::STREAMING_AUDIO) {
        memcpy(audio_data_buffer, network_data_buffer, bytes_read);
        auto* data = std::get_if<AudioStreamData>(&state_data);
        data->bytes_to_process = bytes_read;
        handle_sending_audio_data();
    }
}

void RadioClient::handle_http(HttpResponse& response) {
    // 1. Success (200 OK)
    if (response.status_code == 200) {
        this->state = RadioState::STREAMING_AUDIO;
        
        AudioStreamData audio_data;
        audio_data.state = StreamState::READING_AUDIO;
        audio_data.bytes_to_process = 0;
        audio_data.meta_bytes_remaining = 0;
        
        // Directly map the parsed integer from your struct
        audio_data.icy_metaint = response.icy_metaint;
        audio_data.bytes_until_metadata = response.icy_metaint;

        // Lock in the new state data
        this->state_data = audio_data;
        return;
    }

    // 2. Redirection (3xx)
    if (response.status_code >= 300 && response.status_code < 400) {
        if (response.location.empty()) {
            std::cerr << "Error: Received HTTP " << response.status_code 
                      << " redirect but no 'Location' provided." << std::endl;
            this->state = RadioState::SHUTDOWN;
            return;
        }

        // Store the cookie if the server gave us one
        if (!response.cookie.empty()) {
            this->cookie = response.cookie;
        }

        // TODO: You will need a URL parser here to extract the new 
        // scheme, host, path, and port from `response.location`.
        parse_url(response.location, this->scheme, this->host, this->port, this->path);

        // Close the current socket so the run() loop establishes a new connection
        this->connection.close_connection();

        // Reset state so the client reconnects to the new host
        this->state = RadioState::NOT_CONNECTED;
        this->state_data = std::monostate{};
        return;
    }

    // 3. Client or Server Errors (4xx / 5xx)
    if (response.status_code >= 400) {
        his->connection.close_connection();
        throw HttpException(response.status_code);
    }
}

bool RadioClient::handle_user_input() {
    char buf[USER_INPUT_MAX];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

    if (n > 0) {
        input_buffer.append(buf, n);

        size_t pos;
        while ((pos = input_buffer.find('\n')) != std::string::npos) {
            std::string line = input_buffer.substr(0, pos);
            input_buffer.erase(0, pos + 1);

            if (line == "quit") {
                connection.close_connection();
                process_stream_data();
                return true;
            }
        }
        if(input_buffer.size() > 4){
            input_buffer = "";
        }
    }
    else if (n == 0) {
        LOGD("stdin EOF");
    }
    else {
        if (errno != EINTR) {
            throw std::runtime_error(std::string("stdin read error: ") + strerror(errno));
        }
    }
    return false;
}

void process_stream_data(const uint8_t* audio_stream_buffer) {
    // Ensure we get a reference to the state, not a copy!
    AudioStreamData& state = std::get<AudioStreamData>(this->state_data); 

    if (!audio_stream_buffer || state.bytes_to_process == 0) return;

    // If there is no metadata interval, treat the entire buffer as audio
    if (state.icy_metaint <= 0) {
        handle_audio(audio_stream_buffer, state.bytes_to_process);
        state.bytes_to_process = 0; // Buffer consumed
        return;
    }

    size_t offset = 0;

    // Loop until we have processed all the bytes available in the current buffer
    while (offset < state.bytes_to_process) {
        size_t available = state.bytes_to_process - offset;

        switch (state.state) {
            
            case StreamState::READING_AUDIO: {
                size_t audio_chunk = std::min(static_cast<size_t>(state.bytes_until_metadata), available);
                
                handle_audio(audio_stream_buffer + offset, audio_chunk);
                
                offset += audio_chunk;
                state.bytes_until_metadata -= audio_chunk;

                if (state.bytes_until_metadata == 0) {
                    state.state = StreamState::READING_META_LENGTH;
                }
                break;
            }

            case StreamState::READING_META_LENGTH: {
                uint8_t length_byte = audio_stream_buffer[offset];
                offset += 1;
                
                size_t payload_len = static_cast<size_t>(length_byte) * 16;
                
                if (payload_len == 0) {
                    state.state = StreamState::READING_AUDIO;
                    state.bytes_until_metadata = state.icy_metaint;
                } else {
                    state.meta_bytes_remaining = payload_len;
                    state.metadata_buffer.clear();
                    state.state = StreamState::READING_META_PAYLOAD;
                }
                break;
            }

            case StreamState::READING_META_PAYLOAD: {
                size_t meta_chunk = std::min(state.meta_bytes_remaining, available);
                
                state.metadata_buffer.append(reinterpret_cast<const char*>(audio_stream_buffer + offset), meta_chunk);
                
                offset += meta_chunk;
                state.meta_bytes_remaining -= meta_chunk;

                if (state.meta_bytes_remaining == 0) {
                    handle_accumulated_metadata(state.metadata_buffer);
                    
                    state.state = StreamState::READING_AUDIO;
                    state.bytes_until_metadata = state.icy_metaint;
                }
                break;
            }
        }
    }
    
    // Reset to 0 since we have successfully processed all bytes in this chunk
    state.bytes_to_process = 0;
}

void RadioClient::run() {
    this->state = RadioState::NOT_CONNECTED;
    struct pollfd pfds[2];

    pfds[0].fd = STDIN_FILENO;
 
    while () {
        try {
            // 1. Setup / Re-setup Connection
            if (this->state == RadioState::NOT_CONNECTED || this->connection.get_ms_until_timeout(this->timeout_ms) == 0 ) {
                this->connection.resolve_name(this->scheme, this->host, std::to_string(this->port), this->family_pref);
                this->connection.connect();
                this->state = RadioState::SENDING_HTTP;
                size_t message_len = create_http_request(this->network_data_buffer, this->host, this->path, this->multiplex, this->cookie);
                this->state_data = SendingData{message_len, 0};
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
                continue;
            } else if (ret < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("poll error: ") + strerror(errno));
            }

            // 3. User Input
            if ((pfds[0].revents & POLLIN) && handle_user_input()) {
                return 0;
            }

            // 4. Network IO
            if (this->state != RadioState::SHUTDOWN && pfds[1].revents & (POLLIN | POLLOUT | POLLHUP | POLLERR)) {
                
                if (pfds[1].revents & POLLERR) {
                    throw std::runtime_error("Network socket error (POLLERR)");
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
            // ONLY catch graceful disconnections here
            handle_sending_audio_data();
            this->connection.close_connection();
            return 0;
        } 
        catch (const std::exception& e) {
            // Any other exception: ResolveException, ConnectionException, SSLException, runtime_error...
            std::cerr << "Fatal Error: " << e.what() << std::endl;
            this->connection.close_connection();
            exit(1); 
        } 
        catch (...) {
            // Failsafe for untyped exceptions
            std::cerr << "Unknown fatal exception occurred." << std::endl;
            this->connection.close_connection();
            exit(1);
        }
    }
}