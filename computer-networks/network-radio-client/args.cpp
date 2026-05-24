#include "args.hpp"
#include "logger.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <boost/url.hpp>

namespace {

constexpr int TIMEOUT_MIN_MS = 100;
constexpr int TIMEOUT_MAX_MS = 100000;
constexpr int VERBOSITY_MIN = 0;
constexpr int VERBOSITY_MAX = 4;
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t HTTPS_PORT = 443;
constexpr int MIN_PORT = 1;
constexpr int MAX_PORT = 65535;

/**
 * @brief Helper function to safely parse integers from strings with bounds checking.
 */
void parse_int(const std::string &s, int min_v, int max_v, int &out, const std::string& context) {
    if (s.empty()) {
        throw std::invalid_argument(context + ": empty value");
    }
    
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    
    // Check if the input wasn't a valid integer or contained garbage characters
    if (errno != 0 || end == s.c_str() || *end != '\0') {
        throw std::invalid_argument(context + ": '" + s + "' is not a valid integer");
    }
    
    // Check business logic bounds and standard int limits
    if (v < min_v || v > max_v || v > INT_MAX || v < INT_MIN) {
        throw std::invalid_argument(context + ": value " + s + " out of range");
    }
    
    out = static_cast<int>(v);
}

/**
 * @brief Helper function to retrieve the value for a command-line flag.
 */
std::string take_value(int &i, int argc, char **argv) {
    if (i + 1 >= argc) {
        throw std::invalid_argument("missing parameter value for " + std::string(argv[i]));
    }
    return argv[++i];
}

} // anonymous namespace

void print_options(const Options &opt) {
    std::fprintf(stderr, "=== OPTIONS DUMP ===\n");
    std::fprintf(stderr, "url        : %s\n", opt.url.c_str());
    std::fprintf(stderr, "scheme     : %s\n", opt.scheme.c_str());
    std::fprintf(stderr, "host       : %s\n", opt.host.c_str());
    std::fprintf(stderr, "path       : %s\n", opt.path.c_str());
    std::fprintf(stderr, "port       : %u\n", opt.port);
    std::fprintf(stderr, "multiplex  : %s\n", opt.multiplex ? "true" : "false");
    std::fprintf(stderr, "timeout_ms : %d\n", opt.timeout_ms);
    std::fprintf(stderr, "force_ipv4 : %s\n", opt.force_ipv4 ? "true" : "false");
    std::fprintf(stderr, "force_ipv6 : %s\n", opt.force_ipv6 ? "true" : "false");
    std::fprintf(stderr, "verbosity  : %d\n", opt.verbosity);
    std::fprintf(stderr, "====================\n");
}

void parse_url(const std::string& url,
               std::string& scheme,
               std::string& host,
               std::string& path,
               uint16_t& port) 
{
    auto result = boost::urls::parse_uri(url);

    if (!result) {
        throw std::invalid_argument("invalid URL format: " + url);
    }

    const auto u = *result;
    scheme = std::string(u.scheme());
    host = std::string(u.encoded_host()); 

    if (scheme != "http" && scheme != "https") {
        throw std::invalid_argument("unsupported URL scheme (must be http or https): " + url);
    }

    // Capture both path and query string
    path = std::string(u.encoded_path());
    if (path.empty()) {
        path = "/";
    }

    if (u.has_query()) {
        path += "?" + std::string(u.encoded_query());
    }

    if (u.has_port()) {
        int p = 0;
        parse_int(std::string(u.port()), MIN_PORT, MAX_PORT, p, "URL port");
        port = static_cast<uint16_t>(p);
    } else {
        port = (scheme == "https") ? HTTPS_PORT : HTTP_PORT;
    }

    if (host.empty()) {
        throw std::invalid_argument("URL is missing a host: " + url);
    }
}

void parse_args(int argc, char **argv, Options &opt) {
    opt = Options{}; // Reset options to default

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        
        if (arg.empty() || arg[0] != '-' || arg == "-") {
            throw std::invalid_argument("invalid parameter structure: " + arg);
        }

        for (std::size_t j = 1; j < arg.size(); ++j) {
            const char c = arg[j];
            switch (c) {
                case 'm': opt.multiplex = true; break;
                case '4': opt.force_ipv4 = true; break;
                case '6': opt.force_ipv6 = true; break;
                case 'q': opt.verbosity = 0; break;

                case 'u': {
                    std::string value = (j + 1 < arg.size()) ? arg.substr(j + 1) : take_value(i, argc, argv);
                    opt.url = value;
                    j = arg.size(); // Break inner loop since we consumed the rest
                    break;
                }
                case 't': {
                    std::string value = (j + 1 < arg.size()) ? arg.substr(j + 1) : take_value(i, argc, argv);
                    parse_int(value, TIMEOUT_MIN_MS, TIMEOUT_MAX_MS, opt.timeout_ms, "timeout (-t)");
                    j = arg.size();
                    break;
                }
                case 'v': {
                    std::string value = (j + 1 < arg.size()) ? arg.substr(j + 1) : take_value(i, argc, argv);
                    parse_int(value, VERBOSITY_MIN, VERBOSITY_MAX, opt.verbosity, "verbosity (-v)");
                    j = arg.size();
                    break;
                }
                default:
                    throw std::invalid_argument(std::string("unknown parameter: -") + c);
            }
        }
    }

    if (opt.url.empty()) {
        throw std::invalid_argument("missing required parameter: -u (URL)");
    }

    // Parse internal URL components (throws if bad)
    parse_url(opt.url, opt.scheme, opt.host, opt.path, opt.port);
}