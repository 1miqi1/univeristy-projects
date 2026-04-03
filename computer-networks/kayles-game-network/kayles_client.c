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
    if (argc < 9) {
        fatal("usage: %s -p <port> -a <address> -m <message> -t <client_timeout> ...\n", argv[0]);
    }
    
    uint16_t port;
    uint32_t client_timeout;
    struct sockaddr_in server_address;
    ClientMessage clientMessage;

    for(int i = 1; i < 9; i+= 2){
        if(argv[i][0] == '-'){
            switch (argv[i][1]){
                case 'p':
                    port = read_port(argv[i+1]);
                    break;
                case 'a':
                    char const *host = argv[i+1];
                    struct sockaddr_in server_address = get_server_address(host, port);
                    break;
                case 'm':
                    if(parse_client_message(argv[i+1], &clientMessage)){
                        fatal("wrong client message %s", argv[i+1]);
                    }
                    break;
                case 't':
                    if(validate_number(argv[i+1], client_timeout)){
                        fatal("wrong client timeout %s", argv[i+1]);
                    }
                    client_timeout = (uint8_t)client_timeout;
                    if(!client_timeout > 0 || client_timeout > MAX_TIME){
                        fatal("wrong client timeout %s", argv[i+1]);
                    }
                    break;
                default:
                    break;
                }
        }else{
            fatal("wrong input");
        }
    }

    printf("Formatted output:\n");
    printf("port: %d", port);
    printf("client timeout: %d", client_timeout);
    printf("address: %s", server_address.sin_addr);
    print_client_message(&clientMessage);
}