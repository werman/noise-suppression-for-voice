#pragma once

#include <memory>
#include <vector>
#include "vst2.x/audioeffectx.h"

struct DenoiseState;

class RnNoiseAudioEffect : public AudioEffectX {
public:

    RnNoiseAudioEffect(audioMasterCallback audioMaster, VstInt32 numPrograms, VstInt32 numParams);

    ~RnNoiseAudioEffect() override;

    VstInt32 startProcess() override;

    VstInt32 stopProcess() override;

    void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override;

private:

    void createDenoiseState();

private:
    static const int k_denoiseFrameSize = 480;
    static const int k_denoiseSampleRate = 48000;

    std::shared_ptr<DenoiseState> m_denoiseState;

    std::vector<float> m_inputBuffer;
    std::vector<float> m_outputBuffer;
};