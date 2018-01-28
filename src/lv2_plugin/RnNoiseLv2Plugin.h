#pragma once

#include <memory>
#include "lv2/lv2plug.in/ns/lv2core/Plugin.hpp"

class RnNoiseCommonPlugin;

class RnNoiseLv2Plugin : public lv2::Plugin<lv2::NoExtension> {
public:
    RnNoiseLv2Plugin(double sample_rate, const char *bundle_path, const LV2_Feature *const *features, bool *valid);
    ~RnNoiseLv2Plugin();

    void connect_port(uint32_t port, void *data_location) override;

    void activate() override;

    void run(uint32_t sample_count) override;

    void deactivate() override;

private:

    enum class PortIndex {
        input = 0,
        output,
    };

    const float *m_inPort{nullptr};
    float *m_outPort{nullptr};

    std::unique_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin;
};



