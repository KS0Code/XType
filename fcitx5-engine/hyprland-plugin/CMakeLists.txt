# xtype-inline: Hyprland plugin for ghost.hyprland_inline (see main.cpp).
# Plugins link against the running compositor's internals, so this must be
# built with the headers of the Hyprland version that loads it.
add_library(xtype-inline MODULE main.cpp)
set_target_properties(xtype-inline PROPERTIES PREFIX "" CXX_STANDARD 23)
target_compile_options(xtype-inline PRIVATE -Wno-c++11-narrowing
    $<$<CXX_COMPILER_ID:GNU>:--no-gnu-unique>)
target_link_libraries(xtype-inline PRIVATE PkgConfig::HYPRLAND_PLUGIN_DEPS)
install(TARGETS xtype-inline LIBRARY DESTINATION "${XTYPE_HYPRLAND_PLUGIN_DIR}")
