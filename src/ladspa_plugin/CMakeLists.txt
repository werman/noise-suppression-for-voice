cmake_minimum_required(VERSION 3.6)
project(rnnoise_ladspa_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(LADSPA_INTERFACE_SRC
        ladspa.h
        ladspa++.h)

set(LADSPA_IMPL_SRC
        RnNoiseLadspaPlugin.h
        RnNoiseLadspaPlugin.cpp)

set(LADSPA_PLUGIN_SOURCES
        ${LADSPA_INTERFACE_SRC}
        ${LADSPA_IMPL_SRC})

set(LADSPA_TARGET rnnoise_ladspa)

add_library(${LADSPA_TARGET} SHARED ${LADSPA_PLUGIN_SOURCES})

if (MINGW)
    target_link_libraries(${LADSPA_TARGET} ${MINGW_ADDITIONAL_LINKING_FLAGS})
endif()

target_link_libraries(${LADSPA_TARGET} RnNoisePluginCommon)

set(COMPILE_OPTIONS "$<$<CONFIG:RELEASE>:-O3;>")

target_compile_options(${LADSPA_TARGET} PRIVATE ${COMPILE_OPTIONS})

set_target_properties(${LADSPA_TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/ladspa"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/ladspa")

install(TARGETS ${LADSPA_TARGET}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/ladspa
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/ladspa
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/ladspa)
