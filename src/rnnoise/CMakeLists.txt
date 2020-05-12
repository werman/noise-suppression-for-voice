cmake_minimum_required(VERSION 3.6)
project(RnNoise LANGUAGES C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(RN_NOISE_SRC
        include/rnnoise/_kiss_fft_guts.h
        include/rnnoise/arch.h
        include/rnnoise/celt_lpc.h
        include/rnnoise/common.h
        include/rnnoise/kiss_fft.h
        include/rnnoise/opus_types.h
        include/rnnoise/pitch.h
        include/rnnoise/rnn.h
        include/rnnoise/rnn_data.h
        include/rnnoise/rnnoise.h
        include/rnnoise/tansig_table.h
        src/celt_lpc.c
        src/denoise.c
        src/kiss_fft.c
        src/pitch.c
        src/rnn.c
        src/rnn_data.c)

add_library(RnNoise STATIC ${RN_NOISE_SRC})

# Disable all warnings, since it's an external library.
target_compile_options(RnNoise PRIVATE
        $<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
        -w>
        $<$<CXX_COMPILER_ID:MSVC>:
        /w>)

target_include_directories(RnNoise PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)
