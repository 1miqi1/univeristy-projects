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

void RadioClient::run() {
    this->state = RadioState::NOT_CONNECTED;
    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = -1;
    fds[1].events = POLLIN;
    fds[1].revents = 0;
 
    while (true) {
        if(this->state == RadioState::CONNECTING){

        }


        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            char buf[USER_INPUT_MAX];

            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

            if (n > 0) {
                this->input_buffer.append(buf, n);

                size_t pos;
                while ((pos = input_buffer.find('\n')) != std::string::npos) {
                    std::string line = input_buffer.substr(0, pos);
                    input_buffer.erase(0, pos + 1);

                    if (line == "quit") {
                        this->state = RadioState::USER_QUIT;
                        break;
                    }
                }
            }else if (n == 0) {
                // EOF (stdin closed)
                LOGD("stdin EOF");
            }
            else { // n < 0
                if (errno == EINTR) {
                    // interrupted, just retry next loop
                } else {
                    syserr("stdin read error");
                }
            }
        }
    }
}