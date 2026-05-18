# src/CMakeLists.txt
# Aggregate of all source modules.

add_subdirectory(core)

if(NEURAL_ECO_BUILD_VIZ)
    add_subdirectory(viz)
endif()

add_subdirectory(runtime)

# Top-level executable. CLI parses --mode and dispatches.
add_executable(neural-eco main.cpp)
target_link_libraries(neural-eco
    PRIVATE
        neural_eco::core
        neural_eco::runtime_headless
)
if(NEURAL_ECO_BUILD_VIZ)
    target_link_libraries(neural-eco PRIVATE neural_eco::runtime_interactive)
    target_compile_definitions(neural-eco PRIVATE NEURAL_ECO_HAS_VIZ=1)
endif()

set_target_properties(neural-eco PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

