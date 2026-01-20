#pragma once

#include <JuceHeader.h>
#include <random>
#include <array>

/**
 * Tape Death Engine
 *
 * Smooth tape degradation effect with:
 * - Envelope-based fading with asymmetric attack/release
 * - Adjustable harshness for dropout character
 * - Full tape character controls (saturation, warble, noise, age)
 */
class TapeDeathEngine
{
public:
    TapeDeathEngine()
    {
        random.seed(std::random_device{}());
    }

    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sampleRate = newSampleRate;

        // Initialize filters
        toneFilter.reset();
        loCutFilter.reset();
        hiCutFilter.reset();
        hiCutFilter2.reset();
        noiseFilter.reset();

        updateToneFilter();
        updateLoCutFilter();
        updateHiCutFilter();

        // Noise filter - lowpass at 3kHz to warm up the hiss
        auto noiseCoeffs = juce::IIRCoefficients::makeLowPass(sampleRate, 3000.0f);
        noiseFilter.setCoefficients(noiseCoeffs);

        // Initialize warble delay buffer (enough for max delay + some margin)
        warbleBufferSize = static_cast<size_t>((maxWarbleDelayMs / 1000.0f) * sampleRate * 2) + 1;
        warbleBuffer.resize(warbleBufferSize);
        std::fill(warbleBuffer.begin(), warbleBuffer.end(), 0.0f);
        warbleWritePos = 0;

        // Reset state
        dropoutEnvelope = 1.0f;
        dropoutTarget = 1.0f;
        warbleLFOPhase = 0.0f;
        warbleLFOPhase2 = 0.0f;
        stuckCounter = 0;

        // Reset age state
        ageFilterPhase1 = 0.0f;
        ageFilterPhase2 = 0.0f;
        ageFilterPhase3 = 0.0f;
        agePitchPhase = 0.0f;
        ageFilterMod = 0.0f;
        agePitchMod = 0.0f;

        // Initialize pink noise state
        pinkNoiseState = {};
    }

    void reset()
    {
        toneFilter.reset();
        loCutFilter.reset();
        hiCutFilter.reset();
        hiCutFilter2.reset();
        noiseFilter.reset();
        dropoutEnvelope = 1.0f;
        dropoutTarget = 1.0f;
        warbleLFOPhase = 0.0f;
        warbleLFOPhase2 = 0.0f;
        stuckCounter = 0;
        ageFilterPhase1 = 0.0f;
        ageFilterPhase2 = 0.0f;
        ageFilterPhase3 = 0.0f;
        agePitchPhase = 0.0f;
        ageFilterMod = 0.0f;
        agePitchMod = 0.0f;
        pinkNoiseState = {};
        std::fill(warbleBuffer.begin(), warbleBuffer.end(), 0.0f);
        warbleWritePos = 0;
    }

    float processSample(float input)
    {
        float sample = input;

        // Update age modulations (filter wandering + pitch drift)
        updateAgeModulations();

        // Apply tone filter FIRST - always active, affected by age
        sample = applyTone(sample);

        // Apply warble (pitch modulation) - age adds drift here too
        sample = applyWarble(sample);

        // Apply dropouts (amount/damage controlled)
        sample = applyDropouts(sample);

        // Apply saturation
        sample = applySaturation(sample);

        // Apply noise (hiss + crackle combined)
        sample = applyNoise(sample);

        // Apply output filters (lo-cut, hi-cut)
        sample = applyLoCut(sample);
        sample = applyHiCut(sample);

        return sample;
    }

    // Setters
    void setAmount(float newAmount) { amount = juce::jlimit(0.0f, 1.0f, newAmount); }

    void setDamage(float damage)
    {
        damage = juce::jlimit(0.0f, 1.0f, damage);
        dropoutDensity = damage * damage * 0.8f + damage * 0.2f;
        dropoutDepthBase = 0.2f + damage * 0.8f;
    }

    void setHarshness(float harsh)
    {
        harsh = juce::jlimit(0.0f, 1.0f, harsh);
        harshness = harsh;  // Store raw value for dropout frequency boost
        dropoutSmoothness = 1.0f - (harsh * 0.85f);
    }

    /**
     * Tone control (0-1)
     * 0% = bright (20kHz), 100% = dark/warm (1.5kHz)
     */
    void setTone(float t)
    {
        tone = juce::jlimit(0.0f, 1.0f, t);
        updateToneFilter();
    }

    void setSaturation(float sat) { saturationAmount = juce::jlimit(0.0f, 1.0f, sat); }
    void setWarble(float w) { warble = juce::jlimit(0.0f, 1.0f, w); }
    void setNoise(float n) { noise = juce::jlimit(0.0f, 1.0f, n); }

    /**
     * Age - simulates decades of tape deterioration
     * Adds filter wandering (unstable EQ) and gentle pitch drift
     */
    void setAge(float a) { age = juce::jlimit(0.0f, 1.0f, a); }

    void setLoCut(float freq)
    {
        loCutFreq = juce::jlimit(20.0f, 2000.0f, freq);
        updateLoCutFilter();
    }

    void setHiCut(float freq)
    {
        hiCutFreq = juce::jlimit(500.0f, 20000.0f, freq);
        updateHiCutFilter();
    }

