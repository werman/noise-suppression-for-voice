project("Real-time Noise Suppression Plugin")
cmake_minimum_required(VERSION 3.6)

include(GNUInstallDirs)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BINARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(MINGW_ADDITIONAL_LINKING_FLAGS "-static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic")

if(NOT BUILD_VERSION)
    set(BUILD_VERSION 1.99)
endif()

option(USE_SYSTEM_JUCE "" OFF)
option(BUILD_FOR_RELEASE "Additional optimizations and steps may be taken for release" OFF)
option(BUILD_TESTS "" ON)
option(BUILD_VST_PLUGIN "If the VST2 plugin should be built" ON)
option(BUILD_VST3_PLUGIN "If the VST3 plugin should be built" ON)
option(BUILD_LV2_PLUGIN "If the LV2 plugin should be built" ON)
option(BUILD_LADSPA_PLUGIN "If the LADSPA plugin should be built" ON)
option(BUILD_AU_PLUGIN "If the AU plugin should be built (macOS only)" ON)
option(BUILD_AUV3_PLUGIN "If the AUv3 plugin should be built (macOS only)" ON)
option(BUILD_RTCD "Enable x86 run-time CPU detection (x86 only)" OFF)

if (BUILD_TESTS)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/catch2/)
endif ()

add_subdirectory(external/rnnoise)
add_subdirectory(src/common)
if (BUILD_LADSPA_PLUGIN)
    add_subdirectory(src/ladspa_plugin)
endif ()

if (BUILD_VST_PLUGIN OR BUILD_VST3_PLUGIN OR BUILD_LV2_PLUGIN OR BUILD_AU_PLUGIN OR BUILD_AUV3_PLUGIN)
    if (USE_SYSTEM_JUCE)
        find_package(JUCE)
    else ()
        # For install JUCE copies all its headers, no one needs them. It also doesn't install actual libraries.
        # On the other hand JUCE could install libraries during build process (see COPY_PLUGIN_AFTER_BUILD option).
        # So we have to manually install plugins.
        add_subdirectory(${JUCE_SOURCE_DIR} external/JUCE EXCLUDE_FROM_ALL)
    endif ()

    add_subdirectory(src/juce_plugin)
endif ()
