#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "common/RnNoiseCommonPlugin.h"

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
