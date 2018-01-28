cmake_minimum_required(VERSION 3.6)
project(rnnoise_vst2_plugin)

set(CMAKE_CXX_STANDARD 14)

include_directories(/)

set(PROJECT_ROOT ../../../)

set(VST2_INTERFACE_SRC
        pluginterfaces/vst2.x/aeffect.h
        pluginterfaces/vst2.x/aeffectx.h
        pluginterfaces/vst2.x/vstfxstore.h
        vst2.x/aeffeditor.h
        vst2.x/audioeffect.h
        vst2.x/audioeffectx.h
        vst2.x/vstplugmain.cpp
        vst2.x/audioeffect.cpp
        vst2.x/audioeffectx.cpp)

set(VST2_IMPL_SRC
        RnNoiseVstPlugin.h
        RnNoiseVstPlugin.cpp)

set(VST2_PLUGIN_SRC
        ${VST2_INTERFACE_SRC}
        ${VST2_IMPL_SRC})

function(build bit)
    set(VST2_TARGET_X${bit} rnnoise_vst_x${bit})

    add_library(${VST2_TARGET_X${bit}} SHARED ${VST2_PLUGIN_SRC})

    target_link_libraries(${VST2_TARGET_X${bit}} RnNoisePluginCommon "-static -static-libgcc -static-libstdc++ -m${bit}")

    set(X${bit}_COMPILE_OPTIONS
            "-m${bit};"
            "$<$<CONFIG:RELEASE>:-O3;>")

    target_compile_options(${VST2_TARGET_X${bit}} PRIVATE ${X${bit}_COMPILE_OPTIONS})

    set_target_properties(${VST2_TARGET_X${bit}} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT}/bin/x${bit}/vst")
endfunction()

build(32)
build(64)