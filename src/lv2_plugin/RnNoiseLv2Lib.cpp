#include "RnNoiseLv2Lib.h"

LV2_SYMBOL_EXTERN const LV2_Lib_Descriptor *
lv2_lib_descriptor(const char *bundle_path,
                   const LV2_Feature *const *features) {
    return new RnNoiseLib(bundle_path, features);
}

RnNoiseLib::RnNoiseLib(const char *bundle_path, const LV2_Feature *const *features)
        : Lib(bundle_path, features),
          m_rnNoisePluginDescriptor("https://github.com/werman/noise-suppression-for-voice") {
}

const LV2_Descriptor *RnNoiseLib::get_plugin(uint32_t index) {
    if (index == 0) {
        return &m_rnNoisePluginDescriptor;
    }
    return nullptr;
}
