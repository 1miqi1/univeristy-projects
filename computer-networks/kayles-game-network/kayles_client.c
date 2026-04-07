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

#define MAX_TIME  99
#define BUFFER_SIZE 14
#define INPUT_LENGHT 9

typedef struct {
    uint16_t port;
    uint32_t client_timeout;
    struct sockaddr_in server_address;
    ClientMessage clientMessage;
} ClientInput;

ClientInput parse_client_input(char *argv[]){
    ClientInput args;
    char *host = NULL;

    for (int i = 1; i < INPUT_LENGHT; i += 2){
        if(sizeof(argv[i]) < 2*sizeof(uint8_t) || argv[i][0] != '-'){
            fatal("wrong input: %s", argv[i]);
        }
        if(argv[i][1] == 'p'){
            args.port = read_port(argv[i + 1]);
            break;
        }
    }

    for (int i = 1; i < INPUT_LENGHT; i += 2) {
        switch (argv[i][1]) {
            case 'p':
                break;

            case 'a': {
                host = argv[i + 1];
                args.server_address = get_server_address(host, args.port);
                break;
            }

            case 'm': {
                ParseResult res = parse_client_message(argv[i + 1], &args.clientMessage);
                if (res.status != PARSE_OK) {
                    if (res.status == PARSE_ERR_RESOURCE) {
                        // System level failure (e.g. strdup failed)
                        syserr("parsing system error: %s", res.msg);
                    } else {
                        // User/Protocol level failure
                        fatal("wrong client message '%s': %s", argv[i + 1], res.msg);
                    }
                }
                break;
            }

            case 't': {
                uint32_t value;
                // Using the updated validate_number return logic
                if (validate_number(argv[i + 1], &value) != 0) {
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

    return args;
}

int main(int argc, char *argv[]) {
    if (argc != INPUT_LENGHT) {
        fatal("usage: %s -p <port> -a <address> -m <message> -t <client_timeout>\n", argv[0]);
    }

    ClientInput args = parse_client_input(argv);


    // Convert IP to string for logging
    char addr_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &args.server_address.sin_addr, addr_str, sizeof(addr_str)) == NULL) {
        syserr("inet_ntop");
    }

    char const *server_ip = inet_ntoa(args.server_address.sin_addr);
    uint16_t server_port = ntohs(args.server_address.sin_port);

    printf("Formatted output:\n");
    printf("port: %u\n", args.port);
    printf("client timeout: %" PRIu32 "\n", args.client_timeout);
    printf("address: %s\n", addr_str);

    print_client_message(&args.clientMessage);

    // Communication start
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    size_t message_length = get_client_message_size(&args.clientMessage);
    uint8_t *buf = malloc(message_length * sizeof(uint8_t));

    if (!buf) {       
        syserr("malloc failed for message buffer");
    }

    serialize_client_message(&args.clientMessage, buf);
    printf("sending to %s:%u\n", addr_str, ntohs(args.server_address.sin_port));
    printf("bytes: ");
    for (size_t i = 0; i < message_length; i++) {
            printf("%02X ", (unsigned char)buf[i]);
    }  
    printf("\n");

    int send_flags = 0;
    socklen_t address_length = (socklen_t) sizeof(args.server_address);
    ssize_t sent_length = sendto(socket_fd, buf, message_length, send_flags,
                                    (struct sockaddr *) &args.server_address, address_length);
                     
    free(buf);
    
    if (sent_length < 0) {
        syserr("sendto");
    } else if ((size_t) sent_length != message_length) {
        fatal("incomplete sending: expected %zu, sent %zd", message_length, sent_length);
    }



    // Recieve a message
    static char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer)); // Clean the buffer.

    size_t max_length = sizeof(buffer);
    int receive_flags = 0;
    struct sockaddr_in receive_address;
    address_length = (socklen_t) sizeof(receive_address);
    ssize_t received_length = recvfrom(socket_fd, buffer, max_length, receive_flags,
                                        (struct sockaddr *) &receive_address, &address_length);
    if (received_length < 0) {
        syserr("recvfrom");
    }

    printf("received %zd bytes from %s:%" PRIu16 "\n",
        received_length, server_ip, server_port);

    printf("bytes: ");
    for (ssize_t i = 0; i < received_length; i++) {
        printf("%02X ", (unsigned char)buffer[i]);
    }

    close(socket_fd);
    return 0;
}