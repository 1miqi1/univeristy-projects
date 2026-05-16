#include "io.hpp"
#include "logger.hpp"
#include "exceptions.hpp" // Added your exceptions header

#include <unistd.h>
#include <cstdint>
#include <string>
#include <iostream>
#include <cerrno> // Required for errno

void handle_audio(const char* data, size_t len) {
    if (!data || len == 0) return;

    size_t bytes_written = 0;
    
    
    while (bytes_written < len) {
        // Attempt to write the remaining portion of the buffer
        ssize_t result = write(STDOUT_FILENO, data + bytes_written, len - bytes_written);
        
        if (result > 0) {
            bytes_written += (size_t)result;
            continue;
        }
        
        if (result == -1) {
            // If interrupted by an OS signal, just try again
            if (errno == EINTR) continue; 
            
            // If the pipe is full and somehow set to non-blocking
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000); // Sleep 1 millisecond to let 'play' catch up
                continue;
            }
            
            // It's a fatal pipe error
            throw NetworkError(errno, "write audio failed");
        }
    }
}


// Updated to take std::string directly
void handle_metadata(std::string& meta){
    if (meta.empty()) return;

    // Strip out the trailing null padding required by the ICY protocol
    if (auto pos = meta.find('\0'); pos != std::string::npos) {
        meta.resize(pos);
    }

    // If it was all padding, don't print a blank line
    if (meta.empty()) return;

    // Add the newline character so it prints nicely in the terminal
    meta.push_back('\n');

    size_t written = 0;
    while (written < meta.size()) {
        ssize_t n = write(STDERR_FILENO, meta.data() + written, meta.size() - written);

        if (n > 0) {
            written += (size_t)n;
            continue;
        }

        if (n == -1) {
            if (errno == EINTR) continue;
            
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); 
                continue;
            }

            throw NetworkError(errno, "write metadata failed");
        }
    }
}