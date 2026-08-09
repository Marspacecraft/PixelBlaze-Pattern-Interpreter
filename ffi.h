#pragma once

#include <cstdint>
#include <string>

namespace pixelblaze_cpp {

//#define ENABLE_DUMP 0

typedef enum : uint8_t {
    PBZ_LOG_LEVEL_DEBUG = 0,
    PBZ_LOG_LEVEL_INFO,
    PBZ_LOG_LEVEL_WARN,
    PBZ_LOG_LEVEL_ERROR,
    PBZ_LOG_LEVEL_SCRIPT,
    PBZ_LOG_LEVEL_NONE
} LogLevel;

void pbz_log_print(uint8_t level, const char *fmt, ...);

#define PBZ_DEBUG(...)  pbz_log_print(PBZ_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define PBZ_INFO(...)   pbz_log_print(PBZ_LOG_LEVEL_INFO, __VA_ARGS__)
#define PBZ_WARN(...)   pbz_log_print(PBZ_LOG_LEVEL_WARN, __VA_ARGS__)
#define PBZ_ERROR(...)  pbz_log_print(PBZ_LOG_LEVEL_ERROR, __VA_ARGS__)
#define PBZ_SCRIPT(...) pbz_log_print(PBZ_LOG_LEVEL_SCRIPT, __VA_ARGS__)



}  // namespace pixelblaze_cpp