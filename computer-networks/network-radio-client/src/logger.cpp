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

static void vprint(const char *fmt, va_list ap) {
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
}

void log_msg(LogLevel lvl, const char *fmt, ...) {
    if ((int)lvl > g_level) return;

    if (lvl == LogLevel::WARN) {
        std::fprintf(stderr, "WARN: ");
    }
    else if (lvl == LogLevel::DEBUG) {
        std::fprintf(stderr, "DEBUG: ");
    }

    va_list ap;
    va_start(ap, fmt);
    vprint(fmt, ap);
    va_end(ap);
}
