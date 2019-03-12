#pragma once

#include <memory>
#include <vector>

#include <samplerate.h>

struct DenoiseState;

class RnNoiseCommonPlugin {
public:
    RnNoiseCommonPlugin();

    void setSampleRate(unsigned long sampleRate);

    bool init();

    void deinit();

    const char * getError()
    {
      return m_errorStr;
    }

    void process(const float *in, float *out, int32_t sampleFrames);

private:
    const char * m_errorStr;

    bool m_initialized;
    bool m_resample;

    static const int k_denoiseFrameSize = 480;
    static const int k_denoiseSampleRate = 48000;

    std::shared_ptr<SRC_STATE>    m_srcIn;
    std::shared_ptr<SRC_STATE>    m_srcOut;
    double m_downRatio;
    double m_upRatio;
    std::shared_ptr<DenoiseState> m_denoiseState;

    std::vector<float> m_inBuffer;
    std::vector<float> m_outBuffer;
    size_t m_outBufferR;
    size_t m_outBufferW;
    size_t m_outBufferA;
};