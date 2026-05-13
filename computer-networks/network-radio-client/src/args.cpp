#include "args.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdint>
#include <stdexcept> // Added for std::invalid_argument
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

// Now throws instead of returning bool
void parse_int(const std::string &s, int min_v, int max_v, int &out, const std::string& context) {
    if (s.empty()) {
        throw std::invalid_argument(context + ": empty value");
    }
    
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    
    // Exception: indicates the input wasn't a valid integer
    if (errno != 0 || end == s.c_str() || *end != '\0') {
        throw std::invalid_argument(context + ": '" + s + "' is not a valid integer");
    }
    
    // Exception: indicates the value is technically an int but outside business logic bounds
    if (v < min_v || v > max_v || v > INT_MAX || v < INT_MIN) {
        throw std::invalid_argument(context + ": value " + s + " out of range");
    }
    out = static_cast<int>(v);
}

// Throws if the user provided a flag (like -u) but forgot the actual value
std::string take_value(int &i, int argc, char **argv) {
    if (i + 1 >= argc) {
        throw std::invalid_argument("missing parameter value for " + std::string(argv[i]));
    }
    return argv[++i];
}

}

// Throws if the URL is malformed or missing a host
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
    host   = std::string(u.host());
    path   = std::string(u.path());

    if (path.empty()) path = "/";

    if (u.has_port()) {
        int p = 0;
        // Reusing our throwing parse_int helper
        parse_int(std::string(u.port()), MIN_PORT, MAX_PORT, p, "URL port");
        port = static_cast<uint16_t>(p);
    } else {
        port = (scheme == "https") ? HTTPS_PORT : HTTP_PORT;
    }

    if (host.empty()) {
        throw std::invalid_argument("URL is missing a host: " + url);
    }
}

// Main parsing logic: No longer returns bool. 
// If it completes, 'opt' is guaranteed to be valid.
void parse_args(int argc, char **argv, Options &opt) {
    opt = Options{};

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
                    j = arg.size(); // Break inner loop
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

    // This will throw internally if the URL is bad
    parse_url(opt.url, opt.scheme, opt.host, opt.path, opt.port);

    if (opt.force_ipv4 && opt.force_ipv6) {
        opt.force_ipv4 = false;
        opt.force_ipv6 = false;
    }
}