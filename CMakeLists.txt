cmake_minimum_required(VERSION 3.20)
project(yatze VERSION 0.8.0 LANGUAGES C CXX)

# ── Application identity (one place; as Sudoku) ─────────────────────────────
# The freedesktop app ID, from the project's GitHub home. The code gets it (and
# the version) as compile definitions; the resource FILE names follow it. The
# one place it is still spelled by hand: resources/yatze.gresource.xml (glib-
# compile-resources reads it literally) and the .desktop / metainfo contents.
set(YATZE_APP_ID "io.github.scott8420.Yatze")
string(REPLACE "." "/" YATZE_APP_PATH "/${YATZE_APP_ID}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)
find_package(spdlog REQUIRED)
pkg_check_modules(EPOXY REQUIRED epoxy)   # OpenGL loader (GTK itself uses it)

# ── yatze_model -- GTK-free, BY DESIGN ───────────────────────────────────────
# No gtkmm on this library's link line: a UI include under model/ fails to
# compile, so the seam is enforced by the build graph and the model stays
# headless-testable. Rules, the game and its verbs, the undo journal
# (header-only), the drawers' state, the shortcut registry.
add_library(yatze_model STATIC
    src/model/Rules.cpp
    src/model/Game.cpp
    src/model/Strategy.cpp
    src/model/Computer.cpp
    src/model/TableFile.cpp
    src/model/Settings.cpp
    src/model/Stats.cpp
    src/model/Shortcuts.cpp
)
target_include_directories(yatze_model PUBLIC include)
target_include_directories(yatze_model PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)   # json.hpp: the model's business only
# The strategy engine is optimised in EVERY build type: its table (~1M states)
# takes seconds at -O2 and minutes at -O0, and Debug is the default here.
set_source_files_properties(src/model/Strategy.cpp PROPERTIES COMPILE_OPTIONS "-O2")
find_package(Threads REQUIRED)
target_link_libraries(yatze_model PUBLIC Threads::Threads)

# ── yatze_gfx -- the dice table's geometry and the filmed take (physics +
# planned camera). GTK-free and GL-free, so the selftest shoots takes
# headless. Optimised in every build: a take is ~1500 physics steps.
add_library(yatze_gfx STATIC src/gfx/Take.cpp)
target_include_directories(yatze_gfx PUBLIC include)
set_source_files_properties(src/gfx/Take.cpp PROPERTIES COMPILE_OPTIONS "-O2")

# ── Headless selftest ────────────────────────────────────────────────────────
add_executable(yatze_selftest src/model/selftest.cpp)
target_link_libraries(yatze_selftest PRIVATE yatze_model yatze_gfx)

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
    src/DiceView.cpp
    src/ViewOrb.cpp
    src/Scorecard.cpp
    src/HintPanel.cpp
    src/NewGameDialog.cpp
    src/Engine.cpp
    src/StatsPanel.cpp
    src/AboutWindow.cpp
    src/ShortcutsDialog.cpp
    src/Registry.cpp
    src/Log.cpp
)
target_include_directories(yatze PRIVATE include ${GTKMM_INCLUDE_DIRS} ${EPOXY_INCLUDE_DIRS})
target_link_libraries(yatze     PRIVATE ${GTKMM_LIBRARIES} ${EPOXY_LIBRARIES} yatze_model yatze_gfx spdlog::spdlog)
target_link_directories(yatze   PRIVATE ${GTKMM_LIBRARY_DIRS})
target_compile_options(yatze    PRIVATE ${GTKMM_CFLAGS_OTHER})

target_compile_definitions(yatze PRIVATE
    YATZE_APP_ID="${YATZE_APP_ID}"
    YATZE_APP_PATH="${YATZE_APP_PATH}"
    YATZE_VERSION="${PROJECT_VERSION}")

# ── Install (so it launches from the GNOME overview) ────────────────────────
#   ./install.sh            (or: cmake --install build --prefix ~/.local)
# The binary, the .desktop entry, the full-colour app icon (scalable), the
# symbolic icon, and the AppStream metainfo -- into the XDG dirs GNOME already
# searches. Running ./build/yatze uninstalled still works (the in-app logo is
# in the gresource); install is only for the app grid.
include(GNUInstallDirs)
install(TARGETS yatze RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/${YATZE_APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/${YATZE_APP_ID}.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES resources/${YATZE_APP_ID}-symbolic.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/symbolic/apps)
install(FILES resources/${YATZE_APP_ID}.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
