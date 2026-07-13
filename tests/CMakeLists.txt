add_executable(eq_tests
    test_config.cpp
    test_character_parser.cpp
    test_npc_analysis.cpp
    test_stats_calculator.cpp
    test_spell_stats.cpp
    test_spell_stacking.cpp
)
target_link_libraries(eq_tests PRIVATE eq_core GTest::gtest_main)
target_include_directories(eq_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
include(GoogleTest)
gtest_discover_tests(eq_tests)

option(BUILD_INTEGRATION_TESTS "Build DB integration tests (requires resources/quarm_data.db, cf. db_export/)" OFF)

if(BUILD_INTEGRATION_TESTS)
    add_executable(eq_integration_tests
        test_integration_npc.cpp)
    target_link_libraries(eq_integration_tests PRIVATE
        eq_core eq_db GTest::gtest
        Qt6::Core Qt6::Sql Qt6::Network Qt6::Concurrent)
    target_include_directories(eq_integration_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/src")
    target_compile_definitions(eq_integration_tests PRIVATE
        EQ_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    # Pas de gtest_discover_tests → n'apparaît pas dans ctest standard
endif()
