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
    # Apply host-compiler flags only to CXX/C source files. nvcc must not
    # see /W4 /utf-8 /Zc directly: nvcc parses anything starting with '/'
    # as a Windows-style path/input file, which causes
    #   "nvcc fatal: A single input file is required for a non-link phase".
    # The CUDA host-compiler flags can be wired separately via
    # CMAKE_CUDA_FLAGS or per-target -Xcompiler= if we ever need them.
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:C>:/W4>
        $<$<COMPILE_LANGUAGE:C>:/utf-8>
    )
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608 /MAP")
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
option(NEURAL_ECO_ENABLE_PROTAGONIST_CUDA "Enable CUDA mixed backend for protagonist route" ON)

include(CheckLanguage)
if(NEURAL_ECO_ENABLE_PROTAGONIST_CUDA)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        find_package(CUDAToolkit QUIET)
    endif()
endif()

if(CMAKE_CUDA_COMPILER AND CUDAToolkit_FOUND)
    set(NEURAL_ECO_HAS_PROTAGONIST_CUDA ON)
else()
    set(NEURAL_ECO_HAS_PROTAGONIST_CUDA OFF)
endif()

# ============================================================================
# Dependencies
# ============================================================================
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Dependencies)

# ============================================================================
# Subdirectories
# ============================================================================
add_subdirectory(src)
add_subdirectory(routes/protagonist_ecology)

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
message(STATUS "  Protagonist CUDA  : ${NEURAL_ECO_HAS_PROTAGONIST_CUDA}")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "")
