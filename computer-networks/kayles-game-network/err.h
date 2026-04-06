#ifndef ERR_H
#define ERR_H

#ifdef __cplusplus
#include <cstddef>            // for [[noreturn]] in C++
#define NORETURN [[noreturn]] 
extern "C" {
#else
#include <stdnoreturn.h>     // C11
#define NORETURN noreturn
#endif

// Print information about a system error and quits.
NORETURN void syserr(const char* fmt, ...);

// Print information about an error and quits.
NORETURN void fatal(const char* fmt, ...);

#ifdef __cplusplus
} // extern "C"
#endif

#endif