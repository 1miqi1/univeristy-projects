#ifndef ERR_H
#define ERR_H

#include <stdnoreturn.h>     // C11

// Print information about a system error and quits.
noreturn void syserr(const char* fmt, ...);

// Print information about an error and quits.
noreturn void fatal(const char* fmt, ...);


#endif