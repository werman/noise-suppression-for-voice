#include "common/RnNoiseCommonPlugin.h"

#include <cstring>
#include <limits>
#include <algorithm>
#include <cassert>

#include <rnnoise.h>

static const uint32_t k_minVADGracePeriodBlocks = 20;

const size_t RnNoiseCommonPlugin::k_denoiseBlockSize;
const uint32_t RnNoiseCommonPlugin::k_denoiseSampleRate;
const uint32_t RnNoiseCommonPlugin::k_maxRetroactiveVADGraceBlocks;
const uint32_t RnNoiseCommonPlugin::k_rnnoiseDelayBlocks;
const size_t RnNoiseCommonPlugin::k_queueCapacity;

void RnNoiseCommonPlugin::init() {
    deinit();
    createDenoiseState();
    resetStats();
}

void RnNoiseCommonPlugin::deinit() {
    m_channels.clear();
}

uint32_t RnNoiseCommonPlugin::getLatencySamples(uint32_t retroactiveVADGraceBlocks) {
    retroactiveVADGraceBlocks = std::min(retroactiveVADGraceBlocks, k_maxRetroactiveVADGraceBlocks);
    return static_cast<uint32_t>((k_rnnoiseDelayBlocks + 1 + retroactiveVADGraceBlocks) * k_denoiseBlockSize);
}

void
RnNoiseCommonPlugin::process(const float *const *in, float **out, size_t sampleFrames, float vadThreshold,
                             uint32_t vadGracePeriodBlocks, uint32_t retroactiveVADGraceBlocks, float dryMix) {
    /* TODO: Option to output noise when channel is muted; */

    assert(vadThreshold >= 0.f && vadThreshold <= 1.f);
    assert(dryMix >= 0.f && dryMix <= 1.f);

    if (sampleFrames == 0) {
        return;
    }

    dryMix = std::max(std::min(dryMix, 1.f), 0.f);
    vadGracePeriodBlocks = std::max(vadGracePeriodBlocks, k_minVADGracePeriodBlocks);

    setRetroactiveVADGraceBlocks(retroactiveVADGraceBlocks);

    RnNoiseStats stats = m_stats.load();

    /* Process the input in slices. A slice ends at the end of a block or at the end of the input.
     * After each slice, write the same number of frames to the output. Thus the host block size
     * does not change the latency. Also, the newest R blocks stay in the queue for the
     * retroactive VAD. Each slice is read before its output range is written. Thus the input
     * and output buffers can be the same.
     */
    size_t pos = 0;
    while (pos < sampleFrames) {
        const size_t slice = std::min(k_denoiseBlockSize - m_pendingFrames, sampleFrames - pos);

        for (auto &channel: m_channels) {
            const float *src = in[channel.idx] + pos;
            float *pending = &channel.pendingInput[m_pendingFrames];
            for (size_t i = 0; i < slice; i++) {
                pending[i] = src[i] * std::numeric_limits<short>::max();
            }
            std::copy(src, src + slice, &channel.dryHistory[k_rnnoiseDelayBlocks * k_denoiseBlockSize + m_pendingFrames]);
        }

        m_pendingFrames += slice;
        if (m_pendingFrames == k_denoiseBlockSize) {
            processBlock(vadThreshold, vadGracePeriodBlocks, stats);
            m_pendingFrames = 0;
        }

        outputFrames(out, pos, slice, dryMix);
        pos += slice;
    }

    stats.blocksWaitingForOutput = static_cast<uint32_t>(m_queueSize);
    m_stats.store(stats);
}

