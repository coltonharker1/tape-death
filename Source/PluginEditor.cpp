#include "PluginProcessor.h"
#include "PluginEditor.h"

// Color constants and font helpers are now sourced from sonorous-kit. The
// local names here are kept (as constexpr forwarders / thin wrappers) so the
// rest of this file's call sites don't need to change.
namespace
{
constexpr juce::uint32 paper    = sonorous::palette::paper;
constexpr juce::uint32 ink      = sonorous::palette::ink;
constexpr juce::uint32 quietInk = sonorous::palette::quietInk;
constexpr juce::uint32 faintInk = sonorous::palette::faintInk;

juce::FontOptions monoFont(juce::Typeface::Ptr typeface, float height)
{
    return juce::FontOptions(typeface).withHeight(height);
}

juce::FontOptions serifFont(juce::Typeface::Ptr typeface, float height)
{
    return juce::FontOptions(typeface).withHeight(height);
}
}

TapeDeathAudioProcessorEditor::TapeDeathAudioProcessorEditor(TapeDeathAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Fonts come from sonorous-kit; the local Ptr members keep their names so
    // the rest of the editor's monoFont(...)/serifFont(...) calls don't change.
    instrumentSerifRegular = sonorous::typography::instrumentSerifRegular();
    instrumentSerifItalic  = sonorous::typography::instrumentSerifItalic();
    jetBrainsMono          = sonorous::typography::jetBrainsMono();

    auto& params = audioProcessor.getParameters();

    setupKnob(amountKnob, "amount", "Loss", params);
    setupKnob(damageKnob, "damage", "Rot", params);
    setupKnob(harshnessKnob, "harshness", "Snap", params);
    setupKnob(warbleKnob, "warble", "Warble", params);
    setupKnob(ageKnob, "age", "Age", params);
    setupKnob(saturationKnob, "saturation", "Saturation", params);
    setupKnob(noiseKnob, "noise", "Noise", params);
    setupKnob(toneKnob, "tone", "Darkness", params);
    setupKnob(loCutKnob, "loCut", "Lo-Cut", params);
    setupKnob(hiCutKnob, "hiCut", "Hi-Cut", params);
    setupKnob(dryWetKnob, "dryWet", "Dry/Wet", params);

    updateReadouts();
    startTimerHz(30);
    setSize(760, 500);
}

TapeDeathAudioProcessorEditor::~TapeDeathAudioProcessorEditor()
{
    stopTimer();

    for (auto* child : getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
            slider->setLookAndFeel(nullptr);
}

void TapeDeathAudioProcessorEditor::timerCallback()
{
    const float processorActivity = audioProcessor.getAudioActivity();
    if (processorActivity > 0.018f)
        signalHoldFrames = 28;
    else if (signalHoldFrames > 0)
        --signalHoldFrames;

    const float targetActivity = signalHoldFrames > 0 ? juce::jmax(processorActivity, 0.28f) : processorActivity;
    visualActivity += (targetActivity - visualActivity) * (targetActivity > visualActivity ? 0.22f : 0.08f);

    const float motion = visualActivity * visualActivity;
    reelPhase = std::fmod(reelPhase - motion * 0.19f, juce::MathConstants<float>::twoPi);
    tapeTravel = std::fmod(tapeTravel + motion * 0.058f, 1.0f);

    updateReadouts();
    repaint(diagramBounds);
}

void TapeDeathAudioProcessorEditor::setupKnob(KnobWithLabel& knob, const juce::String& paramId,
                                             const juce::String& labelText,
                                             juce::AudioProcessorValueTreeState& params)
{
    knob.paramId = paramId;

    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setRotaryParameters(juce::MathConstants<float>::pi * 0.75f,
                                    juce::MathConstants<float>::pi * 2.25f,
                                    true);
    knob.slider.setMouseDragSensitivity(180);
    knob.slider.setLookAndFeel(&monoLookAndFeel);
    knob.slider.onValueChange = [this] { updateReadouts(); repaint(diagramBounds); };
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText.toUpperCase(), juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, juce::Colour(quietInk));
    knob.label.setFont(monoFont(jetBrainsMono, 8.0f));
    addAndMakeVisible(knob.label);

    knob.value.setJustificationType(juce::Justification::centred);
    knob.value.setColour(juce::Label::textColourId, juce::Colour(quietInk));
    knob.value.setFont(serifFont(instrumentSerifItalic, 14.0f));
    addAndMakeVisible(knob.value);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        params, paramId, knob.slider);
}

void TapeDeathAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(paper));

    drawHeader(g, bounds.removeFromTop(58));

    g.setColour(juce::Colour(ink));
    g.drawRect(bounds, 1);

    drawDiagram(g, diagramBounds);

    auto controls = bounds.removeFromBottom(162).reduced(18, 0);
    const int gap = 10;
    const int sectionW = (controls.getWidth() - gap * 3) / 4;

    auto damage = controls.removeFromLeft(sectionW);
    controls.removeFromLeft(gap);
    auto motion = controls.removeFromLeft(sectionW);
    controls.removeFromLeft(gap);
    auto material = controls.removeFromLeft(sectionW);
    controls.removeFromLeft(gap);
    auto output = controls;

    drawSection(g, damage, "Dropouts", "loss / rot / snap");
    drawSection(g, motion, "Motion", "pitch drift");
    drawSection(g, material, "Material", "oxide grain");
    drawSection(g, output, "Output", "print blend");
}

void TapeDeathAudioProcessorEditor::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto header = bounds.reduced(22, 0);

    g.setColour(juce::Colour(ink));
    g.setFont(serifFont(instrumentSerifItalic, 36.0f));
    g.drawText("Tape Death", header.removeFromLeft(250), juce::Justification::centredLeft);

    g.setFont(monoFont(jetBrainsMono, 9.0f));
    g.drawText("PLATE XI - MAGNETIC DECAY / LISTEN FIRST", header, juce::Justification::centredRight);

    g.drawLine(0.0f, static_cast<float>(bounds.getBottom()) - 0.5f,
               static_cast<float>(getWidth()), static_cast<float>(bounds.getBottom()) - 0.5f, 1.5f);
}

