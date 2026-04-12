#ifndef ERR_HPP
#define ERR_HPP

#include <cstdio>
#include <cstdarg>

[[noreturn]] void syserr(const char* fmt, ...);

[[noreturn]] void fatal(const char* fmt, ...);

#endif 