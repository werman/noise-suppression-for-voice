#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "common/RnNoiseCommonPlugin.h"

#include <cmath>
#include <random>

namespace {

constexpr size_t kDenoiseBlockSize = 480;

struct MonoBuffers {
    explicit MonoBuffers(size_t sampleFrames) :
            input(sampleFrames, 0.f),
            output(sampleFrames, 0.f),
            inputs{input.data()},
            outputs{output.data()} {}

    std::vector<float> input;
    std::vector<float> output;
    const float *inputs[1];
    float *outputs[1];
};

using Signal = std::vector<std::vector<float>>;

Signal makeSignal(size_t channels, size_t frames, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> noise(-0.05f, 0.05f);

    Signal signal(channels, std::vector<float>(frames));
    for (size_t ch = 0; ch < channels; ch++) {
        for (size_t frame = 0; frame < frames; frame++) {
            const float tone = 0.3f * std::sin(0.06f * static_cast<float>(frame) * static_cast<float>(ch + 1));
            signal[ch][frame] = tone + noise(rng);
        }
    }
    return signal;
}

/* Send the full signal through the plugin. Split it into calls of the given sizes.
 * Repeat the sizes as necessary.
 */
Signal processInBlocks(RnNoiseCommonPlugin &plugin, const Signal &input, const std::vector<size_t> &blockSizes,
                       float vadThreshold, uint32_t vadGracePeriodBlocks, uint32_t retroactiveVADGraceBlocks,
                       float dryMix) {
    const size_t channels = input.size();
    const size_t frames = input[0].size();

    Signal output(channels, std::vector<float>(frames, -1.f));
    std::vector<const float *> inputs(channels);
    std::vector<float *> outputs(channels);

    size_t pos = 0;
    for (size_t call = 0; pos < frames; call++) {
        const size_t sampleFrames = std::min(blockSizes[call % blockSizes.size()], frames - pos);
        for (size_t ch = 0; ch < channels; ch++) {
            inputs[ch] = input[ch].data() + pos;
            outputs[ch] = output[ch].data() + pos;
        }

        plugin.process(inputs.data(), outputs.data(), sampleFrames, vadThreshold, vadGracePeriodBlocks,
                       retroactiveVADGraceBlocks, dryMix);
        pos += sampleFrames;
    }

    return output;
}

std::vector<size_t> randomBlockSizes(uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> small(1, 1100);
    std::vector<size_t> sizes;
    for (int i = 0; i < 64; i++) {
        sizes.push_back(small(rng));
    }
    /* Offline hosts can send very large blocks. */
    sizes.push_back(30000);
    sizes.push_back(480);
    sizes.push_back(1);
    return sizes;
}

void requireDelayedCopy(const Signal &input, const Signal &output, size_t delay, size_t from = 0) {
    for (size_t ch = 0; ch < input.size(); ch++) {
        for (size_t frame = from; frame < output[ch].size(); frame++) {
            const float expected = frame >= delay ? input[ch][frame - delay] : 0.f;
            if (output[ch][frame] != expected) {
                CAPTURE(ch, frame, delay, output[ch][frame], expected);
                FAIL("Output is not input delayed by the reported latency");
            }
        }
    }
}

} // namespace

TEST_CASE("Init -> Deinit cycle", "[common_plugin]") {
    auto channels = GENERATE(1, 2, 4);

    CAPTURE(channels);

    RnNoiseCommonPlugin plugin(channels);

    SECTION("init -> deinit") {
        plugin.init();
        plugin.deinit();
    }

    SECTION("repeated init") {
        plugin.init();
        plugin.init();
        plugin.init();
    }
}

TEST_CASE("VAD grace starts only after a voiced block", "[common_plugin]") {
    MonoBuffers buffers(kDenoiseBlockSize);

    RnNoiseCommonPlugin plugin(1);
    plugin.init();

    for (int i = 0; i < 3; i++) {
        plugin.process(buffers.inputs, buffers.outputs, kDenoiseBlockSize, 1.f, 20, 0);
    }

    REQUIRE(plugin.getStats().vadGraceBlocks == 0);

    plugin.process(buffers.inputs, buffers.outputs, kDenoiseBlockSize, 0.f, 20, 0);
    plugin.resetStats();
    plugin.process(buffers.inputs, buffers.outputs, kDenoiseBlockSize, 1.f, 20, 0);

    REQUIRE(plugin.getStats().vadGraceBlocks == 1);
}

TEST_CASE("Retroactive VAD grace starts only after a voiced block", "[common_plugin]") {
    MonoBuffers buffers(kDenoiseBlockSize);

    RnNoiseCommonPlugin plugin(1);
    plugin.init();

    plugin.process(buffers.inputs, buffers.outputs, kDenoiseBlockSize, 1.f, 20, 3);

    REQUIRE(plugin.getStats().retroactiveVADGraceBlocks == 0);
}

TEST_CASE("All options", "[common_plugin]") {
    auto channels = GENERATE(1, 2, 4);
    auto retroactiveVADGraceBlocks = GENERATE(0, 1, 3);
    auto blockPerCall = GENERATE(1, 10);
    // It's not a meaningful threshold test since the input is just a noise
    auto vadThreshold = GENERATE(0.f, 0.5f, 0.99f);
    auto vadGracePeriodBlocks = GENERATE(0, 20);
    /* 200 - Audacity (actually it has variable block size)
     * 480 - PulseAudio
     * 512 - Pipewire
     */
    auto sampleFrames = GENERATE(200, 480, 512);

    CAPTURE(channels, blockPerCall, retroactiveVADGraceBlocks, vadThreshold, vadGracePeriodBlocks, sampleFrames);

    RnNoiseCommonPlugin plugin(channels);
    plugin.init();

    auto inputs = std::vector<const float *>();
    auto outputs = std::vector<float *>();

    uint32_t iterations = 10 / blockPerCall;
    for (int i = 0; i < channels; i++) {
        inputs.push_back(new float[sampleFrames * blockPerCall * iterations]);
        outputs.push_back(new float[sampleFrames * blockPerCall * iterations]);
    }

    for (uint32_t i = 0; i < iterations; i++) {
        for (int ch = 0; ch < channels; ch++) {
            std::fill(&outputs[ch][0], &outputs[ch][sampleFrames * blockPerCall], -1.f);
        }

        plugin.process(inputs.data(), outputs.data(), sampleFrames * blockPerCall, vadThreshold,
                       vadGracePeriodBlocks, retroactiveVADGraceBlocks);

        for (int ch = 0; ch < channels; ch++) {
            for (int j = 0; j < sampleFrames; j++) {
                if (outputs[ch][j] == -1.f) {
                    CAPTURE(ch, j);
                    FAIL("No output written");
                }
            }
        }
    }

    plugin.deinit();

    for (int i = 0; i < channels; i++) {
        delete[] inputs[i];
        delete[] outputs[i];
    }
}

TEST_CASE("Latency is constant for any block sizes", "[common_plugin][latency]") {
    auto channels = GENERATE(1, 2);
    auto retroactiveVADGraceBlocks = GENERATE(0u, 1u, 3u);
    auto seed = GENERATE(1u, 2u, 3u);

    CAPTURE(channels, retroactiveVADGraceBlocks, seed);

    const Signal input = makeSignal(channels, 48000 * 3, seed);

    RnNoiseCommonPlugin plugin(channels);
    plugin.init();

    /* With dry mix 1, the output is an exact copy of the delayed input. Use it to measure the delay. */
    const Signal output = processInBlocks(plugin, input, randomBlockSizes(seed), 0.f, 20,
                                          retroactiveVADGraceBlocks, 1.f);

    requireDelayedCopy(input, output, RnNoiseCommonPlugin::getLatencySamples(retroactiveVADGraceBlocks));
}

TEST_CASE("Processed output does not depend on block sizes", "[common_plugin][latency]") {
    auto channels = GENERATE(1, 2);
    auto retroactiveVADGraceBlocks = GENERATE(0u, 2u);
    auto vadThreshold = GENERATE(0.f, 0.5f);
    auto dryMix = GENERATE(0.f, 0.3f);

    CAPTURE(channels, retroactiveVADGraceBlocks, vadThreshold, dryMix);

    const Signal input = makeSignal(channels, 48000 * 2, 7);

    RnNoiseCommonPlugin reference(channels);
    RnNoiseCommonPlugin randomBlocks(channels);
    reference.init();
    randomBlocks.init();

    const Signal expected = processInBlocks(reference, input, {480}, vadThreshold, 20,
                                            retroactiveVADGraceBlocks, dryMix);
    const Signal actual = processInBlocks(randomBlocks, input, randomBlockSizes(11), vadThreshold, 20,
                                          retroactiveVADGraceBlocks, dryMix);

    for (size_t ch = 0; ch < input.size(); ch++) {
        for (size_t frame = 0; frame < input[ch].size(); frame++) {
            if (actual[ch][frame] != expected[ch][frame]) {
                CAPTURE(ch, frame, actual[ch][frame], expected[ch][frame]);
                FAIL("Output differs between block sizes");
            }
        }
    }
}

