#pragma once

#include <memory>
#include <vector>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <atomic>

struct DenoiseState;

struct RnNoiseStats {
    /* (Accumulative) How many blocks are unmuted due to grace period */
    uint32_t vadGraceBlocks;
    /* (Accumulative) How many blocks are unmuted due to retroactive grace period */
    uint32_t retroactiveVADGraceBlocks;

    /* Number of blocks in the output queue of one channel. */
    uint32_t blocksWaitingForOutput;
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
     * @param dryMix How much unprocessed input is mixed back in. 0 means fully processed,
     * 1 means dry input only.
     */
    void process(const float *const *in, float **out, size_t sampleFrames, float vadThreshold,
                 uint32_t vadGracePeriodBlocks, uint32_t retroactiveVADGraceBlocks, float dryMix = 0.f);

    /**
     * Returns the delay from input to output, in samples. The block size of process()
     * does not change this delay. The delay is the sum of the RNNoise delay, one buffer
     * block and the retroactive VAD grace blocks.
     */
    static uint32_t getLatencySamples(uint32_t retroactiveVADGraceBlocks);

    void resetStats();
    const RnNoiseStats getStats() const;

    static const size_t k_denoiseBlockSize = 480;
    static const uint32_t k_denoiseSampleRate = 48000;
    static const uint32_t k_maxRetroactiveVADGraceBlocks = 99;

private:

    struct OutputChunk;

    void createDenoiseState();
    void processBlock(float vadThreshold, uint32_t vadGracePeriodBlocks, RnNoiseStats &stats);
    void outputFrames(float **out, size_t outOffset, size_t frames, float dryMix);
    void dropFrames(size_t frames);
    void setRetroactiveVADGraceBlocks(uint32_t retroactiveVADGraceBlocks);
    OutputChunk &queueAt(size_t idx, uint32_t channelIdx);
    void pushSilentChunkFront();

private:
    /* RNNoise output is the result for the input from two blocks before. */
    static const uint32_t k_rnnoiseDelayBlocks = 2;
    /* Maximum queue size: R + 1 latency blocks, one partly sent block and one new block. */
    static const size_t k_queueCapacity = k_maxRetroactiveVADGraceBlocks + 3;

    uint32_t m_channelCount;

    uint64_t m_newOutputIdx = 0;
    bool m_hasLastOutputIdxOverVADThreshold = false;
    uint64_t m_lastOutputIdxOverVADThreshold = 0;

    uint32_t m_retroactiveVADGraceBlocks = 0;

    /* Number of frames in ChannelData::pendingInput. This value is the same for all channels. */
    size_t m_pendingFrames = 0;

    /* The output queue is a ring of chunks. All channels use the same ring positions.
     * After each input slice, this is always true:
     * queued output frames + m_pendingFrames == k_denoiseBlockSize * (1 + R).
     */
    size_t m_queueHead = 0;
    size_t m_queueSize = 0;

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
        float frames[k_denoiseBlockSize];
        float dryFrames[k_denoiseBlockSize];
        size_t curOffset;
    };

    struct ChannelData {
        uint32_t idx;

        std::shared_ptr<DenoiseState> denoiseState;

        /* Scaled input for RNNoise. It stays here until a full block is available. */
        std::vector<float> pendingInput;
        /* Unscaled input of the last k_rnnoiseDelayBlocks + 1 blocks.
         * Use it to align the dry output with the wet output.
         */
        std::vector<float> dryHistory;
        std::vector<OutputChunk> queue;
    };
    std::vector<ChannelData> m_channels;

    std::atomic<RnNoiseStats> m_stats;
};
