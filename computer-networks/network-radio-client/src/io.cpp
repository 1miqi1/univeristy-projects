#include "io.hpp"
#include "logger.hpp"
#include "exceptions.hpp"

#include <unistd.h>
#include <cstdint>
#include <string>
#include <iostream>
#include <cerrno>
#include <stdexcept>

void handle_audio(const char* data, size_t len) {
    if (!data || len == 0) return;

    size_t bytes_written = 0;
    
    while (bytes_written < len) {
        ssize_t result = write(STDOUT_FILENO, data + bytes_written, len - bytes_written);
        
        if (result > 0) {
            bytes_written += static_cast<size_t>(result);
        } else if (result < 0) {
            if (errno == EINTR) {
                // Interrupted by a signal, safe to retry
                continue;
            }
            // The stream was destroyed (EPIPE) or another fatal error occurred
            throw std::runtime_error("cant print data"); 
        }
    }
}

void handle_metadata(const std::string& meta) {
    if (meta.empty()) return;

    size_t written = 0;

    while (written < meta.size()) {
        ssize_t n = write(
            STDERR_FILENO,
            meta.data() + written,
            meta.size() - written
        );

        if (n > 0) {
            written += static_cast<size_t>(n);
        } else if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("cant print data");
        }
    }
}