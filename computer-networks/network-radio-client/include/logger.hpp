#pragma once

#include <cstdarg>

enum class LogLevel {
    ERROR = 2,
    WARN  = 3,
    INFO  = 1,
    DEBUG = 0
};

void log_set_level(int v);

void log_msg(LogLevel lvl, const char *fmt, ...);


#define LOGE(...) log_msg(LogLevel::ERROR, __VA_ARGS__)

#define LOGW(...) log_msg(LogLevel::WARN, __VA_ARGS__)

#define LOGI(...) log_msg(LogLevel::INFO, __VA_ARGS__)

#define LOGD(...) log_msg(LogLevel::DEBUG, __VA_ARGS__)