# CMake Testing Helpers for libaeon
# This file provides utility functions for testing

# Function to add a test executable with standard configuration
function(add_aeon_test TEST_NAME TEST_SOURCE_FILES)
    add_executable(${TEST_NAME} ${TEST_SOURCE_FILES})
    
    # Include directories
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    
    # Link against static library and Google Test
    target_link_libraries(${TEST_NAME} PRIVATE aeon-static gtest_main)
    
    # Set C++ standard
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Platform-specific settings
    if(WIN32)
        target_link_libraries(${TEST_NAME} PRIVATE ws2_32)
    endif()
    
    # Register with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Function to enable test output on failure
function(enable_verbose_testing)
    # Run tests with verbose output
    set(CMAKE_CTEST_COMMAND ctest --output-on-failure -V)
endfunction()

# Function to create a test with custom timeout
function(add_aeon_test_with_timeout TEST_NAME TIMEOUT TEST_SOURCE_FILES)
    add_aeon_test(${TEST_NAME} "${TEST_SOURCE_FILES}")
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT ${TIMEOUT})
endfunction()

# Macro to print test summary
macro(print_test_summary)
    message(STATUS "")
    message(STATUS "libaeon Testing Configuration")
    message(STATUS "=============================")
    message(STATUS "Testing Framework: Google Test (gtest)")
    message(STATUS "Test Runner: CTest")
    message(STATUS "Build test target with: cmake --build . --target aeon_tests")
    message(STATUS "Run tests with: ctest --output-on-failure")
    message(STATUS "")
endmacro()
