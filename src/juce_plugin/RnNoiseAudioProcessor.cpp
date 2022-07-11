#include "RnNoiseAudioProcessor.h"
#include "RnNoisePluginEditor.h"

#include "common/RnNoiseCommonPlugin.h"

//==============================================================================
RnNoiseAudioProcessor::RnNoiseAudioProcessor()
        : AudioProcessor(BusesProperties()
                                 .withInput("Input", juce::AudioChannelSet::namedChannelSet(NUM_CHANNELS), true)
                                 .withOutput("Output", juce::AudioChannelSet::namedChannelSet(NUM_CHANNELS), true)
), m_parameters(*this, nullptr, juce::Identifier("RNNoise"),
                {
                        std::make_unique<juce::AudioParameterFloat>("vad_threshold",
                                                                    "VAD Threshold",
                                                                    0.0f,
                                                                    1.0f,
                                                                    0.6f),
                        std::make_unique<juce::AudioParameterInt>("vad_grace_period",
                                                                  "VAD Grace Period (10ms per unit)",
                                                                  0,
                                                                  500,
                                                                  20),
                        std::make_unique<juce::AudioParameterInt>("vad_retroactive_grace_period",
                                                                  "Retroactive VAD Grace Period (10ms per unit)",
                                                                  0,
                                                                  10,
                                                                  0)
                }) {
    m_vadThresholdParam = (juce::AudioParameterFloat *) m_parameters.getParameter("vad_threshold");
    m_vadGracePeriodParam = (juce::AudioParameterInt *) m_parameters.getParameter("vad_grace_period");
    m_vadRetroactiveGracePeriodParam = (juce::AudioParameterInt *) m_parameters.getParameter(
            "vad_retroactive_grace_period");
}

RnNoiseAudioProcessor::~RnNoiseAudioProcessor() = default;

//==============================================================================
const juce::String RnNoiseAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool RnNoiseAudioProcessor::acceptsMidi() const {
    return false;
}

bool RnNoiseAudioProcessor::producesMidi() const {
    return false;
}

double RnNoiseAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int RnNoiseAudioProcessor::getNumPrograms() {
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int RnNoiseAudioProcessor::getCurrentProgram() {
    return 0;
}

void RnNoiseAudioProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String RnNoiseAudioProcessor::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
}

void RnNoiseAudioProcessor::changeProgramName(int index, const juce::String &newName) {
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void RnNoiseAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(sampleRate, samplesPerBlock);

    m_rnNoisePlugin = std::make_shared<RnNoiseCommonPlugin>(static_cast<uint32_t>(getTotalNumInputChannels()));
    m_rnNoisePlugin->init();
}

void RnNoiseAudioProcessor::releaseResources() {
    m_rnNoisePlugin.reset();
}

bool RnNoiseAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    if (layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void RnNoiseAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                         juce::MidiBuffer &midiMessages) {
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const float *in[8] = {nullptr};
    float *out[8] = {nullptr};
    for (int channel = 0; channel < totalNumInputChannels; ++channel) {
        in[channel] = buffer.getReadPointer(channel);
        out[channel] = buffer.getWritePointer(channel);
    }

    m_rnNoisePlugin->process(in, out, static_cast<size_t>(buffer.getNumSamples()), m_vadThresholdParam->get(),
                             static_cast<uint32_t>(m_vadGracePeriodParam->get()),
                             static_cast<uint32_t>(m_vadRetroactiveGracePeriodParam->get()));
}

//==============================================================================
bool RnNoiseAudioProcessor::hasEditor() const {
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *RnNoiseAudioProcessor::createEditor() {
    return new RnNoiseAudioProcessorEditor(*this, m_parameters);
}

//==============================================================================
void RnNoiseAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
    auto state = m_parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void RnNoiseAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (!xml || !xml->hasTagName(m_parameters.state.getType()))
        return;

    m_parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new RnNoiseAudioProcessor();
}