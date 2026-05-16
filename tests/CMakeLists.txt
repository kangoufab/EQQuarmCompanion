add_executable(eq_tests
    test_config.cpp
    test_npc_analysis.cpp
    test_stats_calculator.cpp
    test_spell_stats.cpp
    test_spell_stacking.cpp
)
target_link_libraries(eq_tests PRIVATE eq_core GTest::gtest_main)
target_include_directories(eq_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
include(GoogleTest)
gtest_discover_tests(eq_tests)
