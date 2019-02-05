cmake_minimum_required(VERSION 3.6)
project(rnnoise_vst2_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

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

foreach (file ${VST2_INTERFACE_SRC})
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
        message(WARNING "${file} does not exist.\n Please see vst2.x/FILES.txt and pluginterfaces/vst2.x/FILES.txt")
        return()
    endif ()
endforeach ()

set(VST2_IMPL_SRC
        RnNoiseVstPlugin.h
        RnNoiseVstPlugin.cpp)

set(VST2_PLUGIN_SRC
        ${VST2_INTERFACE_SRC}
        ${VST2_IMPL_SRC})

set(VST2_TARGET rnnoise_vst)

add_library(${VST2_TARGET} SHARED ${VST2_PLUGIN_SRC})

target_link_libraries(${VST2_TARGET} RnNoisePluginCommon "-static-libgcc -static-libstdc++")

set(COMPILE_OPTIONS "$<$<CONFIG:RELEASE>:-O3;>")

target_compile_options(${VST2_TARGET} PRIVATE ${COMPILE_OPTIONS})

target_compile_definitions(${VST2_TARGET} PRIVATE
        "$<$<CXX_COMPILER_ID:GNU>:__cdecl=;>" # Workaround for vst headers
        )

set_target_properties(${VST2_TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/vst"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/vst")
