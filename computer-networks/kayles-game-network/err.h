#ifndef ERR_HPP
#define ERR_HPP

#include <cstdio>
#include <cstdarg>

/**
 * Prints a system error message and terminates the program.
 *
 * This function formats the given message, appends information
 * related to the current errno value, and exits the program.
 *
 * @param fmt  Format string (printf-style)
 * @param ...  Additional arguments for formatting
 */
[[noreturn]] void syserr(const char* fmt, ...);

/**
 * Prints an error message and terminates the program.
 *
 * This function formats the given message and exits the program
 * without including system error details.
 *
 * @param fmt  Format string (printf-style)
 * @param ...  Additional arguments for formatting
 */
[[noreturn]] void fatal(const char* fmt, ...);

#endif