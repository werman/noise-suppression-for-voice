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

    VstInt32 stopProcess() override;

    void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override;

    void getProgramName(char *name) override;

    void getParameterLabel(VstInt32 index, char* label) override;

    void getParameterName(VstInt32 index, char* label) override;

    void getParameterDisplay(VstInt32 index, char* label) override;

    float getParameter(VstInt32 index) override;

    void setParameter(VstInt32 index, float value) override;

private:
    static const char* s_programName;

    #define NUM_PARAMS 1
    static const VstInt32 numParameters = NUM_PARAMS;
    
    #define PARAM_VAD_THRESHOLD 0
    const char* param_vadThreshold_label = "Voice Activity Detection Threshold";
    const char* param_vadThreshold_name = "VAD Threshold";
    float param_vadThreshold;

    std::unique_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin0;
    std::unique_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin1;
};