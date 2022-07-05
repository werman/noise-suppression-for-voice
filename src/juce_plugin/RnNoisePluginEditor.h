#pragma once

#include "RnNoiseAudioProcessor.h"

//==============================================================================
class RnNoiseAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer {
public:
    explicit RnNoiseAudioProcessorEditor(RnNoiseAudioProcessor &p,
                                         juce::AudioProcessorValueTreeState &vts);

    ~RnNoiseAudioProcessorEditor() override;

    void resized() override;

    void paint(juce::Graphics &g) override;

    void visibilityChanged() override;

    void timerCallback() override;

private:
    typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

    juce::AudioProcessorValueTreeState &m_valueTreeState;

    juce::Label m_headerLabel;

    juce::Label m_vadThresholdLabel;
    juce::Slider m_vadThresholdSlider;
    std::unique_ptr<SliderAttachment> m_vadThresholdAttachment;

    juce::Label m_vadGracePeriodLabel;
    juce::Slider m_vadGracePeriodSlider;
    std::unique_ptr<SliderAttachment> m_vadGracePeriodAttachment;

    juce::Label m_vadRetroactiveGracePeriodLabel;
    juce::Slider m_vadRetroactiveGracePeriodSlider;
    std::unique_ptr<SliderAttachment> m_vadRetroactiveGracePeriodAttachment;

    juce::Label m_statsHeaderLabel;
    juce::Label m_statsVadGraceBlocksLabel;
    juce::Label m_statsRetroactiveVadGraceBlocksLabel;
    juce::Label m_statsBlocksWaitingForOutputLabel;
    juce::Label m_statsOutputFramesForcedToBeZeroedLabel;

    RnNoiseAudioProcessor &m_processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RnNoiseAudioProcessorEditor)
};