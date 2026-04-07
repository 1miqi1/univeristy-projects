#ifndef COMMON_H
#define COMMON_H

bool bitset_get(const uint8_t *bitset, size_t index);
void bitset_set(uint8_t *bitset, size_t index, bool value);
uint16_t read_port(char const *string);
size_t read_size(char const *string);
int validate_number(char const *string, uint32_t* val);
struct sockaddr_in get_server_address(char const *host, uint16_t port);


#endif