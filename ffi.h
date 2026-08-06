#pragma once

#include <cstdint>
#include <string>

namespace pixelblaze_cpp {

//#define ENABLE_DUMP 0

typedef enum : uint8_t {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_SCRIPT,
    LOG_LEVEL_NONE
} LogLevel;

void log_print(uint8_t level, const char *fmt, ...);

#define LOG_DEBUG(...)  log_print(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)   log_print(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(...)   log_print(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(...)  log_print(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_SCRIPT(...) log_print(LOG_LEVEL_SCRIPT, __VA_ARGS__)



}  // namespace pixelblaze_cpp