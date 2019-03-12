#pragma once

#include "ladspa++.h"
#include "common/RnNoiseCommonPlugin.h"

using namespace ladspa;

struct RnNoiseMono {
    enum class port_names {
        in_1,
        out_1,
        size
    };

    static constexpr port_info_t port_info[] =
            {
                    port_info_common::audio_input,
                    port_info_common::audio_output,
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

    RnNoiseMono(sample_rate_t sr) {
        m_rnNoisePlugin.init();
        m_rnNoisePlugin.setSampleRate(sr);
    }

    ~RnNoiseMono() {
        m_rnNoisePlugin.deinit();
    }

    void run(port_array_t<port_names, port_info> &ports) {

		const_buffer in_buffer = ports.get<port_names::in_1>();
		buffer out_buffer = ports.get<port_names::out_1>();

        m_rnNoisePlugin.process(in_buffer.data(), out_buffer.data(), in_buffer.size());
    }

    RnNoiseCommonPlugin m_rnNoisePlugin;
};

struct RnNoiseStereo {
    enum class port_names {
        in_1,
        in_r,
        out_1,
        out_r,
        size
    };

    static constexpr port_info_t port_info[] =
            {
                    port_info_common::audio_input_l,
                    port_info_common::audio_input_r,
                    port_info_common::audio_output_l,
                    port_info_common::audio_output_r,
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

    RnNoiseStereo(sample_rate_t sr) {
        m_rnNoisePluginL.init();
        m_rnNoisePluginR.init();
        m_rnNoisePluginL.setSampleRate(sr);
        m_rnNoisePluginR.setSampleRate(sr);
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

        m_rnNoisePluginL.process(in_buffer_l.data(), out_buffer_l.data(), in_buffer_l.size());
        m_rnNoisePluginR.process(in_buffer_r.data(), out_buffer_r.data(), in_buffer_r.size());
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