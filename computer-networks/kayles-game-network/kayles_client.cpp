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
constexpr std::size_t BUFFER_SIZE = 60;
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
    if (argc < INPUT_LENGTH || argc % 2 == 0) {
        fatal("usage: %s -p <port> -a <address> -m <message> -t <client_timeout>", argv[0]);
    }

    ClientInput args;

    // ----------------------------
    // Check for all options
    // ----------------------------
    const char* port_str = nullptr;
    const char* addr_str = nullptr;
    const char* msg_str = nullptr;
    const char* timeout_str = nullptr;

    for (int i = 1; i < argc; i += 2) {
        if (std::strlen(argv[i]) != 2 || argv[i][0] != '-') {
            fatal("wrong option format: %s", argv[i]);
        }

        char opt = argv[i][1];

        switch (opt) {
            case 'p': port_str = argv[i + 1]; break;
            case 'a': addr_str = argv[i + 1]; break;
            case 'm': msg_str = argv[i + 1]; break;
            case 't': timeout_str = argv[i + 1]; break;
            default:
                fatal("unknown option: %s", argv[i]);
        }
    }

    if (!port_str) fatal("missing required option: -p");
    if (!addr_str) fatal("missing required option: -a");
    if (!msg_str) fatal("missing required option: -m");
    if (!timeout_str) fatal("missing required option: -t");


    // ----------------------------
    // Validate and convert port
    // ----------------------------
    args.port = read_port(port_str);
    if (args.port == 0) {
        fatal("client port must be in range 1-65535");
    }

    // ----------------------------
    // Resolve server address
    // ----------------------------
    args.server_address = get_server_address(addr_str, args.port);

    // ----------------------------
    // Parse and validate client message
    // ----------------------------
    ParseResult res = parse_client_message(msg_str, args.clientMessage);
    if (res.status != PARSE_OK) {
        if (res.status == PARSE_ERR_RESOURCE) {
            syserr("parsing system error: %s", res.msg);
        } else {
            fatal("wrong client message '%s': %s", msg_str, res.msg);
        }
    }

    // ----------------------------
    // Validate and convert timeout
    // ----------------------------
    uint32_t value = 0;
    if (!validate_number(timeout_str, value)) {
        fatal("wrong format of client timeout: %s", timeout_str);
    }
    if (value == 0 || value > MAX_TIME) {
        fatal("client timeout out of range (1-%u): %s", MAX_TIME, timeout_str);
    }
    args.client_timeout = value;

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

    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                &timeout, sizeof(timeout)) < 0) {
        close(socket_fd);
        syserr("setsockopt");
    }

    ssize_t received_length = recvfrom(socket_fd, buf, BUFFER_SIZE, receive_flags,
                                    reinterpret_cast<struct sockaddr *>(&receive_address),
                                    &address_length);

    if (received_length < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "Client waited: " << args.client_timeout << "s, but server didn't respond. " << "\n";
            return 0;
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


    // ----------------------------
    // Cleanup
    // ----------------------------
    close(socket_fd);
    return 0;
}