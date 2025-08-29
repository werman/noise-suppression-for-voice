#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

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
    juce::AudioParameterFloat* m_dryWetParam;

    std::shared_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin;

    // Dry path delay to align with wet path latency for proper Dry/Wet mix
    std::vector<std::vector<float>> m_dryDelayBuffers;   // per-channel circular buffers
    std::vector<size_t> m_dryDelayWritePos;              // per-channel write indices
    std::vector<size_t> m_dryDelayFilled;                // per-channel filled sample counts (up to capacity)
    size_t m_dryDelayCapacity = 0;                       // capacity per channel

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RnNoiseAudioProcessor)
};