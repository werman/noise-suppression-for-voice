#include "common/RnNoiseCommonPlugin.h"

#include <cstring>
#include <ios>
#include <limits>
#include <algorithm>

#include <rnnoise/rnnoise.h>

#define VAD_PROB_THRESHOLD 0.95
// the amount of samples that aren't silenced,
// irregardless of rnnoise's VAD result, after one was detected.
// this fixes cut outs in the middle of words
// each sample is 10ms. 
#define VAD_GRACE_PERIOD 20

void RnNoiseCommonPlugin::init() {
    deinit();
    createDenoiseState();
}

void RnNoiseCommonPlugin::deinit() {
    m_denoiseState.reset();
}

void RnNoiseCommonPlugin::process(const float *in, float *out, int32_t sampleFrames) {
    if (sampleFrames == 0) {
        return;
    }

    if (!m_denoiseState) {
        createDenoiseState();
    }

    // Good case, we can copy less data around and rnnoise lib is built for it
    if (sampleFrames == k_denoiseFrameSize) {
        m_inputBuffer.resize(sampleFrames);

        for (size_t i = 0; i < sampleFrames; i++) {
            m_inputBuffer[i] = in[i] * std::numeric_limits<short>::max();
        }

        float vad_prob = rnnoise_process_frame(m_denoiseState.get(), out, &m_inputBuffer[0]);

        if (vad_prob >= VAD_PROB_THRESHOLD ) {
            m_remaining_grace_period = VAD_GRACE_PERIOD;
        }

        if (m_remaining_grace_period > 0) {
            m_remaining_grace_period--;
            for (size_t i = 0; i < sampleFrames; i++) {
                out[i] /= std::numeric_limits<short>::max();
            }
        } else {
            for (size_t i = 0; i < sampleFrames; i++) {
                out[i] = 0.f;
            }
        }

    } else {
        m_inputBuffer.resize(m_inputBuffer.size() + sampleFrames);

        // From [-1.f,1.f] range to [min short, max short] range which rnnoise lib will understand
        {
            float *inputBufferWriteStart = (m_inputBuffer.end() - sampleFrames).base();
            for (size_t i = 0; i < sampleFrames; i++) {
                inputBufferWriteStart[i] = in[i] * std::numeric_limits<short>::max();
            }
        }

        const size_t samplesToProcess = m_inputBuffer.size() / k_denoiseFrameSize;
        const size_t framesToProcess = samplesToProcess * k_denoiseFrameSize;

        m_outputBuffer.resize(m_outputBuffer.size() + framesToProcess);

        // Process input buffer by chunks of k_denoiseFrameSize, put result into out buffer to return into range [-1.f,1.f]
        {
            float *outBufferWriteStart = (m_outputBuffer.end() - framesToProcess).base();

            for (size_t i = 0; i < samplesToProcess; i++) {
                float *currentOutBuffer = &outBufferWriteStart[i * k_denoiseFrameSize];
                float *currentInBuffer = &m_inputBuffer[i * k_denoiseFrameSize];
                float vad_prob = rnnoise_process_frame(m_denoiseState.get(), currentOutBuffer, currentInBuffer);

                if (vad_prob >= VAD_PROB_THRESHOLD ) {
                    m_remaining_grace_period = VAD_GRACE_PERIOD;
                }

                if (m_remaining_grace_period > 0) {
                    m_remaining_grace_period--;
                    for (size_t j = 0; j < k_denoiseFrameSize; j++) {
                        currentOutBuffer[j] /= std::numeric_limits<short>::max();
                    }
                } else {
                    for (size_t j = 0; j < k_denoiseFrameSize; j++) {
                        currentOutBuffer[j] = 0.f;
                    }
                }
            }
        }

        const size_t toCopyIntoOutput = std::min(m_outputBuffer.size(), static_cast<size_t>(sampleFrames));

        std::memcpy(out, &m_outputBuffer[0], toCopyIntoOutput * sizeof(float));

        m_inputBuffer.erase(m_inputBuffer.begin(), m_inputBuffer.begin() + framesToProcess);
        m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + toCopyIntoOutput);

        if (toCopyIntoOutput < sampleFrames) {
            std::fill(out + toCopyIntoOutput, out + sampleFrames, 0.f);
        }
    }
}

void RnNoiseCommonPlugin::createDenoiseState() {
    m_denoiseState = std::shared_ptr<DenoiseState>(rnnoise_create(), [](DenoiseState *st) {
        rnnoise_destroy(st);
    });
}