#pragma once

#include "ladspa++.h"
#include "common/RnNoiseCommonPlugin.h"

using namespace ladspa;

struct RnNoise {
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
                    "noise_suppressor",
                    properties::realtime,
                    "Noise Suppressor for Voice",
                    "werman",
                    "Removes wide range of noises from voice in real time, based on Xiph's RNNoise library.",
                    {"voice", "noise suppression", "de-noise"},
                    strings::copyright::gpl3,
                    nullptr // implementation data
            };

    RnNoise() {
        m_rnNoisePlugin.init();
    }

    ~RnNoise() {
        m_rnNoisePlugin.deinit();
    }

    void run(port_array_t<port_names, port_info> &ports) {

		const_buffer in_buffer = ports.get<port_names::in_1>();
		buffer out_buffer = ports.get<port_names::out_1>();

        m_rnNoisePlugin.process(in_buffer.data(), out_buffer.data(), in_buffer.size());
    }

    RnNoiseCommonPlugin m_rnNoisePlugin;
};

/*
 * to be called by ladspa
 */

void _init() {}

void _fini() {}

const LADSPA_Descriptor *
ladspa_descriptor(plugin_index_t index) {
    return collection<RnNoise>::get_ladspa_descriptor(index);
}