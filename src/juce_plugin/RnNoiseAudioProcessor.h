#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class RnNoiseCommonPlugin;

class RnNoiseAudioProcessor : public juce::AudioProcessor {
public:
    RnNoiseAudioProcessor();

    ~RnNoiseAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;

    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;

    bool producesMidi() const override;

    double getTailLengthSeconds() const override;

    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram(int index) override;

    const juce::String getProgramName(int index) override;

    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;

    void setStateInformation(const void *data, int sizeInBytes) override;

public:

    juce::AudioProcessorValueTreeState m_parameters;

    juce::AudioParameterFloat* m_vadThresholdParam;
    juce::AudioParameterInt* m_vadGracePeriodParam;
    juce::AudioParameterInt* m_vadRetroactiveGracePeriodParam;

    std::shared_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RnNoiseAudioProcessor)
};