#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

uint16_t read_port(char const *string);
size_t read_size(char const *string);
int validate_number(char const *string, uint32_t* val);
struct sockaddr_in get_server_address(char const *host, uint16_t port);

#ifdef __cplusplus
} // extern "C"
#endif

#endif