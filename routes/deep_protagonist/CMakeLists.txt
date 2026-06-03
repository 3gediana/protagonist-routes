cmake_minimum_required(VERSION 3.25)
project(deep_protagonist
    VERSION 0.0.1
    DESCRIPTION "Single super-agent trained with PPO in a real 3D world"
    LANGUAGES CXX
)

# ============================================================================
# Global settings
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    )
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# libtorch
# ============================================================================
set(LIBTORCH_DIR "${CMAKE_SOURCE_DIR}/third_party/libtorch")
if(NOT EXISTS "${LIBTORCH_DIR}/share/cmake/Torch/TorchConfig.cmake")
    message(FATAL_ERROR
        "libtorch not found at ${LIBTORCH_DIR}.\n"
        "Run scripts/download_libtorch.ps1 first (downloads ~2.65GB).")
endif()
list(APPEND CMAKE_PREFIX_PATH "${LIBTORCH_DIR}")
find_package(Torch REQUIRED)
message(STATUS "Found Torch: ${TORCH_LIBRARIES}")

# ============================================================================
# Third-party (FetchContent) - tomlplusplus + spdlog
# ============================================================================
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
# raylib for 3D rendering. Disable audio (we don't need it) and examples/games.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tomlplusplus spdlog raylib)

# ============================================================================
# Shared core sources (used by both viewer and trainer)
# ============================================================================
set(DP_CORE_SOURCES
    src/world/Heightmap.cpp
    src/world/BiomeMap.cpp
    src/world/RiverSystem.cpp
    src/world/CaveSystem.cpp
    src/world/World.cpp
    src/world/Physics.cpp
    src/world/FoodSystem.cpp
    src/world/Decorations.cpp
    src/world/Daylight.cpp
    src/world/PlantSystem.cpp
    src/world/AnimalSystem.cpp
    src/world/ResourceSystem.cpp
    src/world/BuildingSystem.cpp
    src/world/FireSystem.cpp
    src/world/WeatherSystem.cpp
    src/world/FarmingSystem.cpp
    src/agent/CapsuleAgent.cpp
    src/agent/EpisodeManager.cpp
    src/agent/Inventory.cpp
    src/agent/Observation.cpp
    src/agent/Perception.cpp
    src/agent/ProgressTracker.cpp
    src/agent/RewardSystem.cpp
    src/agent/Shelter.cpp
    src/agent/SpatialMemory.cpp
    src/agent/TameSystem.cpp
    src/agent/VitalSystem.cpp
    src/agent/Wardrobe.cpp
    src/env/Environment.cpp
    src/env/VecEnv.cpp
    src/policy/PPO.cpp
)

# ============================================================================
# Main executable (raylib viewer + self-tests)
# ============================================================================
add_executable(deep_protagonist
    src/main.cpp
    ${DP_CORE_SOURCES}
    src/render/WorldRenderer.cpp
    src/render/FlyCamera.cpp
)
target_include_directories(deep_protagonist PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
# Inject absolute asset path so WorldRenderer can find Quaternius GLB
# files regardless of where the .exe is run from.
target_compile_definitions(deep_protagonist PRIVATE
    DP_ASSET_DIR="${CMAKE_SOURCE_DIR}/third_party/assets"
)
target_link_libraries(deep_protagonist PRIVATE
    ${TORCH_LIBRARIES}
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    raylib
)
target_compile_features(deep_protagonist PRIVATE cxx_std_20)

# raylib on Windows brings in winmm/gdi32 etc - already handled by raylib's CMake

# On Windows: copy libtorch DLLs next to the .exe so it can run
if(MSVC)
    file(GLOB TORCH_DLLS "${LIBTORCH_DIR}/lib/*.dll")
    add_custom_command(TARGET deep_protagonist POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TORCH_DLLS}
        $<TARGET_FILE_DIR:deep_protagonist>)
endif()

# ============================================================================
# Headless trainer executable (no raylib, drives Environment directly)
# ============================================================================
add_executable(deep_protagonist_train
    src/main_train.cpp
    ${DP_CORE_SOURCES}
)
target_include_directories(deep_protagonist_train PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(deep_protagonist_train PRIVATE
    ${TORCH_LIBRARIES}
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    raylib  # headers only - we never call InitWindow() so no graphics
            # context is created at runtime; needed because Daylight.hpp
            # exposes Color in its public API.
)
target_compile_features(deep_protagonist_train PRIVATE cxx_std_20)

# Same DLL drop next to the trainer .exe so it can find libtorch dlls.
if(MSVC)
    add_custom_command(TARGET deep_protagonist_train POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TORCH_DLLS}
        $<TARGET_FILE_DIR:deep_protagonist_train>)
endif()

# ============================================================================
# Replay scene dumper (VIEWER SUPPORT - headless, emits world.json + frames.json)
# ============================================================================
add_executable(deep_protagonist_dump
    tools/scene_dump.cpp
    ${DP_CORE_SOURCES}
)
target_include_directories(deep_protagonist_dump PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(deep_protagonist_dump PRIVATE
    ${TORCH_LIBRARIES}
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    raylib
)
target_compile_features(deep_protagonist_dump PRIVATE cxx_std_20)
if(MSVC)
    add_custom_command(TARGET deep_protagonist_dump POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TORCH_DLLS}
        $<TARGET_FILE_DIR:deep_protagonist_dump>)
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "deep_protagonist ${PROJECT_VERSION}")
message(STATUS "  C++ standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  libtorch          : ${LIBTORCH_DIR}")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "")
