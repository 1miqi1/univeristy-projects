#pragma once

#include <cstdarg>

/**
 * @brief Represents the severity level of a log message.
 */
enum class LogLevel {
    INFO  = 1,
    ERROR = 2,
    WARN  = 3,
    DEBUG = 4
};

/**
 * @brief Sets the global verbosity level for the logger.
 * 
 * @param v The maximum LogLevel to output (e.g., 4 for DEBUG).
 */
void log_set_level(int v);

/**
 * @brief Writes a formatted log message to stderr if the level allows it.
 * 
 * @param lvl The severity of the log message.
 * @param fmt The format string (printf style).
 * @param ... Variable arguments matching the format string.
 */
void log_msg(LogLevel lvl, const char *fmt, ...);

// Helper Macros for easy logging
#define LOGE(...) log_msg(LogLevel::ERROR, __VA_ARGS__)
#define LOGW(...) log_msg(LogLevel::WARN,  __VA_ARGS__)
#define LOGI(...) log_msg(LogLevel::INFO,  __VA_ARGS__)
#define LOGD(...) log_msg(LogLevel::DEBUG, __VA_ARGS__)