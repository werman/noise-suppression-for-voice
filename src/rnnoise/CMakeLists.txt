cmake_minimum_required(VERSION 3.6)
project(RnNoise LANGUAGES C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(RN_NOISE_SRC
        include/rnnoise/_kiss_fft_guts.h
        include/rnnoise/arch.h
        include/rnnoise/celt_lpc.h
        include/rnnoise/common.h
        include/rnnoise/denoise.h
        include/rnnoise/kiss_fft.h
        include/rnnoise/nnet.h
        include/rnnoise/nnet_arch.h
        include/rnnoise/opus_types.h
        include/rnnoise/pitch.h
        include/rnnoise/rnn.h
        include/rnnoise/rnnoise.h
        include/rnnoise/rnnoise_data.h
        include/rnnoise/vec.h
        include/rnnoise/vec_avx.h
        include/rnnoise/vec_neon.h
        include/rnnoise/x86/x86_arch_macros.h
        src/celt_lpc.c
        src/denoise.c
        src/kiss_fft.c
        src/nnet.c
        src/nnet_default.c
        src/parse_lpcnet_weights.c
        src/pitch.c
        src/rnn.c
        src/rnnoise_data.c
        src/rnnoise_tables.c)

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
