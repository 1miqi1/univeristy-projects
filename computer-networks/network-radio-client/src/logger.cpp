#include "logger.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cerrno>
#include <cstring>

static int g_level = 2;

void log_set_level(int v) {
    g_level = v;
}


void log_msg(LogLevel lvl, const char *fmt, ...) {
    if ((int)lvl > g_level) return;

    switch (lvl) {
        case LogLevel::DEBUG:
            std::fprintf(stderr, "[DEBUG]: ");
            break;
        case LogLevel::WARN:
            std::fprintf(stderr, "[WARN ]: ");
            break;
        case LogLevel::ERROR:
            std::fprintf(stderr, "[ERROR]: ");
            break;
        default:
            break;
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    std::fprintf(stderr, "\n");
}
