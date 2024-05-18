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
        if(MSVC)
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX /arch:AVX2 /arch:FMA")
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:SSE4.1")
        else()
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx -mfma -mavx2")
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1")
        endif()
        list(APPEND RN_NOISE_SRC
                src/x86/x86cpu.c
                src/x86/x86_dnn_map.c
                src/x86/nnet_avx2.c
                src/x86/nnet_sse4_1.c
        )
endif()

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
