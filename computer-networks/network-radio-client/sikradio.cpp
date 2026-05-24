#include <iostream>
#include "http.hpp"
#include "args.hpp"
#include "client.hpp"
#include "logger.hpp"

int main(int argc, char **argv) {
    Options opt;

    // Parse command line arguments and handle errors
    try {
        parse_args(argc, argv, opt);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        std::fprintf(stderr,
                    "Usage: my_tool -u <url> [-t timeout] [-v verbosity] [-m46q]\n");
        std::exit(1);
    }

    // Configure logging verbosity
    log_set_level(opt.verbosity);
    if (opt.verbosity == (int)LogLevel::DEBUG) {
        print_options(opt);
    }

    // Determine network address family preference based on flags
    int family_pref = AF_UNSPEC;
    if (opt.force_ipv4 && !opt.force_ipv6) {
        family_pref = AF_INET;
    } else if (!opt.force_ipv4 && opt.force_ipv6) {
        family_pref = AF_INET6;
    }

    // Initialize the RadioClient with parsed configurations
    RadioClient r(opt.multiplex, opt.timeout_ms, family_pref, opt.scheme, opt.host, opt.path, opt.port, opt.url);
    
    // Begin the main execution loop
    r.run();

    return 0;
}