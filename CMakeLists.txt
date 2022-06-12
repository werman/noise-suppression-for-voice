project("Real-time Noise Suppression Plugin")
cmake_minimum_required(VERSION 3.6)

include(GNUInstallDirs)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BINARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(MINGW_ADDITIONAL_LINKING_FLAGS "-static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic")

option(BUILD_TESTS "" ON)
option(BUILD_VST_PLUGIN "If the VST2 plugin should be built" ON)
option(BUILD_VST3_PLUGIN "If the VST3 plugin should be built" ON)
option(BUILD_LV2_PLUGIN "If the LV2 plugin should be built" ON)
option(BUILD_LADSPA_PLUGIN "If the LADSPA plugin should be built" ON)
option(BUILD_AU_PLUGIN "If the AU plugin should be built (macOS only)" ON)
option(BUILD_AUV3_PLUGIN "If the AUv3 plugin should be built (macOS only)" ON)

if (BUILD_TESTS)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/catch2/)
endif ()

add_subdirectory(src/rnnoise)
add_subdirectory(src/common)
if (BUILD_LADSPA_PLUGIN)
    add_subdirectory(src/ladspa_plugin)
endif ()
if (BUILD_VST_PLUGIN OR BUILD_VST3_PLUGIN OR BUILD_LV2_PLUGIN OR BUILD_AU_PLUGIN OR BUILD_AUV3_PLUGIN)
    add_subdirectory(external/JUCE)
    add_subdirectory(src/juce_plugin)
endif ()
