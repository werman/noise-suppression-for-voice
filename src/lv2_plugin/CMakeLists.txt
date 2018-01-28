cmake_minimum_required(VERSION 3.6)
project(rnnoise_lv2_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

include_directories(/)

set(PROJECT_ROOT ../../../)

set(LV2_INTERFACE_SRC
        lv2/lv2plug.in/ns/lv2core/lv2.h
        lv2/lv2plug.in/ns/lv2core/lv2_util.h
        lv2/lv2plug.in/ns/lv2core/Plugin.hpp
        lv2/lv2plug.in/ns/lv2core/Lib.hpp)

set(LV2_IMPL_SRC
        RnNoiseLv2Plugin.h
        RnNoiseLv2Plugin.cpp
        RnNoiseLv2Lib.h
        RnNoiseLv2Lib.cpp)

set(LV2_PLUGIN_SOURCES
        ${LV2_INTERFACE_SRC}
        ${LV2_IMPL_SRC})

function(build bit)
    set(LV2_TARGET_X${bit} rnnoise_lv2_x${bit})

    add_library(${LV2_TARGET_X${bit}} SHARED ${LV2_PLUGIN_SOURCES})

    target_link_libraries(${LV2_TARGET_X${bit}} RnNoisePluginCommon "-static -static-libgcc -static-libstdc++ -m${bit}")

    set(X${bit}_COMPILE_OPTIONS
            "-m${bit};"
            "$<$<CONFIG:RELEASE>:-O3;>")

    target_compile_options(${LV2_TARGET_X${bit}} PRIVATE ${X${bit}_COMPILE_OPTIONS})

    set_target_properties(${LV2_TARGET_X${bit}} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT}/bin/x${bit}/rnnoise.lv2")

    configure_file(resources/manifest_x${bit}.ttl ${PROJECT_ROOT}/bin/x${bit}/rnnoise.lv2/manifest.ttl)
    configure_file(resources/rnnoise.ttl ${PROJECT_ROOT}/bin/x${bit}/rnnoise.lv2/rnnoise.ttl)
endfunction()

build(32)
build(64)
