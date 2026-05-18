# Interactive runtime: with viz. Used for demos and live debugging.
file(GLOB INTERACTIVE_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(neural_eco_runtime_interactive STATIC ${INTERACTIVE_SOURCES})
add_library(neural_eco::runtime_interactive ALIAS neural_eco_runtime_interactive)

target_link_libraries(neural_eco_runtime_interactive
    PUBLIC
        neural_eco::core
        # neural_eco::viz       # added in M2
        # raylib                # added in M2
)
target_compile_features(neural_eco_runtime_interactive PUBLIC cxx_std_20)
