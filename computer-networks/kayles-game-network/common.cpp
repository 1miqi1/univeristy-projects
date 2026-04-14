#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>
#include <vector>

#include "common.h"
#include "err.h"

constexpr std::size_t BYTE_SIZE = 8;

// Reads a single bit from a bitset stored in a byte vector.
// Returns false if the requested bit index is out of bounds.
bool bitset_get(const std::vector<std::uint8_t>& bitset,
                std::size_t index,
                std::uint8_t& value) {
    if (bitset.size() * BYTE_SIZE <= index) {
        return false;
    }

    const std::size_t byte_index = index / BYTE_SIZE;
    const std::size_t bit_pos = 7 - (index % BYTE_SIZE);  // MSB is bit 0.
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit_pos);

    value = (bitset[byte_index] & mask) != 0;
    return true;
}

// Sets or clears a single bit in a bitset stored in a byte vector.
// Returns false if the requested bit index is out of bounds.
bool bitset_set(std::vector<std::uint8_t>& bitset,
                std::size_t index,
                bool value) {
    if (bitset.size() * BYTE_SIZE <= index) {
        return false;
    }

    const std::size_t byte_index = index / BYTE_SIZE;
    const std::size_t bit_pos = 7 - (index % BYTE_SIZE);  // MSB is bit 0.
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit_pos);

    if (value) {
        bitset[byte_index] |= mask;
    } else {
        bitset[byte_index] &= static_cast<std::uint8_t>(~mask);
    }

    return true;
}

// Checks whether the string contains a valid non-negative decimal number
// that fits in uint32_t. On success stores the parsed value in result.
bool validate_number(const std::string& token, std::uint32_t& result) {
    if (token.empty()) {
        return false;
    }

    for (unsigned char c : token) {
        if (!std::isdigit(c)) {
            return false;
        }
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(token.c_str(), &end, 10);

    if (errno == ERANGE || *end != '\0' ||
        value > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    result = static_cast<std::uint32_t>(value);
    return true;
}

// Parses a port number from text.
// Terminates the program with fatal() if the value is invalid.
std::uint16_t read_port(const char* string) {
    char* endptr = nullptr;
    errno = 0;
    const unsigned long port = std::strtoul(string, &endptr, 10);

    if (errno != 0 || *endptr != '\0' ||
        port > std::numeric_limits<std::uint16_t>::max()) {
        fatal("%s is not a valid port number", string);
    }

    return static_cast<std::uint16_t>(port);
}

// Parses a non-negative size value from text.
// Terminates the program with fatal() if the value is invalid.
std::size_t read_size(const char* string) {
    char* endptr = nullptr;
    errno = 0;
    const unsigned long long number = std::strtoull(string, &endptr, 10);

    if (errno != 0 || *endptr != '\0' ||
        number > std::numeric_limits<std::size_t>::max()) {
        fatal("%s is not a valid number", string);
    }

    return static_cast<std::size_t>(number);
}

// Resolves a host name to an IPv4 socket address and assigns the given port.
sockaddr_in get_server_address(const char* host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* address_result = nullptr;
    const int errcode = getaddrinfo(host, nullptr, &hints, &address_result);
    if (errcode != 0 || address_result == nullptr) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    sockaddr_in send_address =
        *reinterpret_cast<sockaddr_in*>(address_result->ai_addr);
    send_address.sin_port = htons(port);

    freeaddrinfo(address_result);
    return send_address;
}