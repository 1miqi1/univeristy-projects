#include<iostream>
#include "protocol.hpp"
#include "args.hpp"
#include "client.hpp"

int main(){
    Options opt;
    try {
        parse_args(argc, argv, opt);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Usage: my_tool -u <url> [-t timeout] [-v verbosity] [-m46q]" << std::endl;
        return 1;
    }

    
}