private:
    //==========================================================================
    // TONE - Dedicated lowpass filter, always active
    //==========================================================================
    float applyTone(float sample)
    {
        // Calculate target cutoff: 20kHz at 0%, 500Hz at 100% (VERY dramatic)
        // Age modulates the cutoff for "wandering" EQ effect
        float baseCutoff = 20000.0f - (tone * 19500.0f);  // 20kHz -> 500Hz

        // Age adds wandering: up to ±2000Hz variation at full age
        float modulatedCutoff = baseCutoff + (ageFilterMod * age * 2000.0f);
        modulatedCutoff = juce::jlimit(400.0f, 20000.0f, modulatedCutoff);

        // ALWAYS update filter (remove the threshold check for debugging)
        lastToneCutoff = modulatedCutoff;
        auto coeffs = juce::IIRCoefficients::makeLowPass(sampleRate, modulatedCutoff);
        toneFilter.setCoefficients(coeffs);

        return toneFilter.processSingleSampleRaw(sample);
    }

    //==========================================================================
    // AGE - Filter wandering and pitch drift modulations
    //==========================================================================
    void updateAgeModulations()
    {
        if (age <= 0.0f)
        {
            ageFilterMod = 0.0f;
            agePitchMod = 0.0f;
            return;
        }

        // Multiple slow LFOs for organic, gradual movement
        // Filter wandering: ~0.15Hz, 0.1Hz, 0.2Hz (subtle but noticeable)
        ageFilterPhase1 += juce::MathConstants<float>::twoPi * 0.15f / static_cast<float>(sampleRate);
        ageFilterPhase2 += juce::MathConstants<float>::twoPi * 0.1f / static_cast<float>(sampleRate);
        ageFilterPhase3 += juce::MathConstants<float>::twoPi * 0.2f / static_cast<float>(sampleRate);

        if (ageFilterPhase1 > juce::MathConstants<float>::twoPi) ageFilterPhase1 -= juce::MathConstants<float>::twoPi;
        if (ageFilterPhase2 > juce::MathConstants<float>::twoPi) ageFilterPhase2 -= juce::MathConstants<float>::twoPi;
        if (ageFilterPhase3 > juce::MathConstants<float>::twoPi) ageFilterPhase3 -= juce::MathConstants<float>::twoPi;

        // Combine for complex, wandering modulation (-1 to +1 range)
        float mod1 = std::sin(ageFilterPhase1);
        float mod2 = std::sin(ageFilterPhase2) * 0.7f;
        float mod3 = std::sin(ageFilterPhase3) * 0.5f;
        ageFilterMod = (mod1 + mod2 + mod3) / 2.2f;

        // Pitch drift: ~0.2Hz for audible pitch wandering
        agePitchPhase += juce::MathConstants<float>::twoPi * 0.2f / static_cast<float>(sampleRate);
        if (agePitchPhase > juce::MathConstants<float>::twoPi) agePitchPhase -= juce::MathConstants<float>::twoPi;

        // Combine multiple rates for pitch too
        float pitchMod1 = std::sin(agePitchPhase);
        float pitchMod2 = std::sin(agePitchPhase * 1.3f + 1.0f) * 0.6f;
        agePitchMod = (pitchMod1 + pitchMod2) / 1.6f;
    }

    //==========================================================================
    // WARBLE - Pitch modulation with age drift
    //==========================================================================
    float applyWarble(float sample)
    {
        // Always write to the delay buffer
        warbleBuffer[warbleWritePos] = sample;
        warbleWritePos = (warbleWritePos + 1) % warbleBufferSize;

        // If no warble AND no age, return dry
        if (warble <= 0.0f && age <= 0.0f) return sample;

        // Warble: periodic wow/flutter
        float warbleModulation = 0.0f;
        if (warble > 0.0f)
        {
            constexpr float warbleRate = 1.0f;  // 1Hz wow
            float slowWow = std::sin(warbleLFOPhase);
            float fastFlutter = std::sin(warbleLFOPhase2) * 0.25f;
            warbleModulation = (slowWow + fastFlutter) * warble;

            warbleLFOPhase += juce::MathConstants<float>::twoPi * warbleRate / static_cast<float>(sampleRate);
            warbleLFOPhase2 += juce::MathConstants<float>::twoPi * 6.0f / static_cast<float>(sampleRate);
            if (warbleLFOPhase > juce::MathConstants<float>::twoPi) warbleLFOPhase -= juce::MathConstants<float>::twoPi;
            if (warbleLFOPhase2 > juce::MathConstants<float>::twoPi) warbleLFOPhase2 -= juce::MathConstants<float>::twoPi;
        }

        // Age: random pitch drift (more dramatic now)
        float ageDriftModulation = agePitchMod * age * 0.8f;  // More obvious drift

        // Combined modulation
        float totalModulation = warbleModulation + ageDriftModulation;

        // Map to delay time: warble up to 15ms, age drift up to 12ms (more dramatic)
        float maxWarbleMs = warble * 15.0f;
        float maxAgeMs = age * 12.0f;
        float baseDelayMs = (maxWarbleMs + maxAgeMs) * 0.5f;  // Center point
        float delayMs = baseDelayMs + totalModulation * (maxWarbleMs * 0.5f + maxAgeMs * 0.5f);
        delayMs = std::max(0.1f, delayMs);

        float delaySamples = (delayMs / 1000.0f) * static_cast<float>(sampleRate);
        delaySamples = std::max(1.0f, delaySamples);

        float readPosFloat = static_cast<float>(warbleWritePos) - delaySamples - 1.0f;
        if (readPosFloat < 0.0f)
            readPosFloat += static_cast<float>(warbleBufferSize);

        size_t readPos0 = static_cast<size_t>(readPosFloat) % warbleBufferSize;
        size_t readPos1 = (readPos0 + 1) % warbleBufferSize;
        float frac = readPosFloat - std::floor(readPosFloat);

        return warbleBuffer[readPos0] * (1.0f - frac) + warbleBuffer[readPos1] * frac;
    }

    //==========================================================================
    // DROPOUTS - Amount/Damage controlled signal loss
    //==========================================================================
    float applyDropouts(float sample)
    {
        if (amount <= 0.0f) return sample;

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // Asymmetric envelope: fast attack, slow release
        // Violence (harshness) makes both faster for more aggressive character
        float envelopeDiff = dropoutTarget - dropoutEnvelope;
        float baseAttackRate = 0.005f + (1.0f - dropoutSmoothness) * 0.15f;
        float attackRate = baseAttackRate * (1.0f + harshness * 3.0f);  // Up to 4x faster attacks

        float baseReleaseRate = 0.0002f + dropoutSmoothness * 0.002f;
        float releaseRate = baseReleaseRate * (1.0f + harshness * 5.0f);  // Up to 6x faster recovery

        if (envelopeDiff < 0)
            dropoutEnvelope += envelopeDiff * attackRate;
        else
            dropoutEnvelope += envelopeDiff * releaseRate;

        sample *= dropoutEnvelope;

        // Stuck mechanic
        if (stuckCounter > 0)
        {
            stuckCounter--;
        }
        else
        {
            // Violence increases dropout frequency
            float violenceBoost = 1.0f + harshness * harshness * 4.0f;  // Up to 5x more frequent at max
            float triggerProb = dropoutDensity * amount * 0.002f * violenceBoost;

            if (dist(random) < triggerProb)
            {
                float depthVariation = dist(random) * 0.4f;
                float thisDepth = std::min(1.0f, dropoutDepthBase + depthVariation);
                dropoutTarget = 1.0f - thisDepth;

                if (dropoutDensity > 0.5f && dist(random) < dropoutDensity * 0.3f)
                {
                    // Violence reduces stuck duration (shorter but more punchy)
                    float stuckDurationScale = 1.0f - harshness * 0.6f;  // Down to 40% duration
                    int stuckMs = static_cast<int>((50 + dist(random) * 300) * stuckDurationScale);
                    stuckCounter = static_cast<int>(stuckMs * sampleRate / 1000.0f);
                }
            }
            else if (dropoutEnvelope < 0.95f)
            {
                // Violence also increases recovery probability for rapid-fire dropouts
                float recoveryBoost = 1.0f + harshness * 2.0f;
                float recoveryProb = ((1.0f - dropoutDensity) * 0.001f + 0.0001f) * recoveryBoost;
                if (dist(random) < recoveryProb)
                {
                    float maxRecovery = 1.0f - (dropoutDensity * amount * 0.2f);
                    dropoutTarget = maxRecovery;
                }
            }
        }

        return sample;
    }

    //==========================================================================
    // SATURATION - Tape saturation/warmth
    //==========================================================================
    float transparentClipping(float x, float gain) noexcept
    {
        float scaled = x * (1.0f + gain * 6.0f);
        float clipped = std::tanh(scaled);
        float originalOutput = clipped * (0.5f + 0.5f / (1.0f + gain));
        float scaledGain = gain * (0.5f / 2.6f);
        float warmthAdjustment = 1.0f + 0.1f * scaledGain;
        return originalOutput * warmthAdjustment;
    }

    float warmTubeStage(float x, float warmth) noexcept
    {
        float scaledWarmth = warmth * 0.5f;
        float asymmetry = x + scaledWarmth * 20.0f * x * x;
        float warmed = std::tanh(asymmetry);
        return warmed * (1.0f - 0.1f * scaledWarmth);
    }

    float applySaturation(float sample)
    {
        if (saturationAmount <= 0.0f) return sample;

        float gain = saturationAmount * 2.0f;
        float warmth = saturationAmount * 0.5f;

        sample = warmTubeStage(sample, warmth);
        sample = transparentClipping(sample, gain);

        return sample;
    }

    //==========================================================================
    // NOISE - Hiss + Crackle
    //==========================================================================
    float applyNoise(float sample)
    {
        if (noise <= 0.0f) return sample;

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // Hiss (continuous pink noise) - exponential curve so low values are subtle
        // Filter through lowpass to make it warmer/tape-like
        float pink = generatePinkNoise();
        float filteredPink = noiseFilter.processSingleSampleRaw(pink);
        float hissLevel = noise * noise * 0.035f;  // Exponential + reduced max level
        sample += filteredPink * hissLevel;

        // Crackle (impulsive) - scales with hiss level for consistency
        float crackleAmount = noise * noise * noise;  // Cubic for even more scaling at low values

        float popProb = crackleAmount * 0.0003f;
        if (dist(random) < popProb)
        {
            float popIntensity = 0.15f + dist(random) * 0.25f;
            float pop = (dist(random) > 0.5f ? 1.0f : -1.0f) * popIntensity * noise * noise;
            sample += pop;
        }

        float softCrackleProb = crackleAmount * 0.001f;
        if (dist(random) < softCrackleProb)
        {
            float softCrackle = (dist(random) * 2.0f - 1.0f) * 0.06f * noise * noise;
            sample += softCrackle;
        }

        return sample;
    }

    //==========================================================================
    // OUTPUT FILTERS - Lo-Cut (high-pass) and Hi-Cut (low-pass)
    //==========================================================================
    float applyLoCut(float sample)
    {
        if (loCutFreq > 25.0f)
            sample = loCutFilter.processSingleSampleRaw(sample);
        return sample;
    }

    float applyHiCut(float sample)
    {
        // Cascade two filters for 24dB/octave
        sample = hiCutFilter.processSingleSampleRaw(sample);
        sample = hiCutFilter2.processSingleSampleRaw(sample);
        return sample;
    }

    //==========================================================================
    // UTILITIES
    //==========================================================================
    float generatePinkNoise()
    {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        pinkNoiseState.counter++;
        float white = dist(random);

        if ((pinkNoiseState.counter & 0x01) == 0) pinkNoiseState.octaves[0] = dist(random);
        if ((pinkNoiseState.counter & 0x03) == 0) pinkNoiseState.octaves[1] = dist(random);
        if ((pinkNoiseState.counter & 0x07) == 0) pinkNoiseState.octaves[2] = dist(random);
        if ((pinkNoiseState.counter & 0x0F) == 0) pinkNoiseState.octaves[3] = dist(random);
        if ((pinkNoiseState.counter & 0x1F) == 0) pinkNoiseState.octaves[4] = dist(random);

        float pink = white * 0.5f;
        pink += pinkNoiseState.octaves[0] * 0.25f;
        pink += pinkNoiseState.octaves[1] * 0.125f;
        pink += pinkNoiseState.octaves[2] * 0.0625f;
        pink += pinkNoiseState.octaves[3] * 0.03125f;
        pink += pinkNoiseState.octaves[4] * 0.015625f;

        return pink;
    }

    void updateToneFilter()
    {
        if (sampleRate > 0)
        {
            float cutoff = 20000.0f - (tone * 18500.0f);
            cutoff = juce::jlimit(1500.0f, 20000.0f, cutoff);
            lastToneCutoff = cutoff;
            auto coeffs = juce::IIRCoefficients::makeLowPass(sampleRate, cutoff);
            toneFilter.setCoefficients(coeffs);
        }
    }

    void updateLoCutFilter()
    {
        if (sampleRate > 0)
        {
            // Use 2nd-order filter (12dB/octave) with Butterworth Q for smooth rolloff
            auto coeffs = juce::IIRCoefficients::makeHighPass(sampleRate, loCutFreq, 0.707f);
            loCutFilter.setCoefficients(coeffs);
        }
    }

    void updateHiCutFilter()
    {
        if (sampleRate > 0)
        {
            // Two cascaded 2nd-order filters for 24dB/octave rolloff
            auto coeffs = juce::IIRCoefficients::makeLowPass(sampleRate, hiCutFreq, 0.707f);
            hiCutFilter.setCoefficients(coeffs);
            hiCutFilter2.setCoefficients(coeffs);
        }
    }

    //==========================================================================
    // STATE
    //==========================================================================

    double sampleRate = 44100.0;

    // Parameters
    float amount = 0.0f;
    float tone = 0.0f;
    float dropoutDensity = 0.3f;
    float dropoutDepthBase = 0.5f;
    float dropoutSmoothness = 0.5f;
    float harshness = 0.0f;  // Raw harshness for Violence effects
    float saturationAmount = 0.0f;
    float warble = 0.0f;
    float noise = 0.0f;
    float age = 0.0f;
    float loCutFreq = 20.0f;
    float hiCutFreq = 20000.0f;

    // Tone filter state
    float lastToneCutoff = 20000.0f;
    juce::IIRFilter toneFilter;

    // Lo-cut filter
    juce::IIRFilter loCutFilter;

    // Hi-cut filter (two stages for 24dB/octave)
    juce::IIRFilter hiCutFilter;
    juce::IIRFilter hiCutFilter2;

    // Noise filter (to warm up the hiss)
    juce::IIRFilter noiseFilter;

    // Dropout envelope state
    float dropoutEnvelope = 1.0f;
    float dropoutTarget = 1.0f;
    int stuckCounter = 0;

    // Age modulation state (filter wandering + pitch drift)
    float ageFilterPhase1 = 0.0f;
    float ageFilterPhase2 = 0.0f;
    float ageFilterPhase3 = 0.0f;
    float agePitchPhase = 0.0f;
    float ageFilterMod = 0.0f;   // Current filter modulation (-1 to +1)
    float agePitchMod = 0.0f;    // Current pitch modulation (-1 to +1)

    // Warble LFO and delay buffer
    float warbleLFOPhase = 0.0f;
    float warbleLFOPhase2 = 0.0f;
    std::vector<float> warbleBuffer;
    size_t warbleBufferSize = 0;
    size_t warbleWritePos = 0;
    static constexpr float maxWarbleDelayMs = 30.0f;

    // Pink noise state
    struct PinkNoiseState
    {
        uint32_t counter = 0;
        std::array<float, 5> octaves = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    } pinkNoiseState;

    std::mt19937 random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeDeathEngine)
};
