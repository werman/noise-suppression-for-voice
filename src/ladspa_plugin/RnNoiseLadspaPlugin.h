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

    constexpr static port_info_t vad_grace_period_input = {
            "VAD Grace period (10ms steps)",
            "Grace period after VAD activation before silence will be returned",
            port_types::input | port_types::control,
            {
                    port_hints::bounded_below | port_hints::bounded_above | port_hints::integer |
                    port_hints::default_minimum,
                    20.f,
                    100.f
            }
    };
}

struct RnNoiseMono {
    enum class port_names {
        in_1,
        out_1,
        in_vad_threshold,
        in_vad_grace_period,
        size
    };

    static constexpr port_info_t port_info[] =
            {
                    port_info_common::audio_input,
                    port_info_common::audio_output,
                    port_info_custom::vad_threshold_input,
                    port_info_custom::vad_grace_period_input,
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

    RnNoiseMono() {
        m_rnNoisePlugin.init();
    }

    ~RnNoiseMono() {
        m_rnNoisePlugin.deinit();
    }

    void run(port_array_t<port_names, port_info> &ports) {
        const_buffer in_buffer = ports.get<port_names::in_1>();
        buffer out_buffer = ports.get<port_names::out_1>();
        uint32_t vad_threshold = ports.get<port_names::in_vad_threshold>();
        short vad_grace_period = ports.get<port_names::in_vad_grace_period>();

        float vad_threshold_normalized = std::max(std::min(vad_threshold / 100.f, 0.99f), 0.f);

        m_rnNoisePlugin.process(in_buffer.data(), out_buffer.data(), in_buffer.size(), vad_threshold_normalized, vad_grace_period);
    }

    RnNoiseCommonPlugin m_rnNoisePlugin;
};

struct RnNoiseStereo {
    enum class port_names {
        in_1,
        in_r,
        out_1,
        out_r,
        in_vad_threshold,
        in_vad_grace_period,
        size
    };

    static constexpr port_info_t port_info[] =
            {
                    port_info_common::audio_input_l,
                    port_info_common::audio_input_r,
                    port_info_common::audio_output_l,
                    port_info_common::audio_output_r,
                    port_info_custom::vad_threshold_input,
                    port_info_custom::vad_grace_period_input,
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

    RnNoiseStereo() {
        m_rnNoisePluginL.init();
        m_rnNoisePluginR.init();
    }

    ~RnNoiseStereo() {
        m_rnNoisePluginL.deinit();
        m_rnNoisePluginR.deinit();
    }

    void run(port_array_t<port_names, port_info> &ports) {
        const_buffer in_buffer_l = ports.get<port_names::in_1>();
        const_buffer in_buffer_r = ports.get<port_names::in_r>();

        buffer out_buffer_l = ports.get<port_names::out_1>();
        buffer out_buffer_r = ports.get<port_names::out_r>();

        uint32_t vad_threshold = ports.get<port_names::in_vad_threshold>();

        float vad_threshold_normalized = std::max(std::min(vad_threshold / 100.f, 0.99f), 0.f);

        short vad_grace_period = ports.get<port_names::in_vad_grace_period>();

        m_rnNoisePluginL.process(in_buffer_l.data(), out_buffer_l.data(), in_buffer_l.size(), vad_threshold_normalized, vad_grace_period);
        m_rnNoisePluginR.process(in_buffer_r.data(), out_buffer_r.data(), in_buffer_r.size(), vad_threshold_normalized, vad_grace_period);
    }

    RnNoiseCommonPlugin m_rnNoisePluginL;
    RnNoiseCommonPlugin m_rnNoisePluginR;
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