void TapeDeathAudioProcessorEditor::drawDiagram(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    auto& params = audioProcessor.getParameters();
    const auto get = [&params](const char* id)
    {
        if (auto* p = params.getRawParameterValue(id))
            return p->load();
        return 0.0f;
    };

    const float amount = get("amount") / 100.0f;
    const float rot = get("damage") / 100.0f;
    const float violence = get("harshness") / 100.0f;
    const float warble = get("warble") / 100.0f;
    const float age = get("age") / 100.0f;
    const float noise = get("noise") / 100.0f;
    const float sat = get("saturation") / 100.0f;

    g.setColour(juce::Colour(faintInk));
    for (int x = bounds.getX(); x < bounds.getRight(); x += 34)
        for (int y = bounds.getY(); y < bounds.getBottom(); y += 34)
            g.fillEllipse(static_cast<float>(x), static_cast<float>(y), 1.0f, 1.0f);

    auto plate = bounds.reduced(72, 34);
    auto cassette = plate.withSizeKeepingCentre(juce::jmin(plate.getWidth(), 560), 170);
    g.setColour(juce::Colour(ink));
    g.drawRect(cassette.toFloat(), 1.4f);

    auto innerPlate = cassette.reduced(18, 16);
    g.drawRect(innerPlate.toFloat(), 0.75f);

    for (auto corner : { innerPlate.getTopLeft(), innerPlate.getTopRight(),
                         innerPlate.getBottomLeft(), innerPlate.getBottomRight() })
    {
        g.drawEllipse(static_cast<float>(corner.x) - 3.0f, static_cast<float>(corner.y) - 3.0f,
                      6.0f, 6.0f, 0.7f);
    }

    g.setFont(monoFont(jetBrainsMono, 7.5f));
    auto topText = innerPlate.reduced(10, 0).removeFromTop(22);
    g.drawText("MAGNETIC DECAY", topText, juce::Justification::centredLeft);
    g.drawText("LISTEN FIRST", topText, juce::Justification::centredRight);

    const float reelR = 42.0f;
    const float reelY = static_cast<float>(cassette.getY()) + 72.0f;
    const float lcx = static_cast<float>(cassette.getX()) + 150.0f;
    const float rcx = static_cast<float>(cassette.getRight()) - 150.0f;
    const float lowerY = static_cast<float>(cassette.getBottom()) - 24.0f;
    const float leftGuideX = static_cast<float>(cassette.getX()) + 46.0f;
    const float rightGuideX = static_cast<float>(cassette.getRight()) - 46.0f;
    const float guideR = 7.0f;
    const float tapeAlpha = 1.0f - age * 0.58f;
    const float ribbonWidth = 5.6f;
    const float signalWidth = 1.05f + sat * 0.7f;

    g.setColour(juce::Colour(ink).withAlpha(0.18f));
    g.drawLine(static_cast<float>(cassette.getX()) + 62.0f, lowerY + 17.0f,
               static_cast<float>(cassette.getRight()) - 62.0f, lowerY + 17.0f, 0.8f);
    g.drawLine(leftGuideX + 22.0f, lowerY + 10.0f, leftGuideX + 92.0f, lowerY + 10.0f, 0.7f);
    g.drawLine(rightGuideX - 92.0f, lowerY + 10.0f, rightGuideX - 22.0f, lowerY + 10.0f, 0.7f);

    auto drawReel = [&](float cx, float phase)
    {
        g.setColour(juce::Colour(paper));
        g.fillEllipse(cx - reelR, reelY - reelR, reelR * 2.0f, reelR * 2.0f);
        g.setColour(juce::Colour(ink));
        g.drawEllipse(cx - reelR, reelY - reelR, reelR * 2.0f, reelR * 2.0f, 1.3f);
        g.drawEllipse(cx - reelR * 0.72f, reelY - reelR * 0.72f, reelR * 1.44f, reelR * 1.44f, 0.75f);
        g.drawEllipse(cx - reelR * 0.46f, reelY - reelR * 0.46f, reelR * 0.92f, reelR * 0.92f, 0.75f);
        g.drawEllipse(cx - 6.0f, reelY - 6.0f, 12.0f, 12.0f, 0.9f);

        for (int i = 0; i < 8; ++i)
        {
            const float a = phase + juce::MathConstants<float>::twoPi * static_cast<float>(i) / 8.0f;
            g.drawLine(cx, reelY, cx + std::cos(a) * (reelR - 11.0f),
                       reelY + std::sin(a) * (reelR - 11.0f), 0.8f);
        }
    };

    struct TapePoint
    {
        float x;
        float y;
    };

    const TapePoint leftReelExit { lcx - reelR * 0.98f, reelY + reelR * 0.30f };
    const TapePoint leftGuide { leftGuideX, lowerY - guideR };
    const TapePoint rightGuide { rightGuideX, lowerY - guideR };
    const TapePoint rightReelEntry { rcx + reelR * 0.98f, reelY + reelR * 0.30f };

    auto drawRibbonSegment = [&](TapePoint a, TapePoint b, float alpha)
    {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.0f)
            return;

        const auto n = juce::Point<float>(-dy / len, dx / len) * (ribbonWidth * 0.5f);
        juce::Path segment;
        segment.startNewSubPath(a.x + n.x, a.y + n.y);
        segment.lineTo(b.x + n.x, b.y + n.y);
        segment.lineTo(b.x - n.x, b.y - n.y);
        segment.lineTo(a.x - n.x, a.y - n.y);
        segment.closeSubPath();

        g.setColour(juce::Colour(ink).withAlpha(alpha * 0.10f));
        g.fillPath(segment);
        g.setColour(juce::Colour(ink).withAlpha(alpha * 0.14f));
        g.strokePath(segment, juce::PathStrokeType(0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };

    drawRibbonSegment(leftReelExit, leftGuide, tapeAlpha);
    drawRibbonSegment(rightGuide, rightReelEntry, tapeAlpha);

    std::array<TapePoint, 96> activeTapePoints;
    for (size_t i = 0; i < activeTapePoints.size(); ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(activeTapePoints.size() - 1);
        const float x = leftGuide.x + (rightGuide.x - leftGuide.x) * t;
        const float sag = std::sin(t * juce::MathConstants<float>::pi) * (amount * 2.2f);
        const float flutter = std::sin(t * 11.0f + tapeTravel * juce::MathConstants<float>::twoPi * visualActivity) * warble * 1.7f;
        activeTapePoints[i] = { x, lowerY - guideR + sag + flutter };
    }

    auto normalAt = [&activeTapePoints](size_t i)
    {
        const auto prev = activeTapePoints[i == 0 ? 0 : i - 1];
        const auto next = activeTapePoints[juce::jmin(i + 1, activeTapePoints.size() - 1)];
        const float dx = next.x - prev.x;
        const float dy = next.y - prev.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.0f)
            return juce::Point<float>(0.0f, 1.0f);

        return juce::Point<float>(-dy / len, dx / len);
    };

    juce::Path ribbon;
    for (size_t i = 0; i < activeTapePoints.size(); ++i)
    {
        const auto n = normalAt(i);
        const auto p = juce::Point<float>(activeTapePoints[i].x, activeTapePoints[i].y) + n * (ribbonWidth * 0.5f);

        if (i == 0)
            ribbon.startNewSubPath(p);
        else
            ribbon.lineTo(p);
    }
    for (size_t i = activeTapePoints.size(); i-- > 0;)
    {
        const auto n = normalAt(i);
        const auto p = juce::Point<float>(activeTapePoints[i].x, activeTapePoints[i].y) - n * (ribbonWidth * 0.5f);
        ribbon.lineTo(p);
    }
    ribbon.closeSubPath();

    g.setColour(juce::Colour(ink).withAlpha(tapeAlpha * 0.10f));
    g.fillPath(ribbon);
    g.setColour(juce::Colour(ink).withAlpha(tapeAlpha * 0.14f));
    g.strokePath(ribbon, juce::PathStrokeType(0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    g.setColour(juce::Colour(paper));
    g.fillEllipse(leftGuideX - guideR, lowerY - guideR * 2.0f, guideR * 2.0f, guideR * 2.0f);
    g.fillEllipse(rightGuideX - guideR, lowerY - guideR * 2.0f, guideR * 2.0f, guideR * 2.0f);
    g.setColour(juce::Colour(ink));
    g.drawEllipse(leftGuideX - guideR, lowerY - guideR * 2.0f, guideR * 2.0f, guideR * 2.0f, 0.9f);
    g.drawEllipse(rightGuideX - guideR, lowerY - guideR * 2.0f, guideR * 2.0f, guideR * 2.0f, 0.9f);
    const int notchCount = static_cast<int>(std::round(amount * (2.0f + violence * 10.0f)));
    auto notchDepthAt = [notchCount, rot, violence, travel = tapeTravel](float t)
    {
        float depth = 0.0f;

        for (int n = 0; n < notchCount; ++n)
        {
            float centre = 0.08f + static_cast<float>((n * 37) % 85) / 100.0f + travel;
            centre -= std::floor(centre);
            const float halfWidth = 0.012f + rot * 0.026f + static_cast<float>((n * 11) % 5) * 0.002f;
            const float rawDistance = std::abs(t - centre);
            const float d = juce::jmin(rawDistance, 1.0f - rawDistance);

            if (d < halfWidth)
            {
                const float x = d / halfWidth;
                const float soft = x * x * (3.0f - 2.0f * x);
                const float hard = x < 0.82f ? 0.0f : (x - 0.82f) / 0.18f;
                const float profile = juce::jmap(violence, soft, hard);
                depth = juce::jmax(depth, juce::jlimit(0.0f, 1.0f, 1.0f - profile) * (0.24f + rot * 0.76f));
            }
        }

        return depth;
    };

    for (size_t i = 1; i < activeTapePoints.size(); ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(activeTapePoints.size() - 1);
        const float prevT = static_cast<float>(i - 1) / static_cast<float>(activeTapePoints.size() - 1);
        const float depth = juce::jmax(notchDepthAt(prevT), notchDepthAt(t));
        const float localAlpha = tapeAlpha * juce::jlimit(0.10f, 1.0f, 1.0f - depth * (0.86f + rot * 0.12f));
        const float localWidth = juce::jmax(0.38f, signalWidth * (1.0f - depth * 0.34f));

        g.setColour(juce::Colour(ink).withAlpha(localAlpha));
        g.drawLine(activeTapePoints[i - 1].x, activeTapePoints[i - 1].y,
                   activeTapePoints[i].x, activeTapePoints[i].y, localWidth);
    }

    const int speckleCount = static_cast<int>(noise * 120.0f);
    if (speckleCount > 0)
    {
        g.setColour(juce::Colour(ink).withAlpha(0.24f + noise * 0.26f));

        for (int i = 0; i < speckleCount; ++i)
        {
            const int travelOffset = static_cast<int>(tapeTravel * static_cast<float>(activeTapePoints.size() - 2));
            const size_t idx = static_cast<size_t>((i * 29 + travelOffset) % static_cast<int>(activeTapePoints.size() - 2)) + 1;
            const auto prev = activeTapePoints[idx - 1];
            const auto next = activeTapePoints[idx + 1];
            const float dx = next.x - prev.x;
            const float dy = next.y - prev.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            const float nx = len > 0.0f ? -dy / len : 0.0f;
            const float ny = len > 0.0f ? dx / len : 1.0f;
            const float along = static_cast<float>((i * 13) % 9 - 4) * 0.45f;
            const float lift = static_cast<float>((i * 31) % 7 - 3) * (0.42f + noise * 0.38f);
            const float x = activeTapePoints[idx].x + (len > 0.0f ? dx / len : 1.0f) * along + nx * lift;
            const float y = activeTapePoints[idx].y + (len > 0.0f ? dy / len : 0.0f) * along + ny * lift;

            if (i % 7 == 0)
                g.drawLine(x - nx * 1.9f, y - ny * 1.9f, x + nx * 1.9f, y + ny * 1.9f, 0.75f);
            else
                g.fillRect(juce::Rectangle<float>(x, y, 1.2f, 1.2f));
        }
    }

    drawReel(lcx, reelPhase);
    drawReel(rcx, reelPhase * 0.86f + 0.4f);

    g.setColour(juce::Colour(ink));
    g.setFont(monoFont(jetBrainsMono, 8.0f));
    auto ledger = bounds.reduced(14, 8).removeFromTop(18);
    g.drawText("OXIDE MAP", ledger, juce::Justification::centredLeft);

    juce::String condition = "STABLE";
    if (amount + rot + violence > 2.0f)
        condition = "BREAKING";
    else if (amount + rot + violence > 1.2f)
        condition = "FRAYED";
    else if (amount + rot + violence > 0.5f)
        condition = "WORN";

    juce::String motion = "STEADY";
    if (warble > 0.65f)
        motion = "ADRIFT";
    else if (warble > 0.24f)
        motion = "WAVERING";

    g.drawText(condition + " / " + motion, ledger, juce::Justification::centredRight);
}

void TapeDeathAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(58);
    diagramBounds = bounds.reduced(18, 18);
    diagramBounds.removeFromBottom(172);

    auto controls = bounds.removeFromBottom(162).reduced(18, 0);
    const int gap = 10;
    const int sectionW = (controls.getWidth() - gap * 3) / 4;

    auto damage = controls.removeFromLeft(sectionW).reduced(10, 34);
    controls.removeFromLeft(gap);
    auto motion = controls.removeFromLeft(sectionW).reduced(10, 34);
    controls.removeFromLeft(gap);
    auto material = controls.removeFromLeft(sectionW).reduced(10, 34);
    controls.removeFromLeft(gap);
    auto output = controls.reduced(10, 34);

    auto layoutThree = [this](juce::Rectangle<int> area, KnobWithLabel& a, KnobWithLabel& b, KnobWithLabel& c)
    {
        const int w = area.getWidth() / 3;
        positionControl(a, area.removeFromLeft(w));
        positionControl(b, area.removeFromLeft(w));
        positionControl(c, area);
    };

    auto layoutTwo = [this](juce::Rectangle<int> area, KnobWithLabel& a, KnobWithLabel& b)
    {
        const int w = area.getWidth() / 2;
        positionControl(a, area.removeFromLeft(w));
        positionControl(b, area);
    };

    layoutThree(damage, amountKnob, damageKnob, harshnessKnob);
    layoutTwo(motion, warbleKnob, ageKnob);
    layoutThree(material, saturationKnob, noiseKnob, toneKnob);
    layoutThree(output, loCutKnob, hiCutKnob, dryWetKnob);
}

void TapeDeathAudioProcessorEditor::drawSection(juce::Graphics& g, juce::Rectangle<int> bounds,
                                                const juce::String& title, const juce::String& subtitle)
{
    g.setColour(juce::Colour(ink));
    g.drawRect(bounds, 1);

    auto header = bounds.removeFromTop(30).reduced(10, 0);
    g.setFont(monoFont(jetBrainsMono, 8.5f));
    g.drawText(title.toUpperCase(), header, juce::Justification::centredLeft);

    g.setFont(serifFont(instrumentSerifItalic, 13.0f));
    g.drawText(subtitle, header, juce::Justification::centredRight);

    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getY()), 1.0f);
}

