cmake_minimum_required(VERSION 3.6)
project(rnnoise_demo)

set(RN_NOISE_FILES
        rnnoise.h
        _kiss_fft_guts.h
        arch.h
        celt_lpc.c
        celt_lpc.h
        common.h
        denoise.c
        kiss_fft.c
        kiss_fft.h
        opus_types.h
        pitch.c
        pitch.h
        rnn.c
        rnn.h
        rnn_data.c
        rnn_data.h
        tansig_table.h
        rnnoise_demo.c)

add_executable(rnnoise_demo ${RN_NOISE_FILES})