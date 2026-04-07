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

#define BUFFER_SIZE 12
#define MAX_TIME  99
#define INPUT_LENGHT 9


typedef struct {
    uint8_t* pawn_row;
    struct sockaddr_in server_address;
    uint16_t port;
    uint32_t server_timeout;
} ServerInput;

ServerInput parse_server_input(char *argv[]){
    ServerInput args;
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

            case 'r': {
                size_t len = strlen(argv[i+1]);
                if(len > MAX_UINT8 || len == 0){
                    fatal("pawn raw lenght out of range (1-%u): %s", MAX_PAWN_ROW_LENGHT, argv[i+1]);
                }
                size_t bytes = (len + 7) / 8;
                uint8_t *pawn_row = (uint8_t *) calloc(bytes, sizeof(uint8_t));
                if(!pawn_row){
                    syserr("malloc failed for pawn row");
                }

                for(size_t j = 0; j < bytes; j++){
                    if(argv[i+1][j] == '1'){
                        bitset_set(pawn_row, i, true);
                    }else if(argv[i+1][j] != '0'){
                        free(pawn_row);
                        fatal("wrong format of pawn row: %s", argv[i+1]);
                    }
                }
                args.pawn_row = pawn_row;
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
                args.server_timeout = value;
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
        fatal("usage: %s -r <pawn row> -a <address> -p <port> -t <client_timeout>\n", argv[0]);
    }

    ServerInput args = parse_server_input(argv);

    // Create a socket. Buffer should not be allocated on the stack.
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // Bind the socket to a concrete address.
    if (bind(socket_fd, (struct sockaddr *) &args.server_address, (socklen_t) sizeof(args.server_address)) < 0) {
        syserr("bind");
    }

    printf("listening on port %" PRIu16 "\n", args.port);

    
    ssize_t received_length;
    do {
        // Receive a message. Buffer should not be allocated on the stack.
        static uint8_t buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer)); // Clean the buffer.

        int flags = 0;
        struct sockaddr_in client_address;
        socklen_t address_length = (socklen_t) sizeof(client_address);

        received_length = recvfrom(socket_fd, buffer, BUFFER_SIZE, flags,
                                   (struct sockaddr *) &client_address, &address_length);
        if (received_length < 0) {
            syserr("recvfrom");
        }

        char const *client_ip = inet_ntoa(client_address.sin_addr);
        uint16_t client_port = ntohs(client_address.sin_port);
        printf("received %zd bytes from %s:%" PRIu16 "\n",
        received_length, client_ip, client_port);

        printf("bytes: ");
        for (ssize_t i = 0; i < received_length; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
        
        // validating the message
        uint8_t error_index;
        ClientMessage msg;
        if(deserialize_client_message(&msg, buffer, received_length, &error_index)){

        }
        // 

        // Send the message back.
        int send_flags = 0;
        ssize_t sent_length = sendto(socket_fd, buffer, received_length, send_flags,
                                     (struct sockaddr *) &client_address, address_length);
        if (sent_length < 0) {
            syserr("sendto");
        }
        else if (sent_length != received_length) {
            fatal("incomplete sending");
        }
    } while (received_length > 0);

    printf("exchange finished\n");

    close(socket_fd);
    return 0;
}