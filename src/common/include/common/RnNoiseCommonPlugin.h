#pragma once

#include <memory>
#include <vector>

#include <cstring>
#include <cassert>
#include <atomic>

struct DenoiseState;

struct RnNoiseStats {
    /* (Accumulative) How many blocks are unmuted due to grace period */
    uint32_t vadGraceBlocks;
    /* (Accumulative) How many blocks are unmuted due to retroactive grace period */
    uint32_t retroactiveVADGraceBlocks;

    /* How many blocks are in an output queue in a single channel. Represents current latency. */
    uint32_t blocksWaitingForOutput;

    /* How many output frames we are forced to zero out because there is not enough frames to write. */
    uint64_t outputFramesForcedToBeZeroed;
};

class RnNoiseCommonPlugin {
public:

    explicit RnNoiseCommonPlugin(uint32_t channels) :
            m_channelCount(channels) {}

    void init();

    void deinit();

    /**
     *
     * @param in
     * @param out
     * @param sampleFrames The amount of frames to process.
     * @param vadThreshold Voice activation threshold.
     * @param vadGracePeriodBlocks For how many blocks output will not be silenced after a voice
     * was detected last time.
     * @param retroactiveVADGraceBlocks If voice is detected in current block, how many blocks
     * in the past will not be silenced. Introduces the delay of retroactiveVADGraceBlocks blocks.
     */
    void process(const float *const *in, float **out, size_t sampleFrames, float vadThreshold,
                 uint32_t vadGracePeriodBlocks, uint32_t retroactiveVADGraceBlocks);

    void resetStats();
    const RnNoiseStats getStats() const;

private:

    void createDenoiseState();

private:
    static const size_t k_denoiseBlockSize = 480;
    static const uint32_t k_denoiseSampleRate = 48000;

    uint32_t m_channelCount;

    uint64_t m_newOutputIdx = 0;
    uint64_t m_lastOutputIdxOverVADThreshold = 0;

    uint64_t m_currentOutputIdxToOutput = 0;

    uint32_t m_prevRetroactiveVADGraceBlocks = 0;

    enum class ChunkUnmuteState {
        MUTED,
        UNMUTED_BY_DEFAULT,
        UNMUTED_VAD,
        UNMUTED_RETRO_VAD,
    };

    struct OutputChunk {
        uint64_t idx;
        float vadProbability;
        float maxVadProbability;
        ChunkUnmuteState muteState;
        float frames[480];
        size_t curOffset;
    };

    struct ChannelData {
        uint32_t idx;

        std::shared_ptr<DenoiseState> denoiseState;

        std::vector<float> rnnoiseInput;
        std::vector<std::unique_ptr<OutputChunk>> rnnoiseOutput;

        std::vector<std::unique_ptr<OutputChunk>> outputBlocksCache;
    };
    std::vector<ChannelData> m_channels;

    std::atomic<RnNoiseStats> m_stats;
};



