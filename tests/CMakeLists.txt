# tests/CMakeLists.txt
include(GoogleTest)

# Collect all test sources via glob.
file(GLOB_RECURSE NEURAL_ECO_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

if(NOT NEURAL_ECO_TEST_SOURCES)
    return()
endif()

add_executable(test_core ${NEURAL_ECO_TEST_SOURCES})

target_link_libraries(test_core
    PRIVATE
        neural_eco::core
        neural_eco_runtime_headless
        neural_eco::route_protagonist
        GTest::gtest_main
)

target_compile_features(test_core PRIVATE cxx_std_20)

gtest_discover_tests(test_core
    DISCOVERY_TIMEOUT 60
)
