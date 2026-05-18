/**
 * @file logger.h
 * @brief Simple logging utilities
 *
 * Unified logging API: use the LOG_INFO / LOG_ERROR / LOG_WARNING macros
 * throughout the codebase. They accept printf-style format strings and any
 * number of arguments. The underlying free functions log_info / log_error /
 * log_warning are available for callers that already hold a std::string.
 *
 * Output destinations:
 *   LOG_INFO    -> stdout
 *   LOG_WARNING -> stderr
 *   LOG_ERROR   -> stderr
 */

#pragma once

#include <cstdio>
#include <string>

namespace ffvoice {

void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);

}  // namespace ffvoice

// Printf-style logging macros
#define LOG_INFO(fmt, ...)                              \
    do {                                                \
        char buf[1024];                                 \
        snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
        ffvoice::log_info(buf);                         \
    } while (0)

#define LOG_WARNING(fmt, ...)                           \
    do {                                                \
        char buf[1024];                                 \
        snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
        ffvoice::log_warning(buf);                      \
    } while (0)

#define LOG_ERROR(fmt, ...)                             \
    do {                                                \
        char buf[1024];                                 \
        snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
        ffvoice::log_error(buf);                        \
    } while (0)
