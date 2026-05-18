# cmake/Dependencies.cmake
#
# Centralized dependency management. Uses FetchContent (built into CMake) to
# pin and fetch all third-party libraries. This avoids requiring users to set
# up vcpkg or system-wide installs.
#
# Override behavior:
#   - Set NEURAL_ECO_USE_SYSTEM_DEPS=ON to prefer find_package() over fetch.
#   - Set NEURAL_ECO_OFFLINE=ON to fail loudly if FetchContent needs network.

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ----------------------------------------------------------------------------
# tomlplusplus - TOML configuration parser (header-only)
# ----------------------------------------------------------------------------
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# ----------------------------------------------------------------------------
# spdlog - logging
# ----------------------------------------------------------------------------
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# ----------------------------------------------------------------------------
# Google Test - unit testing
# ----------------------------------------------------------------------------
if(NEURAL_ECO_BUILD_TESTS)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

# ----------------------------------------------------------------------------
# raylib - rendering (only needed for viz)
# ----------------------------------------------------------------------------
if(NEURAL_ECO_BUILD_VIZ)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)  # we don't need audio
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

message(STATUS "Dependencies fetched:")
message(STATUS "  tomlplusplus  : ${tomlplusplus_SOURCE_DIR}")
message(STATUS "  spdlog        : ${spdlog_SOURCE_DIR}")
if(NEURAL_ECO_BUILD_TESTS)
    message(STATUS "  googletest    : ${googletest_SOURCE_DIR}")
endif()
if(NEURAL_ECO_BUILD_VIZ)
    message(STATUS "  raylib        : ${raylib_SOURCE_DIR}")
endif()
