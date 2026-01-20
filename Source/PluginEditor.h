#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
 * Tape Death Plugin Editor
 *
 * Knob-based interface with organized parameter groups
 */
class TapeDeathAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    TapeDeathAudioProcessorEditor(TapeDeathAudioProcessor&);
    ~TapeDeathAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    TapeDeathAudioProcessor& audioProcessor;

    // Custom font for labels
    juce::Typeface::Ptr labelTypeface;

    // Logo image
    juce::Image logoImage;

    // Labels
    juce::Label titleLabel;
    juce::Label primaryLabel;
    juce::Label characterLabel;
    juce::Label filtersLabel;

    // Knobs and their attachments
    struct KnobWithLabel
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Degradation controls (4 knobs)
    KnobWithLabel amountKnob;
    KnobWithLabel damageKnob;
    KnobWithLabel toneKnob;
    KnobWithLabel harshnessKnob;

    // Character controls (4 knobs)
    KnobWithLabel saturationKnob;
    KnobWithLabel warbleKnob;
    KnobWithLabel noiseKnob;
    KnobWithLabel ageKnob;

    // Output controls (3 knobs)
    KnobWithLabel loCutKnob;
    KnobWithLabel hiCutKnob;
    KnobWithLabel dryWetKnob;

    // Helper to setup a knob
    void setupKnob(KnobWithLabel& knob, const juce::String& paramId,
                   const juce::String& labelText, juce::AudioProcessorValueTreeState& params);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeDeathAudioProcessorEditor)
};
