#include "args.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdint>
#include <string>
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


bool parse_int(const std::string &s, int min_v, int max_v, int &out) {
    if (s.empty()) {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') {
        return false;
    }
    if (v < min_v || v > max_v || v > INT_MAX || v < INT_MIN) {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

bool take_value(int &i, int argc, char **argv, std::string &value, std::string &error) {
    if (i + 1 >= argc) {
        error = "missing parameter value";
        return false;
    }
    value = argv[++i];
    return true;
}

bool parse_url(const std::string& url,
               std::string& scheme,
               std::string& host,
               std::string& path,
               uint16_t& port)
{
    auto result = boost::urls::parse_uri(url);

    if (!result)
        return false;

    const auto u = *result;

    scheme = std::string(u.scheme());
    host   = std::string(u.host());
    path   = std::string(u.path());

    // path normalization
    if (path.empty())
        path = "/";

    // port handling
    if (u.has_port()) {
        auto port_str = u.port();
        int p = 0;

        if (!parse_int(std::string(port_str), MIN_PORT, MAX_PORT, p))
            return false;

        port = static_cast<uint16_t>(p);
    } else {
        port = (scheme == "https") ? HTTPS_PORT : HTTP_PORT;
    }

    return !host.empty();
}

} // namespace

bool parse_args(int argc, char **argv, Options &opt, std::string &error) {
    opt = Options{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg.empty() || arg[0] != '-' || arg == "-") {
            error = "invalid parameter: " + arg;
            return false;
        }

        for (std::size_t j = 1; j < arg.size(); ++j) {
            const char c = arg[j];
            switch (c) {
                case 'm':
                    opt.multiplex = true;
                    break;
                case '4':
                    opt.force_ipv4 = true;
                    break;
                case '6':
                    opt.force_ipv6 = true;
                    break;
                case 'q':
                    opt.verbosity = 0;
                    break;

                case 'u': {
                    std::string value;
                    if (j + 1 < arg.size()) {
                        value = arg.substr(j + 1);
                    } else if (!take_value(i, argc, argv, value, error)) {
                        return false;
                    }
                    opt.url = value;
                    j = arg.size();
                    break;
                }

                case 't': {
                    std::string value;
                    if (j + 1 < arg.size()) {
                        value = arg.substr(j + 1);
                    } else if (!take_value(i, argc, argv, value, error)) {
                        return false;
                    }

                    int parsed = 0;
                    if (!parse_int(value, TIMEOUT_MIN_MS, TIMEOUT_MAX_MS, parsed)) {
                        error = "invalid -t value: " + value;
                        return false;
                    }
                    opt.timeout_ms = parsed;
                    j = arg.size();
                    break;
                }

                case 'v': {
                    std::string value;
                    if (j + 1 < arg.size()) {
                        value = arg.substr(j + 1);
                    } else if (!take_value(i, argc, argv, value, error)) {
                        return false;
                    }

                    int parsed = 0;
                    if (!parse_int(value, VERBOSITY_MIN, VERBOSITY_MAX, parsed)) {
                        error = "invalid -v value: " + value;
                        return false;
                    }
                    opt.verbosity = parsed;
                    j = arg.size();
                    break;
                }

                default:
                    error = std::string("unknown parameter: -") + c;
                    return false;
            }
        }
    }

    if (opt.url.empty()) {
        error = "missing required parameter -u";
        return false;
    }

    if (!parse_url(opt.url,
                opt.scheme,
                opt.host,
                opt.path,
                opt.port)){
        error = "invalid URL format";
        return false;
    }

    if (opt.force_ipv4 && opt.force_ipv6) {
        opt.force_ipv4 = false;
        opt.force_ipv6 = false;
    }

    return true;
}