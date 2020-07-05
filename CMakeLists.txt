cmake_minimum_required(VERSION 3.6)

include(GNUInstallDirs)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BINARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(MINGW_ADDITIONAL_LINKING_FLAGS "-static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic")

option(BUILD_VST_PLUGIN "If the VST plugin should be built" ON)
option(BUILD_LV2_PLUGIN "If the LV2 plugin should be built" ON)
option(BUILD_LADSPA_PLUGIN "If the LADSPA plugin should be built" ON)

add_subdirectory(src/rnnoise)
add_subdirectory(src/common)
if(BUILD_VST_PLUGIN)
    add_subdirectory(src/vst_plugin)
endif()
if(BUILD_LV2_PLUGIN)
    add_subdirectory(src/lv2_plugin)
endif()
if(BUILD_LADSPA_PLUGIN)
    add_subdirectory(src/ladspa_plugin)
endif()
