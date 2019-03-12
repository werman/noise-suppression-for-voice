#pragma once

#include <memory>
#include <vector>
#include <string>

#include <samplerate.h>

struct DenoiseState;

class RnNoiseCommonPlugin {
public:
    bool setSampleRate(uint32_t sampleRate);

    bool init();

    bool resume();

    void deinit();

    const std::string &getError() const {
        return m_errorStr;
    }

    void process(const float *in, float *out, int32_t sampleFrames);

private:
    static const int k_denoiseFrameSize = 480;
    static const int k_denoiseSampleRate = 48000;

    std::string m_errorStr;

    uint32_t m_currentSampleRate{k_denoiseSampleRate};

    bool m_initialized{false};
    bool m_resample{false};

    std::shared_ptr<SRC_STATE> m_srcIn;
    std::shared_ptr<SRC_STATE> m_srcOut;
    double m_downRatio{1.0};
    double m_upRatio{1.0};

    std::shared_ptr<DenoiseState> m_denoiseState;

    std::vector<float> m_inBuffer;
    std::vector<float> m_outBuffer;
    size_t m_outBufferR{0};
    size_t m_outBufferW{0};
    size_t m_outBufferA{0};
};