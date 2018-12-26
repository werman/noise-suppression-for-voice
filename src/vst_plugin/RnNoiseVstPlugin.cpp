#include "RnNoiseVstPlugin.h"

#include "common/RnNoiseCommonPlugin.h"

const char *RnNoiseVstPlugin::s_programName = "Noise Suppression Stereo";

RnNoiseVstPlugin::RnNoiseVstPlugin(audioMasterCallback audioMaster, VstInt32 numPrograms, VstInt32 numParams)
        : AudioEffectX(audioMaster, numPrograms, numParams) {
    setNumInputs(2); // stereo in
    setNumOutputs(2); // stereo out
    setUniqueID(366056);
    canProcessReplacing(); // supports replacing mode

    m_rnNoisePlugin0 = std::make_unique<RnNoiseCommonPlugin>();
    m_rnNoisePlugin1 = std::make_unique<RnNoiseCommonPlugin>();
}

RnNoiseVstPlugin::~RnNoiseVstPlugin() = default;

void RnNoiseVstPlugin::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) {
    // stereo in/out by duplicating mono channels
    float *inChannel0 = inputs[0];
    float *outChannel0 = outputs[0];

    float *inChannel1 = inputs[1];
    float *outChannel1 = outputs[1];

    // TODO can it be improved by considering both channels as correlated?
    m_rnNoisePlugin0->process(inChannel0, outChannel0, sampleFrames);
    m_rnNoisePlugin1->process(inChannel1, outChannel1, sampleFrames);
}

VstInt32 RnNoiseVstPlugin::startProcess() {
    m_rnNoisePlugin0->init();
    m_rnNoisePlugin1->init();

    return AudioEffectX::startProcess();
}

VstInt32 RnNoiseVstPlugin::stopProcess() {
    m_rnNoisePlugin0->deinit();
    m_rnNoisePlugin1->deinit();

    return AudioEffectX::stopProcess();
}

void RnNoiseVstPlugin::getProgramName(char *name) {
    strcpy(name, s_programName);
}

extern AudioEffect *createEffectInstance(audioMasterCallback audioMaster) {
    return new RnNoiseVstPlugin(audioMaster, 0, 0);
}