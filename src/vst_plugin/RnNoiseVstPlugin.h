#pragma once

#include <memory>
#include <vector>
#include "vst2.x/audioeffectx.h"

class RnNoiseCommonPlugin;

class RnNoiseVstPlugin : public AudioEffectX {
public:

    RnNoiseVstPlugin(audioMasterCallback audioMaster, VstInt32 numPrograms, VstInt32 numParams);

    ~RnNoiseVstPlugin() override;

    VstInt32 startProcess() override;
    void resume () override;
    VstInt32 stopProcess() override;

    void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override;

    void getProgramName(char *name) override;

private:
    static const char* s_programName;

    std::unique_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin;
};