#pragma once

#include <memory>
#include "lv2/lv2plug.in/ns/lv2core/Lib.hpp"

#include "RnNoiseLv2Plugin.h"

class RnNoiseLib : public lv2::Lib {
public:
    RnNoiseLib(const char *bundle_path, const LV2_Feature *const *features);

    const LV2_Descriptor *get_plugin(uint32_t index) override;

private:

    const lv2::Descriptor<RnNoiseLv2Plugin> m_rnNoisePluginDescriptor;
};



