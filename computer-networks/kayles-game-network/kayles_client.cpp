#include <sys/socket.h>
#include <inttypes.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <span>
#include <set>

constexpr int MAX_TIME = 99;
constexpr std::size_t BUFFER_SIZE = 1024;
constexpr int INPUT_LENGTH = 9;

#include "err.h"
#include "common.h"
#include "protocol.h"

/**
 * Structure holding parsed client input parameters.
 */
struct ClientInput {
    uint16_t port = 0;                    /**< Server port number */
    time_t client_timeout = 0;          /**< Client timeout value */
    sockaddr_in server_address{};         /**< Resolved server address */
    ClientMessage clientMessage{};        /**< Parsed client message */
};

/**
 * Parses command-line arguments into a ClientInput structure.
 *
 * Expected format:
 *   -p <port> -a <address> -m <message> -t <timeout>
 *
 * @param argv  Command-line arguments
 * @return Parsed ClientInput structure
 * @throws fatal() on invalid input
 */
ClientInput parse_client_input(char *argv[], int argc) {
    if (argc < INPUT_LENGTH) {
        fatal("usage: %s -p <port> -a <address> -m <message> -t <client_timeout>", argv[0]);
    }

    ClientInput args;
    std::set<char> flags;

    for (int i = 1; i + 1 < argc; i+= 2) {
        if (std::strlen(argv[i]) < 2 || argv[i][0] != '-') {
            fatal("wrong input: %s", argv[i]);
        }

        if (argv[i][1] == 'p' && !flags.count('p')) {
            args.port = read_port(argv[i + 1]);
            if(args.port == 0){
                fatal("wrong port number");
            }
            flags.insert('p');
        }
    }

    if (!flags.count('p')) {
        fatal("missing required option: -p");
    }

    for (int i = 1; i + 1 < argc; i+= 2) {
        if(flags.count(argv[i][1])){
            continue;
        }
        flags.insert(argv[i][1]);
        switch (argv[i][1]) {
            case 'p':
                break;
            case 'a':{
                args.server_address = get_server_address(argv[i + 1], args.port);
                break;
            }
            case 'm':{
                ParseResult res = parse_client_message(argv[i + 1], args.clientMessage);

                if (res.status != PARSE_OK) {
                    if (res.status == PARSE_ERR_RESOURCE) {
                        syserr("parsing system error: %s", res.msg);
                    } else {
                        fatal("wrong client message '%s': %s", argv[i + 1], res.msg);
                    }
                }
                break;
            }
            case 't':{
                uint32_t value = 0;

                if (!validate_number(argv[i + 1], value)) {
                    fatal("wrong format of client timeout: %s", argv[i + 1]);
                }

                if (value == 0 || value > MAX_TIME) {
                    fatal("client timeout out of range (1-%u): %s", MAX_TIME, argv[i + 1]);
                }

                args.client_timeout = value;
                break;
            }
            default:
                fatal("unknown option: %s", argv[i]);
        }
    }
    if(!(flags.count('p') && flags.count('a') && flags.count('t') && flags.count('m'))){
        fatal("not all options provided");
    }
    return args;
}

/**
 * Entry point of the client application.
 *
 * Responsibilities:
 *  - Parse input arguments
 *  - Serialize and send client message
 *  - Receive and deserialize server response
 *  - Print results
 *
 * @param argc  Argument count
 * @param argv  Argument vector
 * @return 0 on success
 */
int main(int argc, char *argv[]) {

    // ----------------------------
    // Argument validation adn input parsing
    // ----------------------------
    ClientInput args = parse_client_input(argv, argc);

    // ----------------------------
    // Buffer initialization
    // ----------------------------
    std::uint8_t buf[BUFFER_SIZE];
    std::memset(buf, 0, sizeof(buf));

    // ----------------------------
    // Address formatting (for logging)
    // ----------------------------
    char addr_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &args.server_address.sin_addr, addr_str, sizeof(addr_str)) == nullptr) {
        syserr("inet_ntop");
    }



    // ----------------------------
    // Socket creation
    // ----------------------------
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // ----------------------------
    // Serialization of client message
    // ----------------------------
    std::size_t written =
        serialize_client_message(args.clientMessage, std::span<std::uint8_t>(buf, BUFFER_SIZE));

    if (written == 0) {
        close(socket_fd);
        fatal("serialize_client_message failed");
    }

    // ----------------------------
    // Sending message to server
    // ----------------------------
    int send_flags = 0;
    socklen_t address_length = static_cast<socklen_t>(sizeof(args.server_address));

    ssize_t sent_length = sendto(socket_fd, buf, written, send_flags,
                                 reinterpret_cast<struct sockaddr *>(&args.server_address),
                                 address_length);

    if (sent_length < 0) {
        close(socket_fd);
        syserr("sendto");
    } else if (static_cast<std::size_t>(sent_length) != written) {
        close(socket_fd);
        fatal("incomplete sending: expected %zu, sent %zd", written, sent_length);
    }

    // ----------------------------
    // Prepare buffer for receiving
    // ----------------------------
    std::memset(buf, 0, sizeof(buf));

    int receive_flags = 0;
    sockaddr_in receive_address{};
    address_length = static_cast<socklen_t>(sizeof(receive_address));

    // ----------------------------
    // Receiving response from server
    // ----------------------------
    struct timeval timeout;
    timeout.tv_sec = args.client_timeout;   
    timeout.tv_usec = 0;

    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ssize_t received_length = recvfrom(socket_fd, buf, BUFFER_SIZE, receive_flags,
                                       reinterpret_cast<struct sockaddr *>(&receive_address),
                                       &address_length);

    if (received_length < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "Client waited: " << args.client_timeout << "s, but server didn't respond. " << "\n";
            exit(0);
        }
        close(socket_fd);
        syserr("recvfrom");
    }

    // ----------------------------
    // Address formatting (response sender)
    // ----------------------------
    char recv_addr_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &receive_address.sin_addr, recv_addr_str, sizeof(recv_addr_str)) == nullptr) {
        close(socket_fd);
        syserr("inet_ntop");
    }

    // ----------------------------
    // Print server response
    // ----------------------------
    for (ssize_t i = 0; i < received_length; i++) {
        std::printf("%02X ", buf[i]);
    }
    std::printf("\n");

    ServerResponse server_response;
    deserialize(server_response, buf);

    // ----------------------------
    // Cleanup
    // ----------------------------
    close(socket_fd);
    return 0;
}