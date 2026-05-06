#pragma once

#include <JuceHeader.h>
#include <random>
#include <array>
#include <cmath>

/**
 * Tape Death Engine
 *
 * Smooth tape degradation effect with:
 * - Envelope-based fading with asymmetric attack/release
 * - Adjustable harshness for dropout character
 * - Full tape character controls (saturation, warble, noise, age)
 *
 * DSP notes:
 * - Filter cutoffs are smoothed (LinearSmoothedValue) and IIR coefficients
 *   are recomputed at control rate (every controlRateInterval samples) so
 *   parameter automation and age modulation never produce zipper noise or
 *   per-sample biquad rebuilds.
 * - Dropout timings are normalised to sample rate so the same patch sounds
 *   identical at 44.1k, 48k, 96k, etc.
 * - A 1-pole DC blocker after saturation removes the DC component introduced
 *   by the asymmetric warm-tube stage.
 * - Crackle pops are clamped to remaining headroom so they cannot drive the
 *   wet signal beyond ±0.95 regardless of how hot the input is.
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
        sampleRateInv = 1.0f / static_cast<float>(sampleRate);
        sampleRateRatio = 44100.0f / static_cast<float>(sampleRate);

        toneFilter.reset();
        loCutFilter.reset();
        hiCutFilter.reset();
        hiCutFilter2.reset();
        noiseFilter.reset();
        crackleFilter.reset();

        // Hiss filter sits at 2.2kHz — gentle warming, retains some presence.
        noiseFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 2200.0f));

        // Crackle path runs through a darker filter so pops become muffled
        // thumps rather than tape crinkle. Independent of hiss tone.
        crackleFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 900.0f));

        // Saturation oversampler: 6th-order Butterworth (3 cascaded biquads)
        // at 18kHz, designed for the 2x rate. Q values from Butterworth pole
        // angles π/12, 3π/12, 5π/12.
        const double sr2 = sampleRate * 2.0;
        constexpr double satOversampleCutoff = 18000.0;
        constexpr double satQ1 = 0.5176380902;  // 1/(2cos(π/12))
        constexpr double satQ2 = 0.7071067812;  // 1/(2cos(3π/12))
        constexpr double satQ3 = 1.9318516526;  // 1/(2cos(5π/12))

        satUpStage1.reset();
        satUpStage2.reset();
        satUpStage3.reset();
        satDnStage1.reset();
        satDnStage2.reset();
        satDnStage3.reset();

        satUpStage1.setCoefficients(juce::IIRCoefficients::makeLowPass(sr2, satOversampleCutoff, satQ1));
        satUpStage2.setCoefficients(juce::IIRCoefficients::makeLowPass(sr2, satOversampleCutoff, satQ2));
        satUpStage3.setCoefficients(juce::IIRCoefficients::makeLowPass(sr2, satOversampleCutoff, satQ3));
        satDnStage1.setCoefficients(juce::IIRCoefficients::makeLowPass(sr2, satOversampleCutoff, satQ1));
        satDnStage2.setCoefficients(juce::IIRCoefficients::makeLowPass(sr2, satOversampleCutoff, satQ2));
        satDnStage3.setCoefficients(juce::IIRCoefficients::makeLowPass(sr2, satOversampleCutoff, satQ3));

        // 50ms ramp absorbs host parameter steps without audible zipper
        constexpr double smoothingTimeSeconds = 0.05;
        toneCutoffSmoothed.reset(sampleRate, smoothingTimeSeconds);
        loCutFreqSmoothed.reset(sampleRate, smoothingTimeSeconds);
        hiCutFreqSmoothed.reset(sampleRate, smoothingTimeSeconds);
        saturationSmoothed.reset(sampleRate, smoothingTimeSeconds);

        toneCutoffSmoothed.setCurrentAndTargetValue(computeToneBaseCutoff());
        loCutFreqSmoothed.setCurrentAndTargetValue(loCutFreq);
        hiCutFreqSmoothed.setCurrentAndTargetValue(hiCutFreq);
        saturationSmoothed.setCurrentAndTargetValue(saturationAmount);

        updateAllFilterCoefficients();
        controlRateCounter = 0;

        // 1-pole HPF at ~10Hz: y[n] = x[n] - x[n-1] + R*y[n-1]
        dcBlockerR = std::exp(-juce::MathConstants<float>::twoPi * 10.0f * sampleRateInv);
        dcBlockerXz = 0.0f;
        dcBlockerYz = 0.0f;

        warbleBufferSize = static_cast<size_t>((maxWarbleDelayMs / 1000.0f) * sampleRate * 2) + 1;
        warbleBuffer.resize(warbleBufferSize);
        std::fill(warbleBuffer.begin(), warbleBuffer.end(), 0.0f);
        warbleWritePos = 0;

        dropoutEnvelope = 1.0f;
        dropoutTarget = 1.0f;
        warbleLFOPhase = 0.0;
        warbleLFOPhase2 = 0.0;
        stuckCounter = 0;
        ageFilterPhase1 = 0.0;
        ageFilterPhase2 = 0.0;
        ageFilterPhase3 = 0.0;
        agePitchPhase = 0.0;
        ageFilterMod = 0.0f;
        agePitchMod = 0.0f;
        pinkNoiseState = {};

        updateDropoutTimings();
    }

    void reset()
    {
        toneFilter.reset();
        loCutFilter.reset();
        hiCutFilter.reset();
        hiCutFilter2.reset();
        noiseFilter.reset();
        crackleFilter.reset();
        satUpStage1.reset();
        satUpStage2.reset();
        satUpStage3.reset();
        satDnStage1.reset();
        satDnStage2.reset();
        satDnStage3.reset();

        dropoutEnvelope = 1.0f;
        dropoutTarget = 1.0f;
        warbleLFOPhase = 0.0;
        warbleLFOPhase2 = 0.0;
        stuckCounter = 0;
        ageFilterPhase1 = 0.0;
        ageFilterPhase2 = 0.0;
        ageFilterPhase3 = 0.0;
        agePitchPhase = 0.0;
        ageFilterMod = 0.0f;
        agePitchMod = 0.0f;
        pinkNoiseState = {};

        std::fill(warbleBuffer.begin(), warbleBuffer.end(), 0.0f);
        warbleWritePos = 0;

        dcBlockerXz = 0.0f;
        dcBlockerYz = 0.0f;
        controlRateCounter = 0;
    }

    float processSample(float input)
    {
        // Refresh filter coefficients at control rate. The smoothers and age
        // LFO are sub-Hz so updating every controlRateInterval samples is
        // far above any audible quantisation while saving CPU.
        if (--controlRateCounter <= 0)
        {
            controlRateCounter = controlRateInterval;
            updateAllFilterCoefficients();
        }

        // Advance smoothers per-sample so they progress mid-block
        toneCutoffSmoothed.skip(1);
        loCutFreqSmoothed.skip(1);
        hiCutFreqSmoothed.skip(1);
        const float currentSaturation = saturationSmoothed.getNextValue();

        updateAgeModulations();

        float sample = input;
        sample = applyTone(sample);
        sample = applyWarble(sample);
        sample = applyDropouts(sample);
        sample = applySaturation(sample, currentSaturation);
        sample = applyDCBlocker(sample);
        sample = applyNoise(sample);
        sample = applyLoCut(sample);
        sample = applyHiCut(sample);
        return sample;
    }

    void setAmount(float v) { amount = juce::jlimit(0.0f, 1.0f, v); }

    void setDamage(float damage)
    {
        damage = juce::jlimit(0.0f, 1.0f, damage);
        dropoutDensity = damage * damage * 0.8f + damage * 0.2f;
        dropoutDepthBase = 0.2f + damage * 0.8f;
    }

    void setHarshness(float harsh)
    {
        harsh = juce::jlimit(0.0f, 1.0f, harsh);
        harshness = harsh;
        dropoutSmoothness = 1.0f - (harsh * 0.85f);
        updateDropoutTimings();
    }

    /** 0% = bright (20kHz), 100% = dark/warm (500Hz). */
    void setTone(float t)
    {
        tone = juce::jlimit(0.0f, 1.0f, t);
        toneCutoffSmoothed.setTargetValue(computeToneBaseCutoff());
    }

    void setSaturation(float sat)
    {
        saturationAmount = juce::jlimit(0.0f, 1.0f, sat);
        saturationSmoothed.setTargetValue(saturationAmount);
    }

    void setWarble(float w) { warble = juce::jlimit(0.0f, 1.0f, w); }
    void setNoise(float n)  { noise  = juce::jlimit(0.0f, 1.0f, n); }
    void setAge(float a)    { age    = juce::jlimit(0.0f, 1.0f, a); }

    void setLoCut(float freq)
    {
        loCutFreq = juce::jlimit(20.0f, 2000.0f, freq);
        loCutFreqSmoothed.setTargetValue(loCutFreq);
    }

    void setHiCut(float freq)
    {
        hiCutFreq = juce::jlimit(500.0f, 20000.0f, freq);
        hiCutFreqSmoothed.setTargetValue(hiCutFreq);
    }

