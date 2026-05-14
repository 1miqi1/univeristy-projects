#include<iostream>
#include "protocol.hpp"
#include "args.hpp"
#include "client.hpp"
#include "logger.hpp"

int main(int argc, char **argv){
    Options opt;
    try {
        parse_args(argc, argv, opt);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Usage: my_tool -u <url> [-t timeout] [-v verbosity] [-m46q]" << std::endl;
        exit(1);
    }

    int family_pref = AF_UNSPEC;
    if(opt.force_ipv4 && !opt.force_ipv6){
        family_pref = AF_INET;
    }else if(!opt.force_ipv4 && opt.force_ipv6){
        family_pref = AF_INET6;
    }

    RadioClient r(opt.multiplex, opt.timeout_ms, family_pref, opt.scheme, opt.host, opt.path, opt.port);
    r.run();

    return 0;
}