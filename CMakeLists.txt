cmake_minimum_required(VERSION 3.25)
project(neural-eco
    VERSION 0.1.0
    DESCRIPTION "Neural Evolution Ecosystem"
    LANGUAGES CXX
)

# ============================================================================
# Global settings
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8 /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# Options
# ============================================================================
option(NEURAL_ECO_BUILD_TESTS "Build unit tests" ON)
option(NEURAL_ECO_BUILD_VIZ "Build visualization (interactive runtime)" ON)

# ============================================================================
# Dependencies
# ============================================================================
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Dependencies)

# ============================================================================
# Subdirectories
# ============================================================================
add_subdirectory(src)

if(NEURAL_ECO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "Neural Eco ${PROJECT_VERSION}")
message(STATUS "  C++ standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests       : ${NEURAL_ECO_BUILD_TESTS}")
message(STATUS "  Build viz         : ${NEURAL_ECO_BUILD_VIZ}")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "")
