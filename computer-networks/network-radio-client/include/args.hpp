#pragma once

#include <cstdint>
#include <string>

struct Options {
    std::string url;
    bool multiplex = false;
    int timeout_ms = 5000;
    bool force_ipv4 = false;
    bool force_ipv6 = false;
    int verbosity = 2;

    std::string scheme;
    std::string host;
    std::string path;
    uint16_t port = 0;
};

void print_options(const Options &opt);

void parse_args(int argc, char **argv, Options &opt);

void parse_url(const std::string& url,
               std::string& scheme,
               std::string& host,
               std::string& path,
               uint16_t& port);