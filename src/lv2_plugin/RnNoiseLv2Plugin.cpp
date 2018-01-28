#include "RnNoiseLv2Plugin.h"

#include "common/RnNoiseCommonPlugin.h"

RnNoiseLv2Plugin::RnNoiseLv2Plugin(double sample_rate, const char *bundle_path, const LV2_Feature *const *features,
                                   bool *valid) : Plugin(sample_rate, bundle_path, features, valid) {
    (*valid) = true;

    m_rnNoisePlugin = std::make_unique<RnNoiseCommonPlugin>();
}


RnNoiseLv2Plugin::~RnNoiseLv2Plugin() = default;

void RnNoiseLv2Plugin::connect_port(uint32_t port, void *data_location) {
    PluginBase::connect_port(port, data_location);

    const auto portIdx = static_cast<PortIndex>(port);

    switch (portIdx) {
        case PortIndex::input: {
            m_inPort = static_cast<float *>(data_location);
            break;
        }
        case PortIndex::output: {
            m_outPort = static_cast<float *>(data_location);
            break;
        }
    }
}

void RnNoiseLv2Plugin::activate() {
    PluginBase::activate();

    m_rnNoisePlugin->init();
}

void RnNoiseLv2Plugin::run(uint32_t sample_count) {
    PluginBase::run(sample_count);

    if (m_inPort != nullptr && m_outPort != nullptr) {
        m_rnNoisePlugin->process(m_inPort, m_outPort, sample_count);
    }
}

void RnNoiseLv2Plugin::deactivate() {
    PluginBase::deactivate();

    m_rnNoisePlugin->deinit();
}
