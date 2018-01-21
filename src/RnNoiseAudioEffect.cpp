#include <cstring>
#include <ios>
#include <limits>
#include <algorithm>

#include <rnnoise/rnnoise.h>

#include "RnNoiseAudioEffect.h"

RnNoiseAudioEffect::RnNoiseAudioEffect(audioMasterCallback audioMaster, VstInt32 numPrograms, VstInt32 numParams)
        : AudioEffectX(audioMaster, numPrograms, numParams) {
    setNumInputs(1); // mono in
    setNumOutputs(1); // mono out
    setUniqueID(366056);
    canProcessReplacing(); // supports replacing mode
}

RnNoiseAudioEffect::~RnNoiseAudioEffect() = default;

void RnNoiseAudioEffect::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) {
    if (sampleFrames == 0) {
        return;
    }

    if (!m_denoiseState) {
        createDenoiseState();
    }

    // Mono in/out only
    float *inChannel0 = inputs[0];
    float *outChannel0 = outputs[0];

    // Good case, we can copy less data around and rnnoise lib is built for it
    if (sampleFrames == k_denoiseFrameSize) {
        m_inputBuffer.resize(sampleFrames);

        for (size_t i = 0; i < sampleFrames; i++) {
            m_inputBuffer[i] = inChannel0[i] * std::numeric_limits<short>::max();
        }

        rnnoise_process_frame(m_denoiseState.get(), outChannel0, &m_inputBuffer[0]);

        for (size_t i = 0; i < sampleFrames; i++) {
            outChannel0[i] /= std::numeric_limits<short>::max();
        }
    } else {
        m_inputBuffer.resize(m_inputBuffer.size() + sampleFrames);

        // From [-1.f,1.f] range to [min short, max short] range which rnnoise lib will understand
        {
            float *inputBufferWriteStart = (m_inputBuffer.end() - sampleFrames).base();
            for (size_t i = 0; i < sampleFrames; i++) {
                inputBufferWriteStart[i] = inChannel0[i] * std::numeric_limits<short>::max();
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
                rnnoise_process_frame(m_denoiseState.get(), currentOutBuffer, currentInBuffer);

                for (size_t j = 0; j < k_denoiseFrameSize; j++) {
                    currentOutBuffer[j] /= std::numeric_limits<short>::max();
                }
            }
        }

        const size_t toCopyIntoOutput = std::min(m_outputBuffer.size(), static_cast<size_t>(sampleFrames));

        std::memcpy(outChannel0, &m_outputBuffer[0], toCopyIntoOutput * sizeof(float));

        m_inputBuffer.erase(m_inputBuffer.begin(), m_inputBuffer.begin() + framesToProcess);
        m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + toCopyIntoOutput);

        if (toCopyIntoOutput < sampleFrames) {
            std::fill(outChannel0 + toCopyIntoOutput, outChannel0 + sampleFrames, 0.f);
        }
    }
}

VstInt32 RnNoiseAudioEffect::startProcess() {
    createDenoiseState();

    return AudioEffectX::startProcess();
}

VstInt32 RnNoiseAudioEffect::stopProcess() {
    m_denoiseState.reset();

    return AudioEffectX::stopProcess();
}

void RnNoiseAudioEffect::createDenoiseState() {
    m_denoiseState = std::shared_ptr<DenoiseState>(rnnoise_create(), [](DenoiseState *st) {
        rnnoise_destroy(st);
    });
}

extern AudioEffect *createEffectInstance(audioMasterCallback audioMaster) {
    return new RnNoiseAudioEffect(audioMaster, 0, 0);
}