void RnNoiseCommonPlugin::processBlock(float vadThreshold, uint32_t vadGracePeriodBlocks, RnNoiseStats &stats) {
    assert(m_queueSize < k_queueCapacity);

    const size_t newPos = m_queueSize;
    m_queueSize++;

    const uint64_t blockIdx = m_newOutputIdx++;

    /* We either mute ALL channels or none, so we have to calculate the max VAD
     * probability across each output block.
     */
    float maxVadProbability = 0.f;
    for (auto &channel: m_channels) {
        OutputChunk &chunk = queueAt(newPos, channel.idx);
        chunk.idx = blockIdx;
        chunk.curOffset = 0;
        chunk.muteState = ChunkUnmuteState::UNMUTED_BY_DEFAULT;
        chunk.vadProbability = rnnoise_process_frame(channel.denoiseState.get(), chunk.frames,
                                                     channel.pendingInput.data());
        for (float &frame: chunk.frames) {
            frame /= std::numeric_limits<short>::max();
        }

        /* The oldest block in the history is the input for this RNNoise output. */
        std::copy(channel.dryHistory.begin(), channel.dryHistory.begin() + k_denoiseBlockSize, chunk.dryFrames);
        std::copy(channel.dryHistory.begin() + k_denoiseBlockSize, channel.dryHistory.end(),
                  channel.dryHistory.begin());

        maxVadProbability = std::max(chunk.vadProbability, maxVadProbability);
    }

    for (auto &channel: m_channels) {
        queueAt(newPos, channel.idx).maxVadProbability = maxVadProbability;
    }

    if (maxVadProbability >= vadThreshold) {
        m_lastOutputIdxOverVADThreshold = blockIdx;
        m_hasLastOutputIdxOverVADThreshold = true;

        /* Unmute the previous blocks in the retroactive grace period.
         * These blocks are always fully in the queue.
         */
        for (size_t back = 1; back <= m_retroactiveVADGraceBlocks && back <= newPos; back++) {
            if (queueAt(newPos - back, 0).muteState != ChunkUnmuteState::MUTED) {
                continue;
            }

            for (auto &channel: m_channels) {
                queueAt(newPos - back, channel.idx).muteState = ChunkUnmuteState::UNMUTED_RETRO_VAD;
            }
            stats.retroactiveVADGraceBlocks++;
        }
    } else {
        const bool inVadPeriod = m_hasLastOutputIdxOverVADThreshold
                                 && (blockIdx - m_lastOutputIdxOverVADThreshold) <= vadGracePeriodBlocks;
        for (auto &channel: m_channels) {
            queueAt(newPos, channel.idx).muteState =
                    inVadPeriod ? ChunkUnmuteState::UNMUTED_VAD : ChunkUnmuteState::MUTED;
        }

        if (inVadPeriod) {
            stats.vadGraceBlocks++;
        }
    }
}

void RnNoiseCommonPlugin::outputFrames(float **out, size_t outOffset, size_t frames, float dryMix) {
    const float wetMix = 1.f - dryMix;

    size_t written = 0;
    while (written < frames) {
        assert(m_queueSize > 0);

        size_t copyFromThisBlock = 0;
        for (auto &channel: m_channels) {
            OutputChunk &chunk = queueAt(0, channel.idx);
            copyFromThisBlock = std::min(k_denoiseBlockSize - chunk.curOffset, frames - written);

            float *dst = out[channel.idx] + outOffset + written;
            const float *wet = chunk.frames + chunk.curOffset;
            const float *dry = chunk.dryFrames + chunk.curOffset;
            const bool muted = chunk.muteState == ChunkUnmuteState::MUTED;

            if (dryMix == 1.f) {
                std::copy(dry, dry + copyFromThisBlock, dst);
            } else if (dryMix == 0.f) {
                if (muted) {
                    // TODO: Maybe we should output some noise instead? Make it an option?
                    std::fill(dst, dst + copyFromThisBlock, 0.f);
                } else {
                    std::copy(wet, wet + copyFromThisBlock, dst);
                }
            } else {
                for (size_t i = 0; i < copyFromThisBlock; i++) {
                    const float wetFrame = muted ? 0.f : wet[i];
                    dst[i] = wetFrame * wetMix + dry[i] * dryMix;
                }
            }

            chunk.curOffset += copyFromThisBlock;
        }

        written += copyFromThisBlock;
        if (queueAt(0, 0).curOffset == k_denoiseBlockSize) {
            m_queueHead = (m_queueHead + 1) % k_queueCapacity;
            m_queueSize--;
        }
    }
}

