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

[[noreturn]] void syserr(const char* fmt, ...) {
    if (g_level < 2) {
        std::exit(1);
    }

    int saved_errno = errno;

    std::fprintf(stderr, "ERROR: ");

    va_list ap;
    va_start(ap, fmt);
    vprint(fmt, ap);
    va_end(ap);

    std::fprintf(stderr,
                 " (%d; %s)\n",
                 saved_errno,
                 std::strerror(saved_errno));

    std::exit(1);
}

[[noreturn]] void fatal(const char* fmt, ...) {
    if (g_level < 2) {
        std::exit(1);
    }

    std::fprintf(stderr, "ERROR: ");

    va_list ap;
    va_start(ap, fmt);
    vprint(fmt, ap);
    va_end(ap);

    std::exit(1);
}