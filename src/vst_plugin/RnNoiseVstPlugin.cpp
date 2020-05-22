#include "RnNoiseVstPlugin.h"

#include <cstdio>

#include "common/RnNoiseCommonPlugin.h"

const char *RnNoiseVstPlugin::s_programName = "Noise Suppression";

RnNoiseVstPlugin::RnNoiseVstPlugin(audioMasterCallback audioMaster, VstInt32 numPrograms, VstInt32 numParams)
        : AudioEffectX(audioMaster, numPrograms, numParams) {
    setNumInputs(1); // mono in
    setNumOutputs(1); // mono out
    setUniqueID(366056);
    canProcessReplacing(); // supports replacing mode

    m_rnNoisePlugin = std::make_unique<RnNoiseCommonPlugin>();
}

RnNoiseVstPlugin::~RnNoiseVstPlugin() = default;

void RnNoiseVstPlugin::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) {
    // Mono in/out only
    float *inChannel0 = inputs[0];
    float *outChannel0 = outputs[0];

    m_rnNoisePlugin->process(inChannel0, outChannel0, sampleFrames, paramVadThreshold);
}

VstInt32 RnNoiseVstPlugin::startProcess() {
    m_rnNoisePlugin->init();

    return AudioEffectX::startProcess();
}

VstInt32 RnNoiseVstPlugin::stopProcess() {
    m_rnNoisePlugin->deinit();

    return AudioEffectX::stopProcess();
}

void RnNoiseVstPlugin::getProgramName(char *name) {
    strcpy(name, s_programName);
}

extern AudioEffect *createEffectInstance(audioMasterCallback audioMaster) {
    return new RnNoiseVstPlugin(audioMaster, 0, RnNoiseVstPlugin::numParameters);
}

void RnNoiseVstPlugin::getParameterLabel(VstInt32 index, char* label) {
    const auto paramIdx = static_cast<Parameters>(index);
    switch (paramIdx) {
        case Parameters::vadThreshold:
            strcpy(label, paramVadThresholdLabel);
            break;
    }
}

void RnNoiseVstPlugin::getParameterName(VstInt32 index, char* label) {
    const auto paramIdx = static_cast<Parameters>(index);
    switch (paramIdx) {
        case Parameters::vadThreshold:
            strcpy(label, paramVadThresholdName);
            break;
    }
}

void RnNoiseVstPlugin::getParameterDisplay(VstInt32 index, char* label) {
    const auto paramIdx = static_cast<Parameters>(index);
    switch (paramIdx) {
        case Parameters::vadThreshold:
            snprintf(label, VstStringConstants::kVstMaxParamStrLen, "%.3f", paramVadThreshold);
            break;
    }
}

float RnNoiseVstPlugin::getParameter(VstInt32 index) {
    const auto paramIdx = static_cast<Parameters>(index);
    switch (paramIdx) {
        case Parameters::vadThreshold: return paramVadThreshold;
    }
    return 1;
}

void RnNoiseVstPlugin::setParameter(VstInt32 index, float value) {
    const auto paramIdx = static_cast<Parameters>(index);
    switch (paramIdx)
    {
        case Parameters::vadThreshold:
            paramVadThreshold = value;
            break;
    }
}