void RnNoiseCommonPlugin::dropFrames(size_t frames) {
    while (frames > 0) {
        assert(m_queueSize > 0);

        size_t dropFromThisBlock = 0;
        for (auto &channel: m_channels) {
            OutputChunk &chunk = queueAt(0, channel.idx);
            dropFromThisBlock = std::min(k_denoiseBlockSize - chunk.curOffset, frames);
            chunk.curOffset += dropFromThisBlock;
        }

        frames -= dropFromThisBlock;
        if (queueAt(0, 0).curOffset == k_denoiseBlockSize) {
            m_queueHead = (m_queueHead + 1) % k_queueCapacity;
            m_queueSize--;
        }
    }
}

void RnNoiseCommonPlugin::setRetroactiveVADGraceBlocks(uint32_t retroactiveVADGraceBlocks) {
    retroactiveVADGraceBlocks = std::min(retroactiveVADGraceBlocks, k_maxRetroactiveVADGraceBlocks);

    /* The latency follows the parameter. When the latency increases, add silence.
     * When the latency decreases, remove the oldest output. The RNNoise state does not change.
     */
    for (; m_retroactiveVADGraceBlocks < retroactiveVADGraceBlocks; m_retroactiveVADGraceBlocks++) {
        pushSilentChunkFront();
    }

    if (m_retroactiveVADGraceBlocks > retroactiveVADGraceBlocks) {
        dropFrames((m_retroactiveVADGraceBlocks - retroactiveVADGraceBlocks) * k_denoiseBlockSize);
        m_retroactiveVADGraceBlocks = retroactiveVADGraceBlocks;
    }
}

RnNoiseCommonPlugin::OutputChunk &RnNoiseCommonPlugin::queueAt(size_t idx, uint32_t channelIdx) {
    assert(idx < m_queueSize);
    return m_channels[channelIdx].queue[(m_queueHead + idx) % k_queueCapacity];
}

void RnNoiseCommonPlugin::pushSilentChunkFront() {
    assert(m_queueSize < k_queueCapacity);

    m_queueHead = (m_queueHead + k_queueCapacity - 1) % k_queueCapacity;
    m_queueSize++;

    for (auto &channel: m_channels) {
        OutputChunk &chunk = queueAt(0, channel.idx);
        chunk.idx = 0;
        chunk.vadProbability = 0.f;
        chunk.maxVadProbability = 0.f;
        chunk.muteState = ChunkUnmuteState::UNMUTED_BY_DEFAULT;
        std::fill(std::begin(chunk.frames), std::end(chunk.frames), 0.f);
        std::fill(std::begin(chunk.dryFrames), std::end(chunk.dryFrames), 0.f);
        chunk.curOffset = 0;
    }
}

void RnNoiseCommonPlugin::createDenoiseState() {
    m_newOutputIdx = 0;
    m_hasLastOutputIdxOverVADThreshold = false;
    m_lastOutputIdxOverVADThreshold = 0;
    m_retroactiveVADGraceBlocks = 0;
    m_pendingFrames = 0;
    m_queueHead = 0;
    m_queueSize = 0;

    for (uint32_t i = 0; i < m_channelCount; i++) {
        auto denoiseState = std::shared_ptr<DenoiseState>(rnnoise_create(nullptr), [](DenoiseState *st) {
            rnnoise_destroy(st);
        });

        ChannelData channel{i, denoiseState, {}, {}, {}};
        channel.pendingInput.assign(k_denoiseBlockSize, 0.f);
        channel.dryHistory.assign((k_rnnoiseDelayBlocks + 1) * k_denoiseBlockSize, 0.f);
        channel.queue.resize(k_queueCapacity);
        m_channels.push_back(std::move(channel));
    }

    /* One buffer block is necessary to write any number of frames to the output. */
    pushSilentChunkFront();
}

void RnNoiseCommonPlugin::resetStats() {
    m_stats.store(RnNoiseStats {});
}

const RnNoiseStats RnNoiseCommonPlugin::getStats() const {
    return m_stats.load();
}
