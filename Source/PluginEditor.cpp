#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

TapeDeathAudioProcessorEditor::TapeDeathAudioProcessorEditor(TapeDeathAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Load custom font for labels
    labelTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::XXII_UltimateBlackMetal_ttf,
        BinaryData::XXII_UltimateBlackMetal_ttfSize);

    // Load logo image
    logoImage = juce::ImageCache::getFromMemory(
        BinaryData::tape_death_2_png,
        BinaryData::tape_death_2_pngSize);

    // Section labels (hidden - drawn in paint())
    titleLabel.setVisible(false);
    primaryLabel.setVisible(false);
    characterLabel.setVisible(false);
    filtersLabel.setVisible(false);

    // Setup all knobs
    auto& params = audioProcessor.getParameters();

    // Degradation controls (4 knobs)
    setupKnob(amountKnob, "amount", "Damage", params);
    setupKnob(damageKnob, "damage", "Rot", params);
    setupKnob(toneKnob, "tone", "Darkness", params);
    setupKnob(harshnessKnob, "harshness", "Violence", params);

    // Character controls (4 knobs)
    setupKnob(saturationKnob, "saturation", "Saturation", params);
    setupKnob(warbleKnob, "warble", "Warble", params);
    setupKnob(noiseKnob, "noise", "Noise", params);
    setupKnob(ageKnob, "age", "Age", params);

    // Output controls (3 knobs)
    setupKnob(loCutKnob, "loCut", "Lo-Cut", params);
    setupKnob(hiCutKnob, "hiCut", "Hi-Cut", params);
    setupKnob(dryWetKnob, "dryWet", "Dry/Wet", params);

    setSize(520, 240);
}

TapeDeathAudioProcessorEditor::~TapeDeathAudioProcessorEditor()
{
}

void TapeDeathAudioProcessorEditor::setupKnob(KnobWithLabel& knob, const juce::String& paramId,
                                                 const juce::String& labelText,
                                                 juce::AudioProcessorValueTreeState& params)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff666666));
    knob.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333333));
    knob.slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText.toUpperCase(), juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centredTop);
    knob.label.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    knob.label.setFont(juce::FontOptions(10.0f));
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        params, paramId, knob.slider);
}

void TapeDeathAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0xff121212));

    // Draw logo image in top right corner
    if (logoImage.isValid())
    {
        int imgWidth = 180;
        int imgHeight = static_cast<int>(imgWidth * (float)logoImage.getHeight() / logoImage.getWidth());
        int imgX = getWidth() - imgWidth;
        int imgY = 15;
        g.drawImage(logoImage, imgX, imgY, imgWidth, imgHeight,
                    0, 0, logoImage.getWidth(), logoImage.getHeight());
    }

    // Use custom font for section labels
    g.setFont(juce::FontOptions(labelTypeface).withHeight(48.0f));
    int lineY;

    // DEGRADATION - left aligned over the knobs
    lineY = 22;
    {
        juce::String label = "Degradation";
        int labelWidth = g.getCurrentFont().getStringWidth(label) + 20;
        int labelCenter = 155;  // Center of the 4 degradation knobs (same as character)
        int labelX = labelCenter - labelWidth / 2;
        int lineEndX = 310;  // Where the image area begins

        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawLine(15, lineY, labelX - 8, lineY, 1.0f);
        g.drawLine(labelX + labelWidth + 8, lineY, lineEndX, lineY, 1.0f);

        g.setColour(juce::Colour(0xffcc5500));  // Orange
        g.drawText(label, labelX, lineY - 20, labelWidth, 40, juce::Justification::centred);
    }

    // Row 2 divider - CHARACTER on left, OUTPUT on right
    lineY = 135;
    int charOutputDivide = 310;  // Where character ends and output begins

    // CHARACTER - left section (4 knobs: 15 + 70*3 + 60 = ~285, center around 155)
    {
        juce::String label = "Character";
        int labelWidth = g.getCurrentFont().getStringWidth(label) + 20;
        int charCenter = 155;  // Center of the 4 character knobs
        int labelX = charCenter - labelWidth / 2;

        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawLine(15, lineY, labelX - 8, lineY, 1.0f);
        g.drawLine(labelX + labelWidth + 8, lineY, charOutputDivide - 15, lineY, 1.0f);

        g.setColour(juce::Colour(0xffcc5500));  // Orange (same as Degradation)
        g.drawText(label, labelX, lineY - 20, labelWidth, 40, juce::Justification::centred);
    }

    // OUTPUT - right section (3 knobs at 310, 380, 450, center around 410)
    {
        juce::String label = "Output";
        int labelWidth = g.getCurrentFont().getStringWidth(label) + 20;
        int outputCenter = 410;  // Center of the 3 output knobs
        int labelX = outputCenter - labelWidth / 2;

        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawLine(charOutputDivide + 15, lineY, labelX - 8, lineY, 1.0f);
        g.drawLine(labelX + labelWidth + 8, lineY, getWidth() - 15, lineY, 1.0f);

        g.setColour(juce::Colour(0xffcc5500));  // Orange (same as Degradation)
        g.drawText(label, labelX, lineY - 20, labelWidth, 40, juce::Justification::centred);
    }
}

void TapeDeathAudioProcessorEditor::resized()
{
    const int knobSize = 60;
    const int labelHeight = 24;
    const int knobSpacing = 75;

    auto positionKnob = [&](KnobWithLabel& knob, int x, int y, int size = 60) {
        knob.slider.setBounds(x, y, size, size);
        knob.label.setBounds(x - 15, y + size, size + 30, labelHeight);
    };

    // Row 1: Degradation (4 knobs left-aligned with character section)
    int row1Y = 35;
    int charSpacing = 70;
    int charStartX = 15;

    positionKnob(amountKnob, charStartX, row1Y);
    positionKnob(damageKnob, charStartX + charSpacing, row1Y);
    positionKnob(toneKnob, charStartX + charSpacing * 2, row1Y);
    positionKnob(harshnessKnob, charStartX + charSpacing * 3, row1Y);

    // Row 2: Character (4 knobs) + Output (2 knobs)
    int row2Y = 148;

    // Character section - 4 knobs on left (same alignment as degradation)
    positionKnob(saturationKnob, charStartX, row2Y);
    positionKnob(warbleKnob, charStartX + charSpacing, row2Y);
    positionKnob(noiseKnob, charStartX + charSpacing * 2, row2Y);
    positionKnob(ageKnob, charStartX + charSpacing * 3, row2Y);

    // Output section - 3 knobs on right (with gap from character)
    int outputSpacing = 70;
    int outputStartX = 310;
    positionKnob(loCutKnob, outputStartX, row2Y);
    positionKnob(hiCutKnob, outputStartX + outputSpacing, row2Y);
    positionKnob(dryWetKnob, outputStartX + outputSpacing * 2, row2Y);
}
