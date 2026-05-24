#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Structure to hold all parsed command-line options and URL components.
 */
struct Options {
    std::string url;          ///< The full target URL
    bool multiplex = false;   ///< Flag to enable multiplexing (-m)
    int timeout_ms = 5000;    ///< Connection timeout in milliseconds (-t)
    bool force_ipv4 = false;  ///< Flag to force IPv4 resolution (-4)
    bool force_ipv6 = false;  ///< Flag to force IPv6 resolution (-6)
    int verbosity = 2;        ///< Logging verbosity level (-v)

    std::string scheme;       ///< Parsed URL scheme (e.g., http, https)
    std::string host;         ///< Parsed URL hostname
    std::string path;         ///< Parsed URL path including query string
    uint16_t port = 0;        ///< Parsed or inferred network port
};

/**
 * @brief Prints the parsed options to stderr for debugging purposes.
 * 
 * @param opt The Options structure containing current configurations.
 */
void print_options(const Options &opt);

/**
 * @brief Parses command-line arguments and populates the Options struct.
 * 
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @param opt  The Options structure to populate.
 * @throws std::invalid_argument If parameters are missing, malformed, or out of range.
 */
void parse_args(int argc, char **argv, Options &opt);

/**
 * @brief Parses a URL string into its individual components.
 * 
 * @param url    The input URL string.
 * @param scheme Extracted scheme (output).
 * @param host   Extracted hostname (output).
 * @param path   Extracted path and query (output).
 * @param port   Extracted or inferred port (output).
 * @throws std::invalid_argument If the URL is malformed or missing required components.
 */
void parse_url(const std::string& url,
               std::string& scheme,
               std::string& host,
               std::string& path,
               uint16_t& port);