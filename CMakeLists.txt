cmake_minimum_required(VERSION 3.25)
project(EqQuarmCompanion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Packages ─────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network Concurrent)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest        CONFIG REQUIRED)

# ── Core library (no Qt) ─────────────────────────────────────────────
add_library(eq_core STATIC
    src/core/config.cpp
    src/core/character_parser.cpp
    src/core/stats_calculator.cpp
    src/core/npc_analysis.cpp
    src/core/spell_stats.cpp
    src/core/spell_stacking.cpp
    src/core/derived_metrics.cpp
    src/core/equipped_effects.cpp
)
target_include_directories(eq_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(eq_core PUBLIC nlohmann_json::nlohmann_json)

# ── DB library (Qt SQL + Network) ────────────────────────────────────
add_library(eq_db STATIC
    src/db/db_connection.cpp
    src/db/item_database.cpp
    src/db/npc_database.cpp
    src/db/bis_scraper.cpp
    src/db/item_database.h
    src/db/npc_database.h
)
target_include_directories(eq_db PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(eq_db PUBLIC Qt6::Sql Qt6::Network eq_core)

# ── Executable ───────────────────────────────────────────────────────
add_executable(EqQuarmCompanion WIN32
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/character_tab.cpp
    src/ui/fight_tab.cpp
    src/ui/spells_tab.cpp
    src/ui/spells_tab.h
    src/ui/stats_bar.cpp
    src/ui/stats_bar.h
    src/ui/infos_tab.cpp
    src/ui/widgets.cpp
    src/ui/settings_dialog.cpp
)
target_link_libraries(EqQuarmCompanion PRIVATE
    Qt6::Widgets Qt6::Concurrent eq_core eq_db
)

# Copy config_defaults.json next to exe
add_custom_command(TARGET EqQuarmCompanion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/config_defaults.json"
        "$<TARGET_FILE_DIR:EqQuarmCompanion>/config_defaults.json"
    COMMENT "Copying config_defaults.json"
)

# windeployqt (Release seulement)
find_program(WINDEPLOYQT windeployqt
    HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET EqQuarmCompanion POST_BUILD
        COMMAND "${WINDEPLOYQT}" --no-translations
            "$<TARGET_FILE:EqQuarmCompanion>"
        COMMENT "windeployqt"
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
