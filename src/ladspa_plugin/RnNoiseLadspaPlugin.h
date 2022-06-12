#pragma once

#include "ladspa++.h"
#include "common/RnNoiseCommonPlugin.h"

using namespace ladspa;

namespace port_info_custom {
    constexpr static port_info_t vad_threshold_input = {
            "VAD Threshold (%)",
            "If probability of sound being a voice is lower than this threshold - silence will be returned",
            port_types::input | port_types::control,
            {
                    port_hints::bounded_below | port_hints::bounded_above | port_hints::integer |
                    port_hints::default_low,
                    0.f,
                    99.f
            }
    };
    constexpr static port_info_t vad_grace_period_blocks_input = {
            "VAD Grace Period (blocks)",
            "For how many blocks output will not be silenced after a voice was detected last time.",
            port_types::input | port_types::control,
            {
                    port_hints::bounded_below | port_hints::bounded_above | port_hints::integer |
                    port_hints::default_low,
                    0.f,
                    50.f
            }
    };
    constexpr static port_info_t retroactive_vad_grace_blocks_input = {
            "Retroactive VAD Grace (blocks)",
            "If voice is detected in a sound block, "
            "a number of past blocks are also considered to be with voice. "
            "Causes increase of latency by that number of blocks.",
            port_types::input | port_types::control,
            {
                    port_hints::bounded_below | port_hints::bounded_above | port_hints::integer |
                    port_hints::default_low,
                    0.f,
                    50.f
            }
    };
    constexpr static port_info_t placeholder_input = {
            "Placeholder",
            "Currently unused.",
            port_types::input | port_types::control,
            {0, 0.f, 0.f}
    };
}

struct RnNoiseMono {
    enum class port_names {
        in_1,
        out_1,
        in_vad_threshold,
        in_vad_grace_period_blocks,
        in_retroactive_vad_grace_blocks,
        in_placeholder1,
        in_placeholder2,
        size
    };

    static constexpr port_info_t port_info[] =
            {
                    port_info_common::audio_input,
                    port_info_common::audio_output,
                    port_info_custom::vad_threshold_input,
                    port_info_custom::vad_grace_period_blocks_input,
                    port_info_custom::retroactive_vad_grace_blocks_input,
                    port_info_custom::placeholder_input,
                    port_info_custom::placeholder_input,
                    port_info_common::final_port
            };

    static constexpr info_t info =
            {
                    9354877, // unique id
                    "noise_suppressor_mono",
                    properties::realtime,
                    "Noise Suppressor for Voice (Mono)",
                    "werman",
                    "Removes wide range of noises from voice in real time, based on Xiph's RNNoise library.",
                    {"voice", "noise suppression", "de-noise"},
                    strings::copyright::gpl3,
                    nullptr // implementation data
            };

    explicit RnNoiseMono(sample_rate_t _sample_rate) {
        m_rnNoisePlugin = std::make_unique<RnNoiseCommonPlugin>(1);
        m_rnNoisePlugin->init();
    }

    ~RnNoiseMono() {
        m_rnNoisePlugin->deinit();
    }

    void run(port_array_t<port_names, port_info> &ports) const {
        const_buffer in_buffer = ports.get<port_names::in_1>();
        buffer out_buffer = ports.get<port_names::out_1>();
        uint32_t vad_threshold = ports.get<port_names::in_vad_threshold>();
        uint32_t vad_grace_period_blocks = ports.get<port_names::in_vad_grace_period_blocks>();
        uint32_t retroactive_vad_grace_blocks = ports.get<port_names::in_retroactive_vad_grace_blocks>();

        float vad_threshold_normalized = std::max(std::min(vad_threshold / 100.f, 0.99f), 0.f);

        const float *input[] = {in_buffer.data() };
        float *output[] = {out_buffer.data() };

        m_rnNoisePlugin->process(input, output, in_buffer.size(), vad_threshold_normalized,
                                 vad_grace_period_blocks, retroactive_vad_grace_blocks);
    }

    std::unique_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin;
};

struct RnNoiseStereo {
    enum class port_names {
        in_1,
        in_r,
        out_1,
        out_r,
        in_vad_threshold,
        in_vad_grace_period_blocks,
        in_retroactively_activated_blocks,
        in_placeholder1,
        in_placeholder2,
        size
    };

    static constexpr port_info_t port_info[] =
            {
                    port_info_common::audio_input_l,
                    port_info_common::audio_input_r,
                    port_info_common::audio_output_l,
                    port_info_common::audio_output_r,
                    port_info_custom::vad_threshold_input,
                    port_info_custom::vad_grace_period_blocks_input,
                    port_info_custom::retroactive_vad_grace_blocks_input,
                    port_info_custom::placeholder_input,
                    port_info_custom::placeholder_input,
                    port_info_common::final_port
            };

    static constexpr info_t info =
            {
                    9354877, // unique id
                    "noise_suppressor_stereo",
                    properties::realtime,
                    "Noise Suppressor for Voice (Stereo)",
                    "werman",
                    "Removes wide range of noises from voice in real time, based on Xiph's RNNoise library.",
                    {"voice", "noise suppression", "de-noise"},
                    strings::copyright::gpl3,
                    nullptr // implementation data
            };

    explicit RnNoiseStereo(sample_rate_t _sample_rate) {
        m_rnNoisePlugin = std::make_unique<RnNoiseCommonPlugin>(2);
        m_rnNoisePlugin->init();
    }

    ~RnNoiseStereo() {
        m_rnNoisePlugin->deinit();
    }

    void run(port_array_t<port_names, port_info> &ports) const {
        const_buffer in_buffer_l = ports.get<port_names::in_1>();
        const_buffer in_buffer_r = ports.get<port_names::in_r>();

        buffer out_buffer_l = ports.get<port_names::out_1>();
        buffer out_buffer_r = ports.get<port_names::out_r>();

        uint32_t vad_threshold = ports.get<port_names::in_vad_threshold>();
        uint32_t vad_grace_period_blocks = ports.get<port_names::in_vad_grace_period_blocks>();
        uint32_t retroactive_vad_grace_blocks = ports.get<port_names::in_retroactively_activated_blocks>();

        float vad_threshold_normalized = std::max(std::min(vad_threshold / 100.f, 0.99f), 0.f);

        const float *input[] = {in_buffer_l.data(), in_buffer_r.data()};
        float *output[] = {out_buffer_l.data(), out_buffer_r.data()};

        m_rnNoisePlugin->process(input, output, in_buffer_l.size(), vad_threshold_normalized,
                                 vad_grace_period_blocks, retroactive_vad_grace_blocks);
    }

    std::unique_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin;
};

/*
 * to be called by ladspa
 */

void _init() {}

void _fini() {}

const LADSPA_Descriptor *
ladspa_descriptor(plugin_index_t index) {
    return collection<RnNoiseMono, RnNoiseStereo>::get_ladspa_descriptor(index);
}