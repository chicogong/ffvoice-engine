/**
 * @file logger.h
 * @brief Simple logging utilities
 */

#pragma once

#include <cstdio>
#include <string>

namespace ffvoice {

void log_info(const std::string& message);
void log_error(const std::string& message);

}  // namespace ffvoice

// Printf-style logging macros
#define LOG_INFO(fmt, ...)                              \
    do {                                                \
        char buf[1024];                                 \
        snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
        ffvoice::log_info(buf);                         \
    } while (0)

#define LOG_ERROR(fmt, ...)                             \
    do {                                                \
        char buf[1024];                                 \
        snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
        ffvoice::log_error(buf);                        \
    } while (0)

#define LOG_WARNING(fmt, ...)                                        \
    do {                                                             \
        char buf[1024];                                              \
        snprintf(buf, sizeof(buf), "[WARNING] " fmt, ##__VA_ARGS__); \
        ffvoice::log_info(buf);                                      \
    } while (0)