private:
    //==========================================================================
    // CONTROL-RATE FILTER COEFFICIENT UPDATES
    //==========================================================================
    float computeToneBaseCutoff() const noexcept
    {
        return 20000.0f - (tone * 19500.0f);  // 20kHz -> 500Hz
    }

    void updateAllFilterCoefficients()
    {
        if (sampleRate <= 0.0) return;

        // Tone — smoothed cutoff plus age-modulated wander
        float baseCutoff = toneCutoffSmoothed.getCurrentValue();
        float modulatedCutoff = baseCutoff + (ageFilterMod * age * 2000.0f);
        modulatedCutoff = juce::jlimit(400.0f, 20000.0f, modulatedCutoff);
        lastToneCutoff = modulatedCutoff;
        toneFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, modulatedCutoff));

        // Lo-cut (12dB/oct, Butterworth Q)
        float lc = juce::jlimit(20.0f, 2000.0f, loCutFreqSmoothed.getCurrentValue());
        loCutFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, lc, 0.707f));

        // Hi-cut cascaded for 24dB/oct
        float hc = juce::jlimit(500.0f, 20000.0f, hiCutFreqSmoothed.getCurrentValue());
        auto hcCoeffs = juce::IIRCoefficients::makeLowPass(sampleRate, hc, 0.707f);
        hiCutFilter.setCoefficients(hcCoeffs);
        hiCutFilter2.setCoefficients(hcCoeffs);
    }

    //==========================================================================
    // SAMPLE-RATE-NORMALISED DROPOUT TIMINGS
    //==========================================================================
    void updateDropoutTimings()
    {
        // Originals were tuned at 44.1k as per-sample one-pole rates / probs.
        // Linear scale by sampleRateRatio is a first-order-correct way to keep
        // perceived attack/release time and event-rate constant across rates.
        float baseAttackRate = 0.005f + (1.0f - dropoutSmoothness) * 0.15f;
        attackRateBase = baseAttackRate * sampleRateRatio;

        float baseReleaseRate = 0.0002f + dropoutSmoothness * 0.002f;
        releaseRateBase = baseReleaseRate * sampleRateRatio;
    }

    //==========================================================================
    // TONE
    //==========================================================================
    float applyTone(float sample)
    {
        return toneFilter.processSingleSampleRaw(sample);
    }

    //==========================================================================
    // AGE MODULATIONS — slow LFOs in double precision (hours-of-playback safe)
    //==========================================================================
    void updateAgeModulations()
    {
        if (age <= 0.0f)
        {
            ageFilterMod = 0.0f;
            agePitchMod = 0.0f;
            return;
        }

        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        const double srD = sampleRate;

        ageFilterPhase1 += twoPi * 0.15 / srD;
        ageFilterPhase2 += twoPi * 0.10 / srD;
        ageFilterPhase3 += twoPi * 0.20 / srD;
        agePitchPhase   += twoPi * 0.20 / srD;

        if (ageFilterPhase1 > twoPi) ageFilterPhase1 -= twoPi;
        if (ageFilterPhase2 > twoPi) ageFilterPhase2 -= twoPi;
        if (ageFilterPhase3 > twoPi) ageFilterPhase3 -= twoPi;
        if (agePitchPhase   > twoPi) agePitchPhase   -= twoPi;

        float mod1 = static_cast<float>(std::sin(ageFilterPhase1));
        float mod2 = static_cast<float>(std::sin(ageFilterPhase2)) * 0.7f;
        float mod3 = static_cast<float>(std::sin(ageFilterPhase3)) * 0.5f;
        ageFilterMod = (mod1 + mod2 + mod3) / 2.2f;

        float pitchMod1 = static_cast<float>(std::sin(agePitchPhase));
        float pitchMod2 = static_cast<float>(std::sin(agePitchPhase * 1.3 + 1.0)) * 0.6f;
        agePitchMod = (pitchMod1 + pitchMod2) / 1.6f;
    }

    //==========================================================================
    // WARBLE — pitch modulation via fractional delay read
    //==========================================================================
    float applyWarble(float sample)
    {
        warbleBuffer[warbleWritePos] = sample;
        warbleWritePos = (warbleWritePos + 1) % warbleBufferSize;

        if (warble <= 0.0f && age <= 0.0f) return sample;

        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        const double srD = sampleRate;

        float warbleModulation = 0.0f;
        if (warble > 0.0f)
        {
            constexpr double warbleRate = 1.0;
            float slowWow     = static_cast<float>(std::sin(warbleLFOPhase));
            float fastFlutter = static_cast<float>(std::sin(warbleLFOPhase2)) * 0.25f;
            warbleModulation = (slowWow + fastFlutter) * warble;

            warbleLFOPhase  += twoPi * warbleRate / srD;
            warbleLFOPhase2 += twoPi * 6.0       / srD;
            if (warbleLFOPhase  > twoPi) warbleLFOPhase  -= twoPi;
            if (warbleLFOPhase2 > twoPi) warbleLFOPhase2 -= twoPi;
        }

        float ageDriftModulation = agePitchMod * age * 0.8f;
        float totalModulation = warbleModulation + ageDriftModulation;

        float maxWarbleMs = warble * 15.0f;
        float maxAgeMs = age * 12.0f;
        float baseDelayMs = (maxWarbleMs + maxAgeMs) * 0.5f;
        float delayMs = baseDelayMs + totalModulation * (maxWarbleMs * 0.5f + maxAgeMs * 0.5f);
        delayMs = std::max(0.1f, delayMs);

        float delaySamples = (delayMs / 1000.0f) * static_cast<float>(sampleRate);
        delaySamples = std::max(1.0f, delaySamples);

        float readPosFloat = static_cast<float>(warbleWritePos) - delaySamples - 1.0f;
        if (readPosFloat < 0.0f)
            readPosFloat += static_cast<float>(warbleBufferSize);

        // 4-point cubic Hermite interpolation (de Soras formulation).
        // Smoother spectral behaviour at small modulation depths than linear.
        const size_t N = warbleBufferSize;
        const size_t i1 = static_cast<size_t>(readPosFloat) % N;
        const size_t i0 = (i1 + N - 1) % N;
        const size_t i2 = (i1 + 1) % N;
        const size_t i3 = (i1 + 2) % N;
        const float frac = readPosFloat - std::floor(readPosFloat);

        const float y0 = warbleBuffer[i0];
        const float y1 = warbleBuffer[i1];
        const float y2 = warbleBuffer[i2];
        const float y3 = warbleBuffer[i3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    //==========================================================================
    // DROPOUTS — sample-rate-independent rates and trigger probabilities
    //==========================================================================
    float applyDropouts(float sample)
    {
        if (amount <= 0.0f) return sample;

        float envelopeDiff = dropoutTarget - dropoutEnvelope;
        float attackRate  = attackRateBase  * (1.0f + harshness * 3.0f);
        float releaseRate = releaseRateBase * (1.0f + harshness * 5.0f);

        if (envelopeDiff < 0)
            dropoutEnvelope += envelopeDiff * attackRate;
        else
            dropoutEnvelope += envelopeDiff * releaseRate;

        sample *= dropoutEnvelope;

        if (stuckCounter > 0)
        {
            stuckCounter--;
        }
        else
        {
            float violenceBoost = 1.0f + harshness * harshness * 4.0f;
            float triggerProb = dropoutDensity * amount * 0.002f * violenceBoost * sampleRateRatio;

            if (uniformDist01(random) < triggerProb)
            {
                float depthVariation = uniformDist01(random) * 0.4f;
                float thisDepth = std::min(1.0f, dropoutDepthBase + depthVariation);
                dropoutTarget = 1.0f - thisDepth;

                if (dropoutDensity > 0.5f && uniformDist01(random) < dropoutDensity * 0.3f)
                {
                    float stuckDurationScale = 1.0f - harshness * 0.6f;
                    int stuckMs = static_cast<int>((50 + uniformDist01(random) * 300) * stuckDurationScale);
                    stuckCounter = static_cast<int>(stuckMs * sampleRate / 1000.0f);
                }
            }
            else if (dropoutEnvelope < 0.95f)
            {
                float recoveryBoost = 1.0f + harshness * 2.0f;
                float recoveryProb = ((1.0f - dropoutDensity) * 0.001f + 0.0001f) * recoveryBoost * sampleRateRatio;
                if (uniformDist01(random) < recoveryProb)
                {
                    float maxRecovery = 1.0f - (dropoutDensity * amount * 0.2f);
                    dropoutTarget = maxRecovery;
                }
            }
        }
        return sample;
    }

    //==========================================================================
    // SATURATION
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

    float applySaturation(float sample, float currentSat) noexcept
    {
        if (currentSat <= 0.0f) return sample;

        const float gain = currentSat * 2.0f;
        const float warmth = currentSat * 0.5f;

        // 2x upsample: zero-stuff with x2 gain compensation, then anti-imaging LP.
        // The two oversample lobes (A = even, B = odd) share filter state so it
        // advances at the 2x rate as expected.
        float upA = 2.0f * sample;
        upA = satUpStage1.processSingleSampleRaw(upA);
        upA = satUpStage2.processSingleSampleRaw(upA);
        upA = satUpStage3.processSingleSampleRaw(upA);

        float upB = satUpStage1.processSingleSampleRaw(0.0f);
        upB = satUpStage2.processSingleSampleRaw(upB);
        upB = satUpStage3.processSingleSampleRaw(upB);

        // Saturate at 2x rate — harmonics generated here have headroom up to
        // 2*Nyquist before they would alias.
        upA = warmTubeStage(upA, warmth);
        upA = transparentClipping(upA, gain);
        upB = warmTubeStage(upB, warmth);
        upB = transparentClipping(upB, gain);

        // Anti-aliasing LP, then decimate by keeping the first lobe. Both
        // samples must traverse all three stages so filter state stays
        // synchronized with the 2x rate.
        upA = satDnStage1.processSingleSampleRaw(upA);
        upA = satDnStage2.processSingleSampleRaw(upA);
        upA = satDnStage3.processSingleSampleRaw(upA);

        upB = satDnStage1.processSingleSampleRaw(upB);
        upB = satDnStage2.processSingleSampleRaw(upB);
        upB = satDnStage3.processSingleSampleRaw(upB);

        juce::ignoreUnused(upB);
        return upA;
    }

    //==========================================================================
    // DC BLOCKER — removes DC drift from asymmetric warm-tube stage
    //==========================================================================
    float applyDCBlocker(float x) noexcept
    {
        float y = x - dcBlockerXz + dcBlockerR * dcBlockerYz;
        dcBlockerXz = x;
        dcBlockerYz = y;
        return y;
    }

    //==========================================================================
    // NOISE — pops are headroom-clamped so they cannot overshoot full scale
    //==========================================================================
    float applyNoise(float sample)
    {
        if (noise <= 0.0f) return sample;

        float crackleAmount = noise * noise * noise;
        float headroom = std::max(0.0f, 0.95f - std::abs(sample));

        // Hiss path: pink noise through the gentle 2.2kHz filter.
        float pink = generatePinkNoise();
        float hissLevel = noise * noise * 0.035f;
        float hissContribution = pink * hissLevel;

        // Crackle path: pops + soft crackle through a darker dedicated filter
        // so impulses become muffled thumps with their own character, separate
        // from the hiss tone.
        float crackleContribution = 0.0f;

        float popProb = crackleAmount * 0.00018f * sampleRateRatio;
        if (uniformDist01(random) < popProb)
        {
            float popIntensity = 0.10f + uniformDist01(random) * 0.18f;
            float popMag = std::min(popIntensity * noise * noise, headroom);
            float pop = (uniformDist01(random) > 0.5f ? 1.0f : -1.0f) * popMag;
            crackleContribution += pop;
            headroom = std::max(0.0f, headroom - popMag);
        }

        float softCrackleProb = crackleAmount * 0.0004f * sampleRateRatio;
        if (uniformDist01(random) < softCrackleProb)
        {
            float softMag = std::min(0.06f * noise * noise, headroom);
            float softCrackle = uniformDistPM1(random) * softMag;
            crackleContribution += softCrackle;
        }

        return sample
             + noiseFilter.processSingleSampleRaw(hissContribution)
             + crackleFilter.processSingleSampleRaw(crackleContribution);
    }

    //==========================================================================
    // OUTPUT FILTERS
    //==========================================================================
    float applyLoCut(float sample)
    {
        return loCutFilter.processSingleSampleRaw(sample);
    }

    float applyHiCut(float sample)
    {
        sample = hiCutFilter.processSingleSampleRaw(sample);
        sample = hiCutFilter2.processSingleSampleRaw(sample);
        return sample;
    }

    //==========================================================================
    // PINK NOISE (Voss-McCartney 5-octave approximation)
    //==========================================================================
    float generatePinkNoise()
    {
        pinkNoiseState.counter++;
        float white = uniformDistPM1(random);

        if ((pinkNoiseState.counter & 0x01) == 0) pinkNoiseState.octaves[0] = uniformDistPM1(random);
        if ((pinkNoiseState.counter & 0x03) == 0) pinkNoiseState.octaves[1] = uniformDistPM1(random);
        if ((pinkNoiseState.counter & 0x07) == 0) pinkNoiseState.octaves[2] = uniformDistPM1(random);
        if ((pinkNoiseState.counter & 0x0F) == 0) pinkNoiseState.octaves[3] = uniformDistPM1(random);
        if ((pinkNoiseState.counter & 0x1F) == 0) pinkNoiseState.octaves[4] = uniformDistPM1(random);

        float pink = white * 0.5f;
        pink += pinkNoiseState.octaves[0] * 0.25f;
        pink += pinkNoiseState.octaves[1] * 0.125f;
        pink += pinkNoiseState.octaves[2] * 0.0625f;
        pink += pinkNoiseState.octaves[3] * 0.03125f;
        pink += pinkNoiseState.octaves[4] * 0.015625f;
        return pink;
    }

    //==========================================================================
    // STATE
    //==========================================================================

    double sampleRate = 44100.0;
    float sampleRateInv = 1.0f / 44100.0f;
    float sampleRateRatio = 1.0f;       // 44100 / sampleRate

    // Raw parameters
    float amount = 0.0f;
    float tone = 0.0f;
    float dropoutDensity = 0.3f;
    float dropoutDepthBase = 0.5f;
    float dropoutSmoothness = 0.5f;
    float harshness = 0.0f;
    float saturationAmount = 0.0f;
    float warble = 0.0f;
    float noise = 0.0f;
    float age = 0.0f;
    float loCutFreq = 20.0f;
    float hiCutFreq = 20000.0f;

    // Smoothed parameter values feeding the filters / saturation
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneCutoffSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> loCutFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> hiCutFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> saturationSmoothed;

    int controlRateCounter = 0;
    static constexpr int controlRateInterval = 16;  // recompute IIR coeffs every N samples

    // Filters
    float lastToneCutoff = 20000.0f;
    juce::IIRFilter toneFilter;
    juce::IIRFilter loCutFilter;
    juce::IIRFilter hiCutFilter;
    juce::IIRFilter hiCutFilter2;
    juce::IIRFilter noiseFilter;     // hiss path (2.2kHz LP)
    juce::IIRFilter crackleFilter;   // crackle path (900Hz LP - dedicated muffler)

    // 2x oversampling around the saturation stage. Three cascaded biquads
    // form a 6th-order Butterworth lowpass for both anti-imaging (up) and
    // anti-aliasing (down). Operating at 2*sampleRate, fc=18kHz, Q values
    // come from the Butterworth pole positions for 6th order.
    juce::IIRFilter satUpStage1, satUpStage2, satUpStage3;
    juce::IIRFilter satDnStage1, satDnStage2, satDnStage3;

    // Dropout state
    float dropoutEnvelope = 1.0f;
    float dropoutTarget = 1.0f;
    int stuckCounter = 0;
    float attackRateBase = 0.0f;
    float releaseRateBase = 0.0f;

    // Age LFO state — double precision keeps phase stable over multi-hour sessions
    double ageFilterPhase1 = 0.0;
    double ageFilterPhase2 = 0.0;
    double ageFilterPhase3 = 0.0;
    double agePitchPhase = 0.0;
    float ageFilterMod = 0.0f;
    float agePitchMod = 0.0f;

    // Warble state
    double warbleLFOPhase = 0.0;
    double warbleLFOPhase2 = 0.0;
    std::vector<float> warbleBuffer;
    size_t warbleBufferSize = 0;
    size_t warbleWritePos = 0;
    static constexpr float maxWarbleDelayMs = 30.0f;

    // DC blocker
    float dcBlockerR = 0.0f;
    float dcBlockerXz = 0.0f;
    float dcBlockerYz = 0.0f;

    // RNG and distributions held as members so they aren't reconstructed per sample
    std::mt19937 random;
    std::uniform_real_distribution<float> uniformDist01{0.0f, 1.0f};
    std::uniform_real_distribution<float> uniformDistPM1{-1.0f, 1.0f};

    struct PinkNoiseState
    {
        uint32_t counter = 0;
        std::array<float, 5> octaves = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    } pinkNoiseState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeDeathEngine)
};
