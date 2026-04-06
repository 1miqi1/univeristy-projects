#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "err.h"
#include "common.h"
#include "protocol.h"

int main(int argc, char *argv[]) {
    if (argc != 9) {
        fatal("usage: %s -p <port> -a <address> -m <message> -t <client_timeout>\n", argv[0]);
    }

    uint16_t port = 0;
    uint32_t client_timeout = 0;
    char *host;
    struct sockaddr_in server_address;
    ClientMessage clientMessage;

    for (int i = 1; i < 9; i += 2) {
        if (argv[i][0] != '-') {
            fatal("wrong input: %s", argv[i]);
        }

        switch (argv[i][1]) {
            case 'p':
                port = read_port(argv[i + 1]);
                break;

            case 'a': {
                host = argv[i + 1];
                server_address = get_server_address(host, port);
                break;
            }

            case 'm': {
                ParseResult res = parse_client_message(argv[i + 1], &clientMessage);
                if (res != PARSE_OK) {
                    fatal("wrong client message '%s': %s",
                          argv[i + 1],
                          parse_error_string(res));
                }
                break;
            }

            case 't': {
                uint32_t value;
                if (validate_number(argv[i + 1], &value) != 0) {
                    fatal("wrong format of client timeout: %s", argv[i + 1]);
                }
                if (value <= 0 || value > MAX_TIME) {
                    fatal("client timeout out of range: %s", argv[i + 1]);
                }
                client_timeout = (uint8_t)value;
                break;
            }

            default:
                fatal("unknown option: %s", argv[i]);
        }
    }

    // Convert IP to string
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server_address.sin_addr, addr_str, sizeof(addr_str));

    printf("Formatted output:\n");
    printf("port: %u\n", port);
    printf("client timeout: %" PRIu32 "\n", client_timeout);
    printf("address: %s\n", addr_str);

    print_client_message(&clientMessage);

    //Communication start
    char const *server_ip = inet_ntoa(server_address.sin_addr);
    uint16_t server_port = ntohs(server_address.sin_port);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    size_t  message_length = get_client_message_size(&clientMessage);
    uint8_t *buf = malloc(message_length);

    if (!buf) {       
        fatal("malloc failed for message buffer");
    }
    serialize_client_message(&clientMessage, buf);

    int send_flags = 0;
    socklen_t address_length = (socklen_t) sizeof(server_address);
    ssize_t sent_length = sendto(socket_fd, buf, message_length, send_flags,
                                    (struct sockaddr *) &server_address, address_length);

    free(buf);
    
    if (sent_length < 0) {
        syserr("sendto");
    }
    else if ((size_t) sent_length != message_length) {
        fatal("incomplete sending");
    }

    printf("sent to %s: %s'\n", server_ip, server_port);



    return 0;
}