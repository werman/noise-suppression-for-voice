#pragma once

#include <memory>
#include <vector>

struct DenoiseState;

class RnNoiseCommonPlugin {
public:

    void init();

    void deinit();

    void process(const float *in, float *out, int32_t sampleFrames, float vadThreshold);

private:

    void createDenoiseState();

private:
    static const int k_denoiseFrameSize = 480;
    static const int k_denoiseSampleRate = 48000;

    /**
     * The amount of samples that aren't silenced, regardless of rnnoise's VAD result, after one was detected.
     * This fixes cut outs in the middle of words.
     * Each sample is 10ms.
     */
    static const short k_vadGracePeriodSamples = 20;

    std::shared_ptr<DenoiseState> m_denoiseState;

    short m_remainingGracePeriod = 0;

    std::vector<float> m_inputBuffer;
    std::vector<float> m_outputBuffer;
};



