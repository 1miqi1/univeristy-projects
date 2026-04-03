#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#define MAX_TIME = 99

uint16_t read_port(char const *string);
size_t read_size(char const *string);
int validate_number(char const *string, uint32_t* val);
struct sockaddr_in get_server_address(char const *host, uint16_t port);

#endif