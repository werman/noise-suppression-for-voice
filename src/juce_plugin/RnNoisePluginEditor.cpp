#include "RnNoiseAudioProcessor.h"
#include "RnNoisePluginEditor.h"
#include "common/RnNoiseCommonPlugin.h"

#include <memory>

//==============================================================================
RnNoiseAudioProcessorEditor::RnNoiseAudioProcessorEditor(RnNoiseAudioProcessor &p,
                                                         juce::AudioProcessorValueTreeState &vts)
        : AudioProcessorEditor(&p), m_valueTreeState(vts), m_processorRef(p) {
    juce::ignoreUnused(m_processorRef);

    addAndMakeVisible(m_headerLabel);
    m_headerLabel.setText("Noise Suppressor for Voice", juce::dontSendNotification);
    m_headerLabel.setFont(juce::Font(26.0f, juce::Font::bold | juce::Font::underlined));
    m_headerLabel.setJustificationType(juce::Justification::centred);

    auto vadThresholdParam = m_processorRef.m_parameters.getParameter("vad_threshold");
    auto vadGracePeriodParam = m_processorRef.m_parameters.getParameter("vad_grace_period");
    auto vadRetroactiveGracePeriodParam = m_processorRef.m_parameters.getParameter("vad_retroactive_grace_period");

    m_vadThresholdLabel.setText(vadThresholdParam->getName(99), juce::dontSendNotification);
    addAndMakeVisible(m_vadThresholdLabel);
    addAndMakeVisible(m_vadThresholdSlider);
    m_vadThresholdAttachment = std::make_unique<SliderAttachment>(m_valueTreeState,
                                                                  vadThresholdParam->getParameterID(),
                                                                  m_vadThresholdSlider);

    m_vadGracePeriodLabel.setText(vadGracePeriodParam->getName(99), juce::dontSendNotification);
    addAndMakeVisible(m_vadGracePeriodLabel);
    addAndMakeVisible(m_vadGracePeriodSlider);
    m_vadGracePeriodAttachment = std::make_unique<SliderAttachment>(m_valueTreeState,
                                                                    vadGracePeriodParam->getParameterID(),
                                                                    m_vadGracePeriodSlider);

    m_vadRetroactiveGracePeriodLabel.setText(vadRetroactiveGracePeriodParam->getName(99),
                                             juce::dontSendNotification);
    addAndMakeVisible(m_vadRetroactiveGracePeriodLabel);
    addAndMakeVisible(m_vadRetroactiveGracePeriodSlider);
    m_vadRetroactiveGracePeriodAttachment = std::make_unique<SliderAttachment>(m_valueTreeState,
                                                                               vadRetroactiveGracePeriodParam->getParameterID(),
                                                                               m_vadRetroactiveGracePeriodSlider);

    addAndMakeVisible(m_statsHeaderLabel);
    m_statsHeaderLabel.setText("Debug Statistics (updated once per second)", juce::dontSendNotification);
    m_statsHeaderLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    m_statsHeaderLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(m_statsVadGraceBlocksLabel);
    addAndMakeVisible(m_statsRetroactiveVadGraceBlocksLabel);
    addAndMakeVisible(m_statsBlocksWaitingForOutputLabel);
    addAndMakeVisible(m_statsOutputFramesForcedToBeZeroedLabel);

    setSize(400, 400);
}

void RnNoiseAudioProcessorEditor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colours::black);
}

void RnNoiseAudioProcessorEditor::resized() {
    juce::FlexBox flexBox;
    flexBox.flexWrap = juce::FlexBox::Wrap::wrap;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    flexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
    flexBox.flexDirection = juce::FlexBox::Direction::column;

    float width = static_cast<float>(getLocalBounds().getWidth());

    flexBox.items.add(juce::FlexItem(m_headerLabel).withWidth(width).withFlex(1.0));

    flexBox.items.add(juce::FlexItem(m_vadThresholdLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(juce::FlexItem(m_vadThresholdSlider).withWidth(width).withFlex(1.0));
    flexBox.items.add(juce::FlexItem(m_vadGracePeriodLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(juce::FlexItem(m_vadGracePeriodSlider).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_vadRetroactiveGracePeriodLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_vadRetroactiveGracePeriodSlider).withWidth(width).withFlex(1.0));

    flexBox.items.add(
            juce::FlexItem(m_statsHeaderLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_statsVadGraceBlocksLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_statsRetroactiveVadGraceBlocksLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_statsBlocksWaitingForOutputLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_statsOutputFramesForcedToBeZeroedLabel).withWidth(width).withFlex(1.0));

    flexBox.performLayout(getLocalBounds().toFloat());
}

void RnNoiseAudioProcessorEditor::visibilityChanged() {
    if (isVisible()) {
        timerCallback();
        startTimer(1000);
    } else {
        stopTimer();
    }
}

void RnNoiseAudioProcessorEditor::timerCallback() {
    auto plugin = m_processorRef.m_rnNoisePlugin;

    if (!plugin)
        return;

    RnNoiseStats stats = plugin->getStats();
    m_processorRef.m_rnNoisePlugin->resetStats();

    juce::String statsVadGraceBlocksStr = juce::String("Unmuted via VAD: ");
    statsVadGraceBlocksStr << (int) stats.vadGraceBlocks * 10 << " ms";
    m_statsVadGraceBlocksLabel.setText(statsVadGraceBlocksStr, juce::dontSendNotification);

    juce::String statsRetroactiveVadGraceBlocksStr = juce::String("Unmuted via Retroactive VAD: ");
    statsRetroactiveVadGraceBlocksStr << (int) stats.retroactiveVADGraceBlocks * 10 << " ms";
    m_statsRetroactiveVadGraceBlocksLabel.setText(statsRetroactiveVadGraceBlocksStr, juce::dontSendNotification);

    juce::String statsBlocksWaitingForOutputStr = juce::String("Output queue: ");
    statsBlocksWaitingForOutputStr << (int) stats.blocksWaitingForOutput * 10 << " ms";
    m_statsBlocksWaitingForOutputLabel.setText(statsBlocksWaitingForOutputStr, juce::dontSendNotification);

    juce::String statsOutputFramesForcedToBeZeroedStr = juce::String("Output stomped: ");
    statsOutputFramesForcedToBeZeroedStr << (long long int) stats.outputFramesForcedToBeZeroed * 10 << " ms";
    m_statsOutputFramesForcedToBeZeroedLabel.setText(statsOutputFramesForcedToBeZeroedStr, juce::dontSendNotification);
}

RnNoiseAudioProcessorEditor::~RnNoiseAudioProcessorEditor() = default;
