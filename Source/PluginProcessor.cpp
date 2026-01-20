#include "PluginProcessor.h"
#include "PluginEditor.h"

TapeDeathAudioProcessor::TapeDeathAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("TapeDeathParams"), createParameterLayout())
{
    // Cache parameter pointers
    amountParam = parameters.getRawParameterValue("amount");
    damageParam = parameters.getRawParameterValue("damage");
    toneParam = parameters.getRawParameterValue("tone");
    harshnessParam = parameters.getRawParameterValue("harshness");
    saturationParam = parameters.getRawParameterValue("saturation");
    warbleParam = parameters.getRawParameterValue("warble");
    noiseParam = parameters.getRawParameterValue("noise");
    ageParam = parameters.getRawParameterValue("age");
    loCutParam = parameters.getRawParameterValue("loCut");
    hiCutParam = parameters.getRawParameterValue("hiCut");
    dryWetParam = parameters.getRawParameterValue("dryWet");
}

TapeDeathAudioProcessor::~TapeDeathAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeDeathAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // DEGRADATION SECTION (4 knobs)

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amount", "Damage",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "damage", "Rot",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tone", "Darkness",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "harshness", "Violence",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    // CHARACTER SECTION (4 knobs)

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "saturation", "Saturation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "warble", "Warble",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 15.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "noise", "Noise",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "age", "Age",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    // OUTPUT SECTION (2 knobs)

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "loCut", "Lo-Cut",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 20.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            if (value >= 1000.0f)
                return juce::String(value / 1000.0f, 1) + " kHz";
            return juce::String(static_cast<int>(value)) + " Hz";
        }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "hiCut", "Hi-Cut",
        juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.3f), 20000.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            if (value >= 1000.0f)
                return juce::String(value / 1000.0f, 1) + " kHz";
            return juce::String(static_cast<int>(value)) + " Hz";
        }));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWet", "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }));

    return {params.begin(), params.end()};
}

void TapeDeathAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engineLeft.prepare(sampleRate, samplesPerBlock);
    engineRight.prepare(sampleRate, samplesPerBlock);
    updateEngineParameters();
}

void TapeDeathAudioProcessor::releaseResources()
{
    engineLeft.reset();
    engineRight.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TapeDeathAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void TapeDeathAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    updateEngineParameters();

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float dryWet = dryWetParam->load() / 100.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (numChannels >= 1)
        {
            float* leftData = buffer.getWritePointer(0);
            float input = leftData[sample];
            float wet = engineLeft.processSample(input);
            leftData[sample] = input * (1.0f - dryWet) + wet * dryWet;
        }

        if (numChannels >= 2)
        {
            float* rightData = buffer.getWritePointer(1);
            float input = rightData[sample];
            float wet = engineRight.processSample(input);
            rightData[sample] = input * (1.0f - dryWet) + wet * dryWet;
        }
    }
}

void TapeDeathAudioProcessor::updateEngineParameters()
{
    // Degradation controls
    engineLeft.setAmount(amountParam->load() / 100.0f);
    engineRight.setAmount(amountParam->load() / 100.0f);

    engineLeft.setDamage(damageParam->load() / 100.0f);
    engineRight.setDamage(damageParam->load() / 100.0f);

    engineLeft.setTone(toneParam->load() / 100.0f);
    engineRight.setTone(toneParam->load() / 100.0f);

    engineLeft.setHarshness(harshnessParam->load() / 100.0f);
    engineRight.setHarshness(harshnessParam->load() / 100.0f);

    // Character controls
    engineLeft.setSaturation(saturationParam->load() / 100.0f);
    engineRight.setSaturation(saturationParam->load() / 100.0f);

    engineLeft.setWarble(warbleParam->load() / 100.0f);
    engineRight.setWarble(warbleParam->load() / 100.0f);

    engineLeft.setNoise(noiseParam->load() / 100.0f);
    engineRight.setNoise(noiseParam->load() / 100.0f);

    engineLeft.setAge(ageParam->load() / 100.0f);
    engineRight.setAge(ageParam->load() / 100.0f);

    // Output
    engineLeft.setLoCut(loCutParam->load());
    engineRight.setLoCut(loCutParam->load());

    engineLeft.setHiCut(hiCutParam->load());
    engineRight.setHiCut(hiCutParam->load());
}

juce::AudioProcessorEditor* TapeDeathAudioProcessor::createEditor()
{
    return new TapeDeathAudioProcessorEditor(*this);
}

bool TapeDeathAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String TapeDeathAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TapeDeathAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TapeDeathAudioProcessor::producesMidi() const
{
    return false;
}

bool TapeDeathAudioProcessor::isMidiEffect() const
{
    return false;
}

double TapeDeathAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TapeDeathAudioProcessor::getNumPrograms()
{
    return 1;
}

int TapeDeathAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TapeDeathAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String TapeDeathAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void TapeDeathAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void TapeDeathAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TapeDeathAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeDeathAudioProcessor();
}
