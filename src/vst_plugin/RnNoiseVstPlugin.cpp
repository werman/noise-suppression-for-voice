#include "RnNoiseVstPlugin.h"

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

    m_rnNoisePlugin->process(inChannel0, outChannel0, sampleFrames);
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
    return new RnNoiseVstPlugin(audioMaster, 0, 0);
}