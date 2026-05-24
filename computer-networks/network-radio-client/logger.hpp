#pragma once

#include <cstdarg>

/**
 * @brief Represents the severity level of a log message.
 * 
 * Levels are typically ordered by verbosity. For example, setting the 
 * logger to DEBUG (4) will print everything, whereas ERROR (2) might 
 * only print critical failures.
 */
enum class LogLevel {
    INFO  = 1, // Standard operational messages (e.g., "Connecting to server...")
    ERROR = 2, // Critical issues that cause a failure or abort
    WARN  = 3, // Non-fatal issues or unexpected behaviors (e.g., "Retrying connection")
    DEBUG = 4  // Highly detailed diagnostic information for development
};

/**
 * @brief Sets the global verbosity level for the logger.
 * 
 * Any log messages with a severity level strictly greater than 'v' 
 * will be silently ignored.
 * 
 * @param v The maximum LogLevel to output (e.g., 4 for DEBUG).
 */
void log_set_level(int v);

/**
 * @brief Writes a formatted log message to the console if the level allows it.
 * 
 * @param lvl The severity of the log message.
 * @param fmt The format string (printf style).
 * @param ... Variable arguments matching the format string.
 */
void log_msg(LogLevel lvl, const char* fmt, ...);

/* 
 * =========================================================================
 * Helper Macros for easy logging 
 * =========================================================================
 * Use these macros throughout the codebase instead of calling log_msg 
 * directly. They keep your code cleaner and easier to read.
 */
#define LOGE(...) log_msg(LogLevel::ERROR, __VA_ARGS__)
#define LOGW(...) log_msg(LogLevel::WARN,  __VA_ARGS__)
#define LOGI(...) log_msg(LogLevel::INFO,  __VA_ARGS__)
#define LOGD(...) log_msg(LogLevel::DEBUG, __VA_ARGS__)