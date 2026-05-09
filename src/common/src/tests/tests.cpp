#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "common/RnNoiseCommonPlugin.h"

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

TEST_CASE("VAD grace absent state is restored when buffered output resets", "[common_plugin]") {
    MonoBuffers buffers(kDenoiseBlockSize);

    RnNoiseCommonPlugin plugin(1);
    plugin.init();

    plugin.process(buffers.inputs, buffers.outputs, kDenoiseBlockSize, 0.f, 20, 0);
    plugin.process(buffers.inputs, buffers.outputs, kDenoiseBlockSize, 1.f, 20, 0, 1.f);
    plugin.resetStats();
    plugin.process(buffers.inputs, buffers.outputs, kDenoiseBlockSize, 1.f, 20, 0);

    REQUIRE(plugin.getStats().vadGraceBlocks == 0);
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

TEST_CASE("Change Retroactive VAD", "[common_plugin]") {
    auto sampleFrames = 512;
    auto channels = 2;
    auto startRetroactiveVADGraceBlocks = 10;
    auto endRetroactiveVADGraceBlocks = 2;

    CAPTURE(startRetroactiveVADGraceBlocks, endRetroactiveVADGraceBlocks, channels, sampleFrames);

    RnNoiseCommonPlugin plugin(2);
    plugin.init();

    auto inputs = std::vector<const float *>();
    auto outputs = std::vector<float *>();

    uint32_t iterations = 10;
    for (int i = 0; i < channels; i++) {
        inputs.push_back(new float[sampleFrames * iterations]);
        outputs.push_back(new float[sampleFrames * iterations]);
    }

    for (int i = 0; i < startRetroactiveVADGraceBlocks; i++) {
        plugin.process(inputs.data(), outputs.data(), sampleFrames, 0.0,
                       2, startRetroactiveVADGraceBlocks);
    }

    for (int i = 0; i < endRetroactiveVADGraceBlocks; i++) {
        plugin.process(inputs.data(), outputs.data(), sampleFrames, 0.0,
                       2, endRetroactiveVADGraceBlocks);
    }

    const RnNoiseStats stats = plugin.getStats();
    REQUIRE(stats.blocksWaitingForOutput <= endRetroactiveVADGraceBlocks + 1);
}

TEST_CASE("Dry mix blends processed output with input", "[common_plugin]") {
    constexpr auto channels = 2;
    constexpr auto sampleFrames = 480;
    constexpr auto dryMix = 0.25f;

    std::vector<std::vector<float>> inputData(channels);
    std::vector<const float *> inputs;
    std::vector<std::vector<float>> wetOutputData(channels);
    std::vector<std::vector<float>> mixedOutputData(channels);
    std::vector<std::vector<float>> dryOutputData(channels);
    std::vector<float *> wetOutputs;
    std::vector<float *> mixedOutputs;
    std::vector<float *> dryOutputs;

    for (int ch = 0; ch < channels; ch++) {
        inputData[ch].resize(sampleFrames);
        wetOutputData[ch].resize(sampleFrames);
        mixedOutputData[ch].resize(sampleFrames);
        dryOutputData[ch].resize(sampleFrames);

        for (int frame = 0; frame < sampleFrames; frame++) {
            inputData[ch][frame] = 0.1f * static_cast<float>((frame % 17) - 8) / 8.f;
        }

        inputs.push_back(inputData[ch].data());
        wetOutputs.push_back(wetOutputData[ch].data());
        mixedOutputs.push_back(mixedOutputData[ch].data());
        dryOutputs.push_back(dryOutputData[ch].data());
    }

    RnNoiseCommonPlugin wetPlugin(channels);
    RnNoiseCommonPlugin mixedPlugin(channels);
    RnNoiseCommonPlugin dryPlugin(channels);
    wetPlugin.init();
    mixedPlugin.init();
    dryPlugin.init();

    wetPlugin.process(inputs.data(), wetOutputs.data(), sampleFrames, 0.f, 20, 0, 0.f);
    mixedPlugin.process(inputs.data(), mixedOutputs.data(), sampleFrames, 0.f, 20, 0, dryMix);
    dryPlugin.process(inputs.data(), dryOutputs.data(), sampleFrames, 0.f, 20, 0, 1.f);

    for (int ch = 0; ch < channels; ch++) {
        for (int frame = 0; frame < sampleFrames; frame++) {
            CAPTURE(ch, frame);

            REQUIRE(dryOutputData[ch][frame] == Approx(inputData[ch][frame]));

            const float expectedMixedOutput =
                    wetOutputData[ch][frame] * (1.f - dryMix) + inputData[ch][frame] * dryMix;
            REQUIRE(mixedOutputData[ch][frame] == Approx(expectedMixedOutput));
        }
    }
}

TEST_CASE("Dry-only mix bypasses RNNoise buffering for partial blocks", "[common_plugin]") {
    constexpr auto channels = 2;
    constexpr auto sampleFrames = 200;
    constexpr auto dryMix = 1.f;

    std::vector<std::vector<float>> inputData(channels);
    std::vector<std::vector<float>> outputData(channels);
    std::vector<const float *> inputs;
    std::vector<float *> outputs;

    for (int ch = 0; ch < channels; ch++) {
        inputData[ch].resize(sampleFrames);
        outputData[ch].resize(sampleFrames, -1.f);

        for (int frame = 0; frame < sampleFrames; frame++) {
            inputData[ch][frame] = 0.001f * static_cast<float>((ch + 1) * (frame + 1));
        }

        inputs.push_back(inputData[ch].data());
        outputs.push_back(outputData[ch].data());
    }

    RnNoiseCommonPlugin plugin(channels);
    plugin.init();

    plugin.process(inputs.data(), outputs.data(), sampleFrames, 0.f, 20, 0, dryMix);

    for (int ch = 0; ch < channels; ch++) {
        for (int frame = 0; frame < sampleFrames; frame++) {
            CAPTURE(ch, frame);
            REQUIRE(outputData[ch][frame] == Approx(inputData[ch][frame]));
        }
    }
}

TEST_CASE("Dry mix uses the same delayed timeline as wet output", "[common_plugin]") {
    constexpr auto channels = 1;
    constexpr auto sampleFrames = 200;
    constexpr auto calls = 6;
    constexpr auto dryMix = 0.25f;
    constexpr auto wetMix = 1.f - dryMix;

    std::vector<float> inputData(sampleFrames * calls);
    std::vector<float> wetOutputData(sampleFrames);
    std::vector<float> mixedOutputData(sampleFrames);

    for (int frame = 0; frame < sampleFrames * calls; frame++) {
        inputData[frame] = 0.0001f * static_cast<float>(frame + 1);
    }

    RnNoiseCommonPlugin wetPlugin(channels);
    RnNoiseCommonPlugin mixedPlugin(channels);
    wetPlugin.init();
    mixedPlugin.init();

    for (int call = 0; call < calls; call++) {
        const float *inputs[] = {inputData.data() + sampleFrames * call};
        float *wetOutputs[] = {wetOutputData.data()};
        float *mixedOutputs[] = {mixedOutputData.data()};

        wetPlugin.process(inputs, wetOutputs, sampleFrames, 0.f, 20, 0, 0.f);
        mixedPlugin.process(inputs, mixedOutputs, sampleFrames, 0.f, 20, 0, dryMix);

        for (int frame = 0; frame < sampleFrames; frame++) {
            CAPTURE(call, frame);

            float expectedDryOutput = 0.f;
            if (call >= 2) {
                expectedDryOutput = inputData[sampleFrames * (call - 2) + frame];
            }

            const float expectedMixedOutput =
                    wetOutputData[frame] * wetMix + expectedDryOutput * dryMix;
            REQUIRE(mixedOutputData[frame] == Approx(expectedMixedOutput));
        }
    }
}