void TapeDeathAudioProcessorEditor::positionControl(KnobWithLabel& knob, juce::Rectangle<int> bounds)
{
    bounds.reduce(3, 0);
    const int knobSize = juce::jmin(64, bounds.getWidth());
    knob.slider.setBounds(bounds.withSizeKeepingCentre(knobSize, knobSize).withY(bounds.getY()));
    knob.label.setBounds(bounds.withY(knob.slider.getBottom() + 3).withHeight(18));
    knob.value.setBounds(bounds.withY(knob.label.getBottom() - 1).withHeight(20));
}

void TapeDeathAudioProcessorEditor::updateReadouts()
{
    amountKnob.value.setText(formatParameterValue(amountKnob.paramId), juce::dontSendNotification);
    damageKnob.value.setText(formatParameterValue(damageKnob.paramId), juce::dontSendNotification);
    toneKnob.value.setText(formatParameterValue(toneKnob.paramId), juce::dontSendNotification);
    harshnessKnob.value.setText(formatParameterValue(harshnessKnob.paramId), juce::dontSendNotification);
    saturationKnob.value.setText(formatParameterValue(saturationKnob.paramId), juce::dontSendNotification);
    warbleKnob.value.setText(formatParameterValue(warbleKnob.paramId), juce::dontSendNotification);
    noiseKnob.value.setText(formatParameterValue(noiseKnob.paramId), juce::dontSendNotification);
    ageKnob.value.setText(formatParameterValue(ageKnob.paramId), juce::dontSendNotification);
    loCutKnob.value.setText(formatParameterValue(loCutKnob.paramId), juce::dontSendNotification);
    hiCutKnob.value.setText(formatParameterValue(hiCutKnob.paramId), juce::dontSendNotification);
    dryWetKnob.value.setText(formatParameterValue(dryWetKnob.paramId), juce::dontSendNotification);
}

