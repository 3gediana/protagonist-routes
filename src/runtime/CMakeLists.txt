# src/runtime/CMakeLists.txt
add_subdirectory(headless)

if(NEURAL_ECO_BUILD_VIZ)
    add_subdirectory(interactive)
endif()
