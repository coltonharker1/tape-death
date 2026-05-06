#pragma once

#include <JuceHeader.h>
#include "TapeDeathEngine.h"

/**
 * Tape Death Audio Processor
 *
 * Standalone tape death effect with:
 * - Aggressive mode (harsh dropouts, direct amplitude reduction)
 * - Smooth mode (envelope-following fades, progressive LPF)
 * - Blend mode (mix between aggressive and smooth)
 * - Full tape character controls (saturation, hiss, warble)
 * - Hi-cut and lo-cut filters
 */
class TapeDeathAudioProcessor : public juce::AudioProcessor
{
public:
    TapeDeathAudioProcessor();
    ~TapeDeathAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    float getAudioActivity() const { return audioActivity.load(); }

private:
    // DSP engines (one per channel for stereo independence)
    TapeDeathEngine engineLeft;
    TapeDeathEngine engineRight;

    // Smoother for the dry/wet mix (per-sample read kills automation zipper)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dryWetSmoother;

    // Parameter tree
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> audioActivity { 0.0f };

    // Create parameter layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Update engines from parameters
    void updateEngineParameters();

    // Parameter pointers for efficient access
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* damageParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* harshnessParam = nullptr;
    std::atomic<float>* saturationParam = nullptr;
    std::atomic<float>* warbleParam = nullptr;
    std::atomic<float>* noiseParam = nullptr;
    std::atomic<float>* ageParam = nullptr;
    std::atomic<float>* loCutParam = nullptr;
    std::atomic<float>* hiCutParam = nullptr;
    std::atomic<float>* dryWetParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeDeathAudioProcessor)
};