juce::String TapeDeathAudioProcessorEditor::formatParameterValue(const juce::String& paramId) const
{
    auto& params = audioProcessor.getParameters();
    if (auto* parameter = params.getParameter(paramId))
    {
        const float normalised = parameter->getValue();

        if (paramId == "loCut")
            return normalised < 0.08f ? "open" : normalised < 0.45f ? "lifted" : "thin";

        if (paramId == "hiCut")
            return normalised > 0.92f ? "open" : normalised > 0.55f ? "veiled" : "closed";

        if (paramId == "dryWet")
            return normalised < 0.18f ? "dry" : normalised < 0.72f ? "parallel" : "printed";

        if (paramId == "tone")
            return normalised < 0.18f ? "clear" : normalised < 0.6f ? "warmed" : "buried";

        if (paramId == "amount")
            return normalised < 0.18f ? "none" : normalised < 0.55f ? "some" : normalised < 0.82f ? "many" : "constant";

        if (paramId == "damage")
            return normalised < 0.18f ? "shallow" : normalised < 0.55f ? "dipped" : normalised < 0.82f ? "deep" : "stuck";

        if (paramId == "harshness")
            return normalised < 0.18f ? "soft" : normalised < 0.55f ? "firm" : normalised < 0.82f ? "cut" : "chop";

        if (paramId == "warble" || paramId == "age")
            return normalised < 0.18f ? "still" : normalised < 0.55f ? "loose" : normalised < 0.82f ? "adrift" : "lost";

        if (paramId == "saturation")
            return normalised < 0.18f ? "clean" : normalised < 0.55f ? "warm" : normalised < 0.82f ? "pushed" : "hot";

        if (paramId == "noise")
            return normalised < 0.18f ? "quiet" : normalised < 0.55f ? "dust" : normalised < 0.82f ? "hiss" : "static";
    }

    return {};
}

// MonoPlateLookAndFeel::drawRotarySlider was moved into sonorous_visual's
// SonorousLookAndFeel. The editor's monoLookAndFeel member is now a
// SonorousLookAndFeel instance, so the rotary draw routine comes from the kit.
