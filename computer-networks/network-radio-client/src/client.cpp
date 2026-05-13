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
#include <string>
#include <cstdint>
#include <poll.h>
#include <string>
#include <poll.h>
#include <unistd.h>

#define USER_INPUT_MAX  4056


void RadioClient::handle_sending_http_data() {
    SendingData info = std::get<SendingData>(this->state_data); 
    size_t start = info.bytes_sent;
    size_t max_len = info.request_len;
    ssize_t len = this->connection.write(this->network_data_buffer + info.bytes_sent, info.request_len - info.bytes_sent);
    if(len == 0 && this->connection.get_state() == ConnectionState::SERVER_CLOSED_CONNECTION){
        this->state == RadioState::SHUTDOWN;
        this->connection.close_connection();
    }
}


void RadioClient:: handle_reading(){

}

void RadioClient::handle_user_input() {
    char buf[USER_INPUT_MAX];

    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

    if (n > 0) {
        input_buffer.append(buf, n);

        size_t pos;
        while ((pos = input_buffer.find('\n')) != std::string::npos) {
            std::string line = input_buffer.substr(0, pos);
            input_buffer.erase(0, pos + 1);

            if (line == "quit") {
                state = RadioState::SHUTDOWN;
                this->connection.close_connection();
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
        if (errno == EINTR) {
        } else {
            syserr("stdin read error");
        }
    };
}

void handle_sending_data();

void RadioClient::run() {
    this->state = RadioState::NOT_CONNECTED;
    struct pollfd pfds[2];

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
 
    while (this->state != RadioState::SHUTDOWN) {
        // Handling connections 

        if(this->state == RadioState::NOT_CONNECTED || this->connection.get_ms_until_timeout(this->timeout_ms) == 0 ){
            this->connection.resolve_name(this->scheme, this->host, std::to_string(this->port), this->family_pref);
            this->connection.connect();
            this->state = RadioState::SENDING_HTTP;
            size_t message_len = create_http_request(this->network_data_buffer, this->host, this->path, this->multiplex, this->cookie);
            this->state_data = SendingData{message_len, 0};
            pfds[1].fd = this->connection.get_sockfd();
        }

        if(this->state == RadioState::SENDING_HTTP){
            pfds[0].events = POLLOUT;
        }else{
            pfds[0].events = POLLIN;
        }

        int ret = poll(pfds, 2, this->connection.get_ms_until_timeout(this->timeout_ms));

        if(ret == 0){
            continue;
        }else if(ret < 0){
            syserr("poll error");
        }

        // handling user stding
        if (pfds[0].revents & POLLIN) {
            this->handle_user_input();
        }

        if (this->state != RadioState::SHUTDOWN  && pfds[1].revents & (POLLIN | POLLOUT | POLLHUP | POLLERR)) {
            if (this->state == RadioState::SENDING_HTTP && (pfds[1].revents & POLLOUT)) {
                this->handle_sending_http_data(); 
            } 
            else if (pfds[1].revents & POLLIN) {
                this->handle_reading();
            }

            if(pfds[1].revents & POLLHUP){
                this->state == RadioState::SHUTDOWN;
            }else if(pfds[1].revents & POLLERR){
                this->connection.close_connection();
                fatal("connection error");
            }
            
            if (pfds[1].revents & (POLLHUP | POLLERR)) {
                this->state = RadioState::NOT_CONNECTED;
                this->connection.close_connection();
            }
        }

        // handling upcoming data and sending data

        handle_sending_audio_data();

    }
}