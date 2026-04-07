#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "err.h"
#include "common.h"

bool bitset_get(const uint8_t *bitset, size_t index) {
    if (!bitset) return false;
    size_t byte_index = index / 8;
    size_t bit_pos   = 7 - (index % 8);  // MSB = pion 0
    return (bitset[byte_index] >> bit_pos) & 1;
}


void bitset_set(uint8_t *bitset, size_t index, bool value) {
    if (!bitset) return;
    size_t byte_index = index / 8;
    size_t bit_pos   = 7 - (index % 8);  // MSB = pion 0
    if (value)
        bitset[byte_index] |= 1 << bit_pos;
    else
        bitset[byte_index] &= ~(1 << bit_pos);
}


int validate_number(const char* token, uint32_t* result) {
    if (!token || strlen(token) == 0) {
        return -1;
    }

    for (size_t i = 0; token[i] != '\0'; i++) {
        if (!isdigit((unsigned char)token[i])) return -1;
    }

    char* end;
    errno = 0;
    unsigned long val = strtoul(token, &end, 10);
    if (errno == ERANGE || val > UINT32_MAX) return -1;

    *result = (uint32_t)val;
    return 0;
}

uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

size_t read_size(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long long number = strtoull(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || number > SIZE_MAX) {
        fatal("%s is not a valid number", string);
    }
    return number;
}

struct sockaddr_in get_server_address(char const *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;   // IPv4
    send_address.sin_addr.s_addr =       // IP address
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}