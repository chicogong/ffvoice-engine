/**
 * @file test_logger.cpp
 * @brief Unit tests for Logger utilities
 */

#include "utils/logger.h"

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>

using namespace ffvoice;

class LoggerTest : public ::testing::Test {
protected:
    // We can't easily capture stdout/stderr in tests, so we mainly test
    // that the logging functions don't crash and handle various inputs
};

// =============================================================================
// Basic Function Tests
// =============================================================================

TEST_F(LoggerTest, LogInfo_EmptyString) {
    // Should not crash
    log_info("");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LogInfo_SimpleMessage) {
    log_info("Test message");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LogInfo_LongMessage) {
    std::string long_msg(500, 'x');
    log_info(long_msg);
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LogError_EmptyString) {
    log_error("");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LogError_SimpleMessage) {
    log_error("Test error message");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LogError_LongMessage) {
    std::string long_msg(500, 'e');
    log_error(long_msg);
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LogWarning_EmptyString) {
    log_warning("");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LogWarning_SimpleMessage) {
    log_warning("Test warning message");
    EXPECT_TRUE(true);
}

// =============================================================================
// Macro Tests
// =============================================================================

TEST_F(LoggerTest, LOG_INFO_Macro_NoArgs) {
    LOG_INFO("Simple log message");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_INFO_Macro_WithInt) {
    LOG_INFO("Value: %d", 42);
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_INFO_Macro_WithFloat) {
    LOG_INFO("Float: %.2f", 3.14);
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_INFO_Macro_WithString) {
    LOG_INFO("String: %s", "hello");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_INFO_Macro_MultipleArgs) {
    LOG_INFO("Multiple: %d, %s, %.1f", 1, "two", 3.0);
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_ERROR_Macro_NoArgs) {
    LOG_ERROR("Simple error");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_ERROR_Macro_WithArgs) {
    LOG_ERROR("Error code: %d, message: %s", 404, "not found");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_WARNING_Macro_NoArgs) {
    LOG_WARNING("Simple warning");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, LOG_WARNING_Macro_WithArgs) {
    LOG_WARNING("Warning: %s at line %d", "something fishy", 100);
    EXPECT_TRUE(true);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(LoggerTest, EdgeCase_SpecialCharacters) {
    log_info("Special chars: \t\n\\\"");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, EdgeCase_Unicode) {
    log_info("Unicode: 你好世界 🎉");
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, EdgeCase_FormatStringOnly) {
    // Format string with % but no corresponding args - might be UB in real code
    // but LOG_INFO should handle it
    LOG_INFO("100%% complete");  // %% is literal %
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, EdgeCase_VeryLongFormatString) {
    // Test buffer overflow protection
    char format[2000];
    memset(format, 'x', sizeof(format) - 1);
    format[sizeof(format) - 1] = '\0';

    log_info(format);
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, EdgeCase_RapidLogging) {
    // Rapid successive logging
    for (int i = 0; i < 100; ++i) {
        LOG_INFO("Rapid log %d", i);
    }
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, EdgeCase_MixedLogLevels) {
    LOG_INFO("Info 1");
    LOG_ERROR("Error 1");
    LOG_WARNING("Warning 1");
    LOG_INFO("Info 2");
    LOG_ERROR("Error 2");
    EXPECT_TRUE(true);
}

// =============================================================================
// Thread Safety (Basic Test)
// =============================================================================

TEST_F(LoggerTest, ThreadSafety_SingleThread) {
    // Basic single-threaded test - multi-threaded would need more setup
    for (int i = 0; i < 10; ++i) {
        LOG_INFO("Thread safety test %d", i);
        LOG_ERROR("Thread safety error %d", i);
    }
    EXPECT_TRUE(true);
}

// =============================================================================
// Stream Destination Tests — assert all log levels go to STDERR, not STDOUT
// =============================================================================

// Helper RAII guard that redirects a stream to a local ostringstream for the
// lifetime of the guard, then restores the original rdbuf on destruction.
class StreamCapture {
public:
    explicit StreamCapture(std::ostream& stream)
        : stream_(stream), original_(stream.rdbuf(captured_.rdbuf())) {
    }
    ~StreamCapture() {
        stream_.rdbuf(original_);
    }
    std::string str() const {
        return captured_.str();
    }

private:
    std::ostream& stream_;
    std::ostringstream captured_;
    std::streambuf* original_;
};

TEST_F(LoggerTest, LogInfo_GoesToStderr_NotStdout) {
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);

    log_info("info destination check");

    // Must appear on stderr
    EXPECT_NE(std::string::npos, err.str().find("info destination check"))
        << "log_info() did not write to stderr";
    // Must NOT appear on stdout
    EXPECT_EQ(std::string::npos, out.str().find("info destination check"))
        << "log_info() incorrectly wrote to stdout";
}

TEST_F(LoggerTest, LogWarning_GoesToStderr_NotStdout) {
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);

    log_warning("warning destination check");

    EXPECT_NE(std::string::npos, err.str().find("warning destination check"))
        << "log_warning() did not write to stderr";
    EXPECT_EQ(std::string::npos, out.str().find("warning destination check"))
        << "log_warning() incorrectly wrote to stdout";
}

TEST_F(LoggerTest, LogError_GoesToStderr_NotStdout) {
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);

    log_error("error destination check");

    EXPECT_NE(std::string::npos, err.str().find("error destination check"))
        << "log_error() did not write to stderr";
    EXPECT_EQ(std::string::npos, out.str().find("error destination check"))
        << "log_error() incorrectly wrote to stdout";
}

TEST_F(LoggerTest, AllLevels_GoesToStderr_WithPrefixes) {
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);

    log_info("prefix_info");
    log_warning("prefix_warning");
    log_error("prefix_error");

    const std::string captured = err.str();

    // Each message should appear with the right prefix
    EXPECT_NE(std::string::npos, captured.find("[INFO]"))
        << "log_info() prefix [INFO] missing from stderr";
    EXPECT_NE(std::string::npos, captured.find("[WARNING]"))
        << "log_warning() prefix [WARNING] missing from stderr";
    EXPECT_NE(std::string::npos, captured.find("[ERROR]"))
        << "log_error() prefix [ERROR] missing from stderr";

    // stdout must be completely empty
    EXPECT_TRUE(out.str().empty()) << "Unexpected stdout output: " << out.str();
}
