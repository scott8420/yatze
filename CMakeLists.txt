cmake_minimum_required(VERSION 3.20)
project(yatze VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)
find_package(spdlog REQUIRED)

# ── yatze_model -- GTK-free, BY DESIGN ───────────────────────────────────────
# No gtkmm on this library's link line: a UI include under model/ fails to
# compile, so the seam is enforced by the build graph and the model stays
# headless-testable. Rules, the game and its verbs, the undo journal
# (header-only), the drawers' state, the shortcut registry.
add_library(yatze_model STATIC
    src/model/Rules.cpp
    src/model/Game.cpp
    src/model/Shortcuts.cpp
)
target_include_directories(yatze_model PUBLIC include)

# ── Headless selftest ────────────────────────────────────────────────────────
add_executable(yatze_selftest src/model/selftest.cpp)
target_link_libraries(yatze_selftest PRIVATE yatze_model)

# ── Resources (gresource): the symbolic logo, compiled into the binary ───────
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
set(YATZE_GRESOURCE_XML "${CMAKE_CURRENT_SOURCE_DIR}/resources/yatze.gresource.xml")
set(YATZE_RESOURCES_C   "${CMAKE_CURRENT_BINARY_DIR}/yatze-resources.c")
file(GLOB YATZE_RESOURCE_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/scalable/apps/*.svg")
add_custom_command(
    OUTPUT  "${YATZE_RESOURCES_C}"
    COMMAND "${GLIB_COMPILE_RESOURCES}"
            --target "${YATZE_RESOURCES_C}"
            --generate-source
            --c-name yatze_resources
            yatze.gresource.xml
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    DEPENDS "${YATZE_GRESOURCE_XML}" ${YATZE_RESOURCE_FILES}
    COMMENT "Compiling gresource bundle (yatze-resources.c)"
    VERBATIM)

# ── yatze -- the GTK app. The Shell is split by category (index: Shell.hpp).
add_executable(yatze
    ${YATZE_RESOURCES_C}
    src/main.cpp
    src/App.cpp
    src/Appearance.cpp
    src/Shell.cpp
    src/Shell_zones.cpp
    src/Shell_bindings.cpp
    src/Shell_handlers.cpp
    src/Shell_helpers.cpp
    src/DiceRow.cpp
    src/Scorecard.cpp
    src/HintPanel.cpp
    src/StatsPanel.cpp
    src/AboutWindow.cpp
    src/ShortcutsDialog.cpp
    src/Registry.cpp
    src/Log.cpp
)
target_include_directories(yatze PRIVATE include ${GTKMM_INCLUDE_DIRS})
target_link_libraries(yatze     PRIVATE ${GTKMM_LIBRARIES} yatze_model spdlog::spdlog)
target_link_directories(yatze   PRIVATE ${GTKMM_LIBRARY_DIRS})
target_compile_options(yatze    PRIVATE ${GTKMM_CFLAGS_OTHER})
