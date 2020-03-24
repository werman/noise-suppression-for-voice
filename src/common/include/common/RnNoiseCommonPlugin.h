#pragma once

#include <memory>
#include <vector>

struct DenoiseState;

class RnNoiseCommonPlugin {
public:

    void init();

    void deinit();

    void process(const float *in, float *out, int32_t sampleFrames);

private:

    void createDenoiseState();

private:
    static const int k_denoiseFrameSize = 480;
    static const int k_denoiseSampleRate = 48000;

    std::shared_ptr<DenoiseState> m_denoiseState;

    short m_remaining_grace_period = 0;

    std::vector<float> m_inputBuffer;
    std::vector<float> m_outputBuffer;
};



