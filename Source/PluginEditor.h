#pragma once

#include <JuceHeader.h>
#include <sonorous_visual/sonorous_visual.h>
#include "PluginProcessor.h"

class TapeDeathAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit TapeDeathAudioProcessorEditor(TapeDeathAudioProcessor&);
    ~TapeDeathAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // The plugin used to declare its own MonoPlateLookAndFeel here. It now uses
    // sonorous::SonorousLookAndFeel from sonorous-kit so visual styling stays
    // consistent across the Project Sonorous instrument line. The member name
    // below (monoLookAndFeel) is preserved for source compatibility.

    struct KnobWithLabel
    {
        juce::Slider slider;
        juce::Label label;
        juce::Label value;
        juce::String paramId;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    TapeDeathAudioProcessor& audioProcessor;
    sonorous::SonorousLookAndFeel monoLookAndFeel;
    juce::Rectangle<int> diagramBounds;
    juce::Typeface::Ptr instrumentSerifRegular;
    juce::Typeface::Ptr instrumentSerifItalic;
    juce::Typeface::Ptr jetBrainsMono;
    float visualActivity = 0.0f;
    float reelPhase = 0.0f;
    float tapeTravel = 0.0f;
    int signalHoldFrames = 0;

    KnobWithLabel amountKnob;
    KnobWithLabel damageKnob;
    KnobWithLabel toneKnob;
    KnobWithLabel harshnessKnob;
    KnobWithLabel saturationKnob;
    KnobWithLabel warbleKnob;
    KnobWithLabel noiseKnob;
    KnobWithLabel ageKnob;
    KnobWithLabel loCutKnob;
    KnobWithLabel hiCutKnob;
    KnobWithLabel dryWetKnob;

    void timerCallback() override;
    void setupKnob(KnobWithLabel& knob, const juce::String& paramId,
                   const juce::String& labelText, juce::AudioProcessorValueTreeState& params);
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDiagram(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSection(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title,
                     const juce::String& subtitle);
    void positionControl(KnobWithLabel& knob, juce::Rectangle<int> bounds);
    void updateReadouts();
    juce::String formatParameterValue(const juce::String& paramId) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeDeathAudioProcessorEditor)
};
