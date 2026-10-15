cmake_minimum_required(VERSION 3.19)
project(xtype-fcitx5 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Fcitx5Core 5.0 REQUIRED)
find_package(CURL REQUIRED)

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
endif()

# context_buffer compiled once; reused by the addon and the test binary
add_library(context_buffer_obj OBJECT src/context_buffer.cpp)
target_include_directories(context_buffer_obj PUBLIC src)

# path_utils — small ~ expansion helper, shared across collector/profile/engine
add_library(path_utils_obj OBJECT src/path_utils.cpp)
target_include_directories(path_utils_obj PUBLIC src)
set_target_properties(path_utils_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# corpus_collector compiled once; reused by the addon and the test binary
add_library(corpus_collector_obj OBJECT src/corpus_collector.cpp)
target_include_directories(corpus_collector_obj PUBLIC src)
set_target_properties(corpus_collector_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# style_profile compiled once; consumed by the addon and tests
add_library(style_profile_obj OBJECT src/style_profile.cpp)
target_include_directories(style_profile_obj PUBLIC src)
set_target_properties(style_profile_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# prompt_builder — pure function, consumed by the addon and tests
add_library(prompt_builder_obj OBJECT src/prompt_builder.cpp)
target_include_directories(prompt_builder_obj PUBLIC src)
set_target_properties(prompt_builder_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(config_loader_obj OBJECT src/config_loader.cpp)
target_include_directories(config_loader_obj PUBLIC src)
set_target_properties(config_loader_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(xtype-fcitx5 SHARED
    src/xtype.cpp
    src/inference_client.cpp
    src/ghost_popup.cpp
)
target_include_directories(xtype-fcitx5 PRIVATE src)
target_link_libraries(xtype-fcitx5 PRIVATE
    context_buffer_obj
    path_utils_obj
    corpus_collector_obj
    style_profile_obj
    prompt_builder_obj
    config_loader_obj
    Fcitx5::Core
    CURL::libcurl
)

set(ADDON_LIB "libxtype-fcitx5")
configure_file(data/xtype.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/data/xtype.conf @ONLY)

install(TARGETS xtype-fcitx5
    LIBRARY DESTINATION "${FCITX_INSTALL_ADDONDIR}"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/data/xtype.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
)
# Popup ghost style: gray-text Fcitx5 theme, selected by the engine while
# ghost.style = "popup" (see src/ghost_popup.h).
install(FILES data/themes/xtype-ghost/theme.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/themes/xtype-ghost"
)

# Optional Hyprland plugin for ghost.hyprland_inline. Built when the Hyprland
# plugin headers are installed; must be rebuilt after every Hyprland update.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(HYPRLAND_PLUGIN_DEPS QUIET IMPORTED_TARGET
        hyprland pixman-1 libdrm libinput libudev wayland-server xkbcommon)
endif()
option(XTYPE_HYPRLAND_PLUGIN "Build the xtype-inline Hyprland plugin" ${HYPRLAND_PLUGIN_DEPS_FOUND})
if(XTYPE_HYPRLAND_PLUGIN)
    set(XTYPE_HYPRLAND_PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/lib/xtype")
    add_subdirectory(hyprland-plugin)
    target_compile_definitions(xtype-fcitx5 PRIVATE
        XTYPE_HYPRLAND_PLUGIN_PATH="${XTYPE_HYPRLAND_PLUGIN_DIR}/xtype-inline.so")
endif()

install(FILES data/xtype-im.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
    RENAME xtype.conf
)

if(BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
