/**
 * @file logger.cpp
 * @brief Logging implementation
 */

#include "utils/logger.h"

#include <iostream>

namespace ffvoice {

void log_info(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void log_error(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

}  // namespace ffvoice
