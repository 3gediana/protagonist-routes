# src/core/CMakeLists.txt
#
# core: Pure logic. No rendering, no runtime concerns.
# Defines the neural_eco_core STATIC library.

# Collect sources via glob; CONFIGURE_DEPENDS triggers CMake re-run on add/del.
file(GLOB_RECURSE NEURAL_ECO_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB_RECURSE NEURAL_ECO_CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(neural_eco_core STATIC
    ${NEURAL_ECO_CORE_SOURCES}
    ${NEURAL_ECO_CORE_HEADERS}
)
add_library(neural_eco::core ALIAS neural_eco_core)

target_include_directories(neural_eco_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
)

target_link_libraries(neural_eco_core
    PUBLIC
        tomlplusplus::tomlplusplus
        spdlog::spdlog
)

target_compile_features(neural_eco_core PUBLIC cxx_std_20)

# Architecture rule: core must NEVER include viz or runtime headers.
# We don't enforce this with CMake (would require complex include-what-you-use
# plumbing), but it is documented in src/core/README.md and AGENTS.md.

