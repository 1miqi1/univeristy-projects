#include "io.hpp"
#include "logger.hpp"

#include <unistd.h>
#include <cstdint>
#include <string>
#include <iostream>
#include <string>

void handle_audio(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    size_t written = 0;

    while (written < len) {
        ssize_t n = write(STDOUT_FILENO, data + written, len - written);

        if (n > 0) {
            written += (size_t)n;
            continue;
        }

        if (n == -1 && errno == EINTR) {
            continue; // retry
        }

        syserr("write audio failed");
        return;
    }
}

int read_user_line(char* buffer, size_t size) {
    ssize_t n = read(STDIN_FILENO, buffer, size);

    if (n > 0) {
        return (int)n; 
    }

    if (n == 0) {
        LOGW("stdin closed (EOF)");
        return 0; 
    }

    if (errno == EINTR) {
        return -1; 
    }

    syserr("stdin error");
    return -2; 
}

void handle_metadata(const uint8_t* data, size_t len) {
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

        syserr("write metadata failed");
    }
}