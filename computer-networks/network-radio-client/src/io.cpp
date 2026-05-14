#include "io.hpp"
#include "logger.hpp"
#include "exceptions.hpp" // Added your exceptions header

#include <unistd.h>
#include <cstdint>
#include <string>
#include <iostream>
#include <cerrno> // Required for errno

void handle_audio(const char* buffer, size_t len) {
    if (!buffer || len == 0) return;

    size_t written = 0;

    while (written < len) {
        ssize_t n = write(STDOUT_FILENO, buffer + written, len - written);

        if (n > 0) {
            written += (size_t)n;
            continue;
        }

        if (n == -1 && errno == EINTR) {
            continue; // retry
        }

        // Replaced syserr with NetworkError
        throw NetworkError(errno, "write audio failed");
    }
}



void handle_metadata(const char* data, size_t len) {
    if (!data || len <= 1) return;

    size_t meta_len = (size_t)data[0] * 16;

    if (meta_len == 0) return;

    if (meta_len > len - 1) {
        meta_len = len - 1;
    }

    std::string meta(reinterpret_cast<const char*>(data + 1), meta_len);

    if (auto pos = meta.find('\0'); pos != std::string::npos) {
        meta.resize(pos);
    }

    meta.push_back('\n');

    size_t written = 0;
    while (written < meta.size()) {
        ssize_t n = write(STDERR_FILENO, meta.data() + written, meta.size() - written);

        if (n > 0) {
            written += (size_t)n;
            continue;
        }

        if (n == -1 && errno == EINTR) {
            continue;
        }

        // Replaced syserr with NetworkError
        throw NetworkError(errno, "write metadata failed");
    }
}