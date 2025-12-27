/**
 * @file test_main.cpp
 * @brief Main entry point for Google Test framework
 *
 * This file initializes the Google Test framework and configures
 * global test environment settings for ffvoice-engine tests.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

/**
 * @class GlobalTestEnvironment
 * @brief Global test environment for setup/teardown operations
 *
 * This environment is set up once before all tests run and
 * torn down after all tests complete.
 */
class GlobalTestEnvironment : public ::testing::Environment {
public:
    /**
     * @brief Set up global test environment
     *
     * Performs one-time initialization:
     * - Sets up logging
     * - Initializes test data directories
     * - Configures test environment variables
     */
    void SetUp() override {
        std::cout << "=== Setting up global test environment ===" << std::endl;

// Set environment variables for testing
#ifdef _WIN32
        _putenv_s("FFVOICE_TEST_MODE", "1");
#else
        setenv("FFVOICE_TEST_MODE", "1", 1);
#endif

// Disable audio device initialization in test mode
#ifdef _WIN32
        _putenv_s("FFVOICE_DISABLE_AUDIO", "1");
#else
        setenv("FFVOICE_DISABLE_AUDIO", "1", 1);
#endif

        std::cout << "Test mode enabled" << std::endl;
        std::cout << "Audio devices disabled for testing" << std::endl;
    }

    /**
     * @brief Tear down global test environment
     *
     * Performs cleanup after all tests:
     * - Cleans up temporary test files
     * - Resets environment variables
     */
    void TearDown() override {
        std::cout << "=== Tearing down global test environment ===" << std::endl;

        // Cleanup is handled by individual test fixtures
    }
};

/**
 * @brief Custom test event listener for verbose test output
 */
class VerboseTestEventListener : public ::testing::EmptyTestEventListener {
private:
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        std::cout << std::endl;
        std::cout << "[ RUN      ] " << test_info.test_suite_name() << "." << test_info.name()
                  << std::endl;
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        if (test_info.result()->Passed()) {
            std::cout << "[       OK ] " << test_info.test_suite_name() << "." << test_info.name()
                      << " (" << test_info.result()->elapsed_time() << " ms)" << std::endl;
        } else {
            std::cout << "[  FAILED  ] " << test_info.test_suite_name() << "." << test_info.name()
                      << " (" << test_info.result()->elapsed_time() << " ms)" << std::endl;
        }
    }
};

/**
 * @brief Main entry point for test executable
 *
 * Initializes Google Test and Google Mock frameworks, configures
 * test environment, and runs all registered tests.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Test result status (0 for success, non-zero for failure)
 */
int main(int argc, char** argv) {
    // Print banner
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  FFVoice Engine Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Initialize Google Mock
    ::testing::InitGoogleMock(&argc, argv);

    // Add global test environment
    ::testing::AddGlobalTestEnvironment(new GlobalTestEnvironment);

    // Configure test output
    if (argc > 1 && std::string(argv[1]) == "--verbose") {
        ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
        listeners.Append(new VerboseTestEventListener);
    }

    // Set default death test style to threadsafe
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    // Run all tests
    int result = RUN_ALL_TESTS();

    // Print summary
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    if (result == 0) {
        std::cout << "  All tests PASSED" << std::endl;
    } else {
        std::cout << "  Some tests FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    return result;
}
