cmake_minimum_required(VERSION 3.6)
project(rnnoise_vst)

set(CMAKE_CXX_STANDARD 14)

include_directories(src/)

set(PLUGIN_INTERFACE_SRC
        src/pluginterfaces/vst2.x/aeffect.h
        src/pluginterfaces/vst2.x/aeffectx.h
        src/pluginterfaces/vst2.x/vstfxstore.h)

set(VST2_SRC
        src/vst2.x/aeffeditor.h
        src/vst2.x/audioeffect.h
        src/vst2.x/audioeffectx.h
        src/vst2.x/vstplugmain.cpp
        src/vst2.x/audioeffect.cpp
        src/vst2.x/audioeffectx.cpp)

set(RNNOISE_SRC
        src/rnnoise/rnnoise.h
        src/rnnoise/_kiss_fft_guts.h
        src/rnnoise/arch.h
        src/rnnoise/celt_lpc.c
        src/rnnoise/celt_lpc.h
        src/rnnoise/common.h
        src/rnnoise/denoise.c
        src/rnnoise/kiss_fft.c
        src/rnnoise/kiss_fft.h
        src/rnnoise/opus_types.h
        src/rnnoise/pitch.c
        src/rnnoise/pitch.h
        src/rnnoise/rnn.c
        src/rnnoise/rnn.h
        src/rnnoise/rnn_data.c
        src/rnnoise/rnn_data.h
        src/rnnoise/tansig_table.h)

set(ALL_SOURCES
        ${PLUGIN_INTERFACE_SRC}
        ${VST2_SRC}
        ${RNNOISE_SRC}
        src/RnNoiseAudioEffect.h
        src/RnNoiseAudioEffect.cpp)

add_subdirectory(src/rnnoise)

set(TARGET_X64 rnnoise_vst_x64)
set(TARGET_X32 rnnoise_vst_x32)

add_library(${TARGET_X64} SHARED ${ALL_SOURCES})
add_library(${TARGET_X32} SHARED ${ALL_SOURCES})

target_link_libraries(${TARGET_X64} "-static -static-libgcc -static-libstdc++ -m64")
target_link_libraries(${TARGET_X32} "-static -static-libgcc -static-libstdc++ -m32")

target_compile_options(${TARGET_X64}
        PRIVATE
        "-m64;"
        "$<$<CONFIG:RELEASE>:-O3;>"
        )

target_compile_options(${TARGET_X32}
        PRIVATE
        "-m32;"
        "$<$<CONFIG:RELEASE>:-O3;>"
        )