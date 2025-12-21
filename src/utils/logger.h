/**
 * @file logger.h
 * @brief Simple logging utilities
 */

#pragma once

#include <string>

namespace ffvoice {

void log_info(const std::string& message);
void log_error(const std::string& message);

} // namespace ffvoice
