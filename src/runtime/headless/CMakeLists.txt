# Headless runtime: training, no rendering.
file(GLOB HEADLESS_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(neural_eco_runtime_headless STATIC ${HEADLESS_SOURCES})
add_library(neural_eco::runtime_headless ALIAS neural_eco_runtime_headless)

target_link_libraries(neural_eco_runtime_headless
    PUBLIC neural_eco::core
)
target_compile_features(neural_eco_runtime_headless PUBLIC cxx_std_20)