TEST_CASE("Change Retroactive VAD", "[common_plugin][latency]") {
    auto startRetroactiveVADGraceBlocks = GENERATE(0u, 2u, 10u);
    auto endRetroactiveVADGraceBlocks = GENERATE(0u, 2u, 10u);
    constexpr auto channels = 2;

    CAPTURE(startRetroactiveVADGraceBlocks, endRetroactiveVADGraceBlocks);

    const Signal input = makeSignal(channels, 48000 * 2, 5);
    const size_t switchFrame = 48000;

    RnNoiseCommonPlugin plugin(channels);
    plugin.init();

    std::vector<const float *> inputs(channels);
    std::vector<float *> outputs(channels);
    Signal output(channels, std::vector<float>(input[0].size()));

    const std::vector<size_t> blockSizes = randomBlockSizes(3);
    size_t pos = 0;
    for (size_t call = 0; pos < input[0].size(); call++) {
        const size_t sampleFrames = std::min(blockSizes[call % blockSizes.size()] % 1000 + 1,
                                             input[0].size() - pos);
        const uint32_t retro = pos < switchFrame ? startRetroactiveVADGraceBlocks : endRetroactiveVADGraceBlocks;
        for (int ch = 0; ch < channels; ch++) {
            inputs[ch] = input[ch].data() + pos;
            outputs[ch] = output[ch].data() + pos;
        }
        plugin.process(inputs.data(), outputs.data(), sampleFrames, 0.f, 20, retro, 1.f);
        pos += sampleFrames;
    }

    /* After the change, the output must have the new latency. */
    const size_t endLatency = RnNoiseCommonPlugin::getLatencySamples(endRetroactiveVADGraceBlocks);
    requireDelayedCopy(input, output, endLatency, switchFrame + 1000 + 10 * 480 + 480);

    REQUIRE(plugin.getStats().blocksWaitingForOutput <= endRetroactiveVADGraceBlocks + 2);
}

TEST_CASE("Dry mix blends processed output with delayed input", "[common_plugin]") {
    constexpr auto channels = 2;
    constexpr auto dryMix = 0.25f;

    const Signal input = makeSignal(channels, 48000, 9);
    const std::vector<size_t> blockSizes = {200, 512, 37};

    RnNoiseCommonPlugin wetPlugin(channels);
    RnNoiseCommonPlugin mixedPlugin(channels);
    wetPlugin.init();
    mixedPlugin.init();

    const Signal wet = processInBlocks(wetPlugin, input, blockSizes, 0.f, 20, 0, 0.f);
    const Signal mixed = processInBlocks(mixedPlugin, input, blockSizes, 0.f, 20, 0, dryMix);

    const size_t latency = RnNoiseCommonPlugin::getLatencySamples(0);
    for (int ch = 0; ch < channels; ch++) {
        for (size_t frame = 0; frame < input[ch].size(); frame++) {
            CAPTURE(ch, frame);

            const float dry = frame >= latency ? input[ch][frame - latency] : 0.f;
            REQUIRE(mixed[ch][frame] == Approx(wet[ch][frame] * (1.f - dryMix) + dry * dryMix).margin(1e-6));
        }
    }
}

TEST_CASE("Reset clears queued audio and RNNoise state", "[common_plugin][latency]") {
    constexpr auto channels = 2;
    auto retroactiveBlocks = GENERATE(0u, 3u);

    CAPTURE(retroactiveBlocks);

    /* 1000 frames is not a full number of blocks. Thus some input is pending at the reset. */
    const Signal before = makeSignal(channels, 1000, 17);
    const Signal after = makeSignal(channels, 48000, 19);
    const std::vector<size_t> blockSizes = {200, 512, 37};

    RnNoiseCommonPlugin freshPlugin(channels);
    RnNoiseCommonPlugin resetPlugin(channels);
    freshPlugin.init();
    resetPlugin.init();

    processInBlocks(resetPlugin, before, blockSizes, 0.f, 20, retroactiveBlocks, 0.f);
    resetPlugin.reset();

    /* After the reset, the output must be the same as the output of a new plugin. */
    const Signal expected = processInBlocks(freshPlugin, after, blockSizes, 0.f, 20, retroactiveBlocks, 0.3f);
    const Signal actual = processInBlocks(resetPlugin, after, blockSizes, 0.f, 20, retroactiveBlocks, 0.3f);

    /* The output must not be silent. Otherwise the test checks nothing. */
    float energy = 0.f;
    for (float frame: expected[0]) {
        energy += frame * frame;
    }
    REQUIRE(energy > 1.f);

    for (int ch = 0; ch < channels; ch++) {
        for (size_t frame = 0; frame < after[ch].size(); frame++) {
            if (actual[ch][frame] != expected[ch][frame]) {
                CAPTURE(ch, frame, actual[ch][frame], expected[ch][frame]);
                FAIL("Output after reset() is not the output of a new plugin");
            }
        }
    }
}
