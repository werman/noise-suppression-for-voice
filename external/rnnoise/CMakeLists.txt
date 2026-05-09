cmake_minimum_required(VERSION 3.6)
project(RnNoise LANGUAGES C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(src/)
set(RN_NOISE_SRC
        src/celt_lpc.c
        src/denoise.c
        src/kiss_fft.c
        src/nnet.c
        src/nnet_default.c
        src/parse_lpcnet_weights.c
        src/pitch.c
        src/rnn.c
        src/rnnoise_tables.c
        src/rnnoise_data.c)

if(BUILD_RTCD)
        add_compile_definitions(CPU_INFO_BY_ASM)
        add_compile_definitions(RNN_ENABLE_X86_RTCD)
        list(APPEND RN_NOISE_SRC
                src/x86/x86cpu.c
                src/x86/x86_dnn_map.c
                src/x86/nnet_avx2.c
                src/x86/nnet_sse4_1.c
        )
endif()

add_library(RnNoise STATIC ${RN_NOISE_SRC})

if(BUILD_RTCD)
        if(MSVC)
                set_source_files_properties(src/x86/nnet_avx2.c PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
                set_source_files_properties(src/x86/nnet_sse4_1.c PROPERTIES COMPILE_DEFINITIONS OPUS_X86_MAY_HAVE_SSE4_1)
        else()
                set_source_files_properties(src/x86/nnet_avx2.c PROPERTIES COMPILE_OPTIONS "-mavx;-mfma;-mavx2")
                set_source_files_properties(src/x86/nnet_sse4_1.c PROPERTIES COMPILE_OPTIONS "-msse4.1")
        endif()
endif()

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
