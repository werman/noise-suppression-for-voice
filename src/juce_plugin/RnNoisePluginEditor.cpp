#include "RnNoiseAudioProcessor.h"
#include "RnNoisePluginEditor.h"
#include "common/RnNoiseCommonPlugin.h"

#include <memory>

//==============================================================================
RnNoiseAudioProcessorEditor::RnNoiseAudioProcessorEditor(RnNoiseAudioProcessor &p,
                                                         juce::AudioProcessorValueTreeState &vts)
        : AudioProcessorEditor(&p), m_valueTreeState(vts), m_processorRef(p) {
    juce::ignoreUnused(m_processorRef);

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

    addAndMakeVisible(m_statsVadGraceBlocksLabel);
    addAndMakeVisible(m_statsRetroactiveVadGraceBlocksLabel);
    addAndMakeVisible(m_statsBlocksWaitingForOutputLabel);
    addAndMakeVisible(m_statsOutputFramesForcedToBeZeroedLabel);

    setSize(400, 400);
}

void RnNoiseAudioProcessorEditor::paint(juce::Graphics &g) {
    Component::paint(g);
}

void RnNoiseAudioProcessorEditor::resized() {
    juce::FlexBox flexBox;
    flexBox.flexWrap = juce::FlexBox::Wrap::wrap;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    flexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
    flexBox.flexDirection = juce::FlexBox::Direction::column;

    float width = static_cast<float>(getLocalBounds().getWidth());

    flexBox.items.add(juce::FlexItem(m_vadThresholdLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(juce::FlexItem(m_vadThresholdSlider).withWidth(width).withFlex(1.0));
    flexBox.items.add(juce::FlexItem(m_vadGracePeriodLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(juce::FlexItem(m_vadGracePeriodSlider).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_vadRetroactiveGracePeriodLabel).withWidth(width).withFlex(1.0));
    flexBox.items.add(
            juce::FlexItem(m_vadRetroactiveGracePeriodSlider).withWidth(width).withFlex(1.0));

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

    juce::String statsVadGraceBlocksStr = juce::String("Blocks unmuted via VAD: ");
    statsVadGraceBlocksStr << (int) stats.vadGraceBlocks;
    m_statsVadGraceBlocksLabel.setText(statsVadGraceBlocksStr, juce::dontSendNotification);

    juce::String statsRetroactiveVadGraceBlocksStr = juce::String("Blocks unmuted via Retroactive VAD: ");
    statsRetroactiveVadGraceBlocksStr << (int) stats.retroactiveVADGraceBlocks;
    m_statsRetroactiveVadGraceBlocksLabel.setText(statsRetroactiveVadGraceBlocksStr, juce::dontSendNotification);

    juce::String statsBlocksWaitingForOutputStr = juce::String("Blocks in output queue: ");
    statsBlocksWaitingForOutputStr << (int) stats.blocksWaitingForOutput;
    m_statsBlocksWaitingForOutputLabel.setText(statsBlocksWaitingForOutputStr, juce::dontSendNotification);

    juce::String statsOutputFramesForcedToBeZeroedStr = juce::String("Output blocks stompted: ");
    statsOutputFramesForcedToBeZeroedStr << (long long int) stats.outputFramesForcedToBeZeroed;
    m_statsOutputFramesForcedToBeZeroedLabel.setText(statsOutputFramesForcedToBeZeroedStr, juce::dontSendNotification);
}

RnNoiseAudioProcessorEditor::~RnNoiseAudioProcessorEditor() = default;
