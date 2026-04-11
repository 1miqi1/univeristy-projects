#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <netinet/in.h>
#include <string>

/**
 * Reads a single bit from a bitset stored in a byte vector.
 *
 * @param bitset  Vector representing bitset (each byte stores 8 bits)
 * @param index   Bit index to read
 * @param value   Output parameter (0 or 1)
 * @return true if index is valid, false if out of bounds
 */
bool bitset_get(const std::vector<std::uint8_t>& bitset,
                std::size_t index,
                std::uint8_t& value);

/**
 * Sets or clears a bit in a bitset stored in a byte vector.
 *
 * @param bitset  Vector representing bitset (modified in place)
 * @param index   Bit index to modify
 * @param value   true = set bit to 1, false = set bit to 0
 * @return true if index is valid, false if out of bounds
 */
bool bitset_set(std::vector<std::uint8_t>& bitset,
                std::size_t index,
                bool value);

/**
 * Parses a string into a valid TCP/UDP port number.
 *
 * @param string  Input string
 * @return Parsed port number (0–65535)
 * @throws fatal() on invalid input
 */
std::uint16_t read_port(const char *string);

/**
 * Parses a string into a valid size_t value.
 *
 * @param string  Input string
 * @return Parsed size value
 * @throws fatal() on invalid input
 */
std::size_t read_size(const char *string);

/**
 * Validates and parses a numeric string into uint32_t.
 *
 * @param string  Input string (must contain only digits)
 * @param val     Output parameter for parsed value
 * @return 0 on success, -1 on failure
 */
bool validate_number(const std::string& str, std::uint32_t& val);

/**
 * Resolves a hostname into an IPv4 socket address structure.
 *
 * @param host  Hostname or IP address (e.g. "localhost", "127.0.0.1")
 * @param port  Port number
 * @return sockaddr_in structure ready for use in socket calls
 * @throws fatal() on resolution failure
 */
struct sockaddr_in get_server_address(const char *host,
                                      std::uint16_t port);

#endif