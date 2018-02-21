cmake_minimum_required(VERSION 3.6)
project(rnnoise_ladspa_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(PROJECT_ROOT ../../../)

set(LADSPA_INTERFACE_SRC
        ladspa.h
        ladspa++.h)

set(LADSPA_IMPL_SRC
        RnNoiseLadspaPlugin.h
        RnNoiseLadspaPlugin.cpp)

set(LADSPA_PLUGIN_SOURCES
        ${LADSPA_INTERFACE_SRC}
        ${LADSPA_IMPL_SRC})

function(build bit)
    set(LADSPA_TARGET_X${bit} rnnoise_ladspa_x${bit})

    add_library(${LADSPA_TARGET_X${bit}} SHARED ${LADSPA_PLUGIN_SOURCES})

    target_link_libraries(${LADSPA_TARGET_X${bit}} RnNoisePluginCommon "-static-libgcc -static-libstdc++ -m${bit}")

    set(X${bit}_COMPILE_OPTIONS
            "-m${bit};"
            "$<$<CONFIG:RELEASE>:-O3;>")

    target_compile_options(${LADSPA_TARGET_X${bit}} PRIVATE ${X${bit}_COMPILE_OPTIONS})

    set_target_properties(${LADSPA_TARGET_X${bit}} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${PROJECT_ROOT}/bin/x${bit}/ladspa")
endfunction()

build(32)
build(64)
