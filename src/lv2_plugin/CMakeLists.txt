cmake_minimum_required(VERSION 3.6)
project(rnnoise_lv2_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(LV2_INTERFACE_SRC
        lv2core/lv2.h
        lv2core/lv2_util.h
        lv2core/Plugin.hpp
        lv2core/Lib.hpp)

set(LV2_IMPL_SRC
        RnNoiseLv2Plugin.h
        RnNoiseLv2Plugin.cpp
        RnNoiseLv2Lib.h
        RnNoiseLv2Lib.cpp)

set(LV2_PLUGIN_SOURCES
        ${LV2_INTERFACE_SRC}
        ${LV2_IMPL_SRC})

set(LV2_TARGET rnnoise_lv2)

add_library(${LV2_TARGET} SHARED ${LV2_PLUGIN_SOURCES})

target_link_libraries(${LV2_TARGET} RnNoisePluginCommon "-static-libgcc -static-libstdc++")

set(COMPILE_OPTIONS "$<$<CONFIG:RELEASE>:-O3;>")

target_compile_options(${LV2_TARGET} PRIVATE ${COMPILE_OPTIONS})

set_target_properties(${LV2_TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rnnoise.lv2"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rnnoise.lv2")

configure_file(resources/manifest.ttl ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rnnoise.lv2/manifest.ttl)
configure_file(resources/rnnoise.ttl ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rnnoise.lv2/rnnoise.ttl)

install(TARGETS ${LV2_TARGET}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
