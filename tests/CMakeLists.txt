find_package(GTest REQUIRED)

add_executable(carl_tests
    test_utils.cpp
    test_config_value.cpp
    test_config_group.cpp
    test_config_map.cpp
    test_parsing_extensions.cpp
    test_integration.cpp
)

target_link_libraries(carl_tests PRIVATE
    carl::carl
    GTest::gtest_main
)

target_compile_features(carl_tests PRIVATE cxx_std_17)

include(GoogleTest)
gtest_discover_tests(carl_tests)
