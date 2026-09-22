#include "PluginEditor.h"
#include "PluginParameters.h"

// ── Layout constants (pixels) ────────────────────────────────────────────────
static constexpr int kEar    = 38;
static constexpr int kMeterW = 36;
static constexpr int kH      = 172;   // control-strip height (matches SaturationModel1's knob proportions)

// Preset bar — a slim strip at the very top of the window, above the shape displays.
static constexpr int kPresetBarY = 6;
static constexpr int kPresetBarH = 24;

static constexpr int kDisplayTop = kPresetBarY + kPresetBarH + 10;
static constexpr int kDisplayH   = 110;
static constexpr int kRackY      = kDisplayTop + kDisplayH + 14;
static constexpr int kTotalH     = kRackY + kH;

// Column widths within the faceplate, left to right = signal-flow order.
static constexpr int kColPad  = 20;
static constexpr int kColGap  = 20;
static constexpr int kDistW   = 300;
static constexpr int kFiltW   = 196;  // must match FilterComponent's own internal calibration
static constexpr int kModelW  = 260;

static constexpr int kFaceW = kColPad + kDistW + kColGap + kFiltW + kColGap + kModelW + kColPad;
static constexpr int kTotalW = kMeterW * 2 + kEar * 2 + kFaceW;

static constexpr int kFaceX  = kMeterW + kEar;
static constexpr int kDistX  = kFaceX + kColPad;
static constexpr int kFiltX  = kDistX + kDistW + kColGap;
static constexpr int kModelX = kFiltX + kFiltW + kColGap;

// Modeling column: Drive / [latency buttons, stacked] / Output.
static constexpr int kModelKnobY    = kRackY + 94;  // leaves room for modelSwitch (ends kRackY+78) + the LATENCY label above the buttons
static constexpr int kModelBtnStripW = 60;
static constexpr int kModelKnobW    = (kModelW - kModelBtnStripW) / 2;
static constexpr int kModelBtnX     = kModelX + kModelKnobW;

// Distortion column's top slot: shared side-by-side between the nameplate
// (left) and the transfer-curve display (right) — Filter and Modeling each get
// a full-width display up there, but Distortion splits its slot between the two.
static constexpr int kTopSlotMargin = 15;
static constexpr int kTopSlotGap    = 10;
static constexpr int kTopSlotY      = kDisplayTop + 10;
static constexpr int kTopSlotH      = kDisplayH - 20;
static constexpr int kTopSlotItemW  = (kDistW - 2 * kTopSlotMargin - kTopSlotGap) / 2;
static constexpr int kNameplateX    = kDistX + kTopSlotMargin;
static constexpr int kCurveX        = kNameplateX + kTopSlotItemW + kTopSlotGap;

ExpandedSaturationEditor::ExpandedSaturationEditor(ExpandedSaturationProcessor& p)
    : AudioProcessorEditor(&p),
      pluginProcessor(p),
      presetBar(p.getPresetManager()),
      filterShapeDisplay(p.apvts),
      transferCurveDisplay(p.apvts),
      distortionSelector(p.apvts, SaturationModeEngine::getModeNames()),
      distortionDriveKnob (p.apvts, ParamIDs::algoDrive.getParamID(),  "DRIVE"),
      distortionOutputKnob(p.apvts, ParamIDs::algoOutput.getParamID(), "OUTPUT"),
      distortionMixKnob   (p.apvts, ParamIDs::algoMix.getParamID(),    "MIX"),
      filterComponent(p.apvts),
      modelSwitch(p.apvts),
      modelDriveKnob (p.apvts, ParamIDs::modelDrive.getParamID(),  "DRIVE"),
      modelOutputKnob(p.apvts, ParamIDs::modelOutput.getParamID(), "OUTPUT"),
      distortionOverlay(p.apvts, ParamIDs::algoBypass.getParamID()),
      filterOverlay(p.apvts, ParamIDs::filterBypass.getParamID()),
      modelOverlay(p.apvts, ParamIDs::modelBypass.getParamID())
{
    setLookAndFeel(&rackLookAndFeel);

    addAndMakeVisible(presetBar);
    addAndMakeVisible(filterShapeDisplay);
    addAndMakeVisible(frequencyResponseDisplay);
    addAndMakeVisible(transferCurveDisplay);

    addAndMakeVisible(distortionSelector);
    addAndMakeVisible(distortionDriveKnob);
    addAndMakeVisible(distortionOutputKnob);
    addAndMakeVisible(distortionMixKnob);
    distortionSelector.onSelectionChanged = [this](int) { updateFrequencyResponseDisplay(); };

    addAndMakeVisible(filterComponent);

    modelSwitch.onSelectionChanged = [this](int) { updateFrequencyResponseDisplay(); };
    addAndMakeVisible(modelSwitch);
    addAndMakeVisible(modelDriveKnob);
    addAndMakeVisible(modelOutputKnob);

    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    // Bypass LEDs — lit = section IS bypassed (a common hardware convention: the
    // red light warns you that stage is being skipped). Direct ButtonAttachment
    // to the "...Bypass" parameters, which default to false/unlit (all three
    // sections active on load). This is the opposite convention from
    // filterEnableLed/airEnableLed below (lit = that control IS engaged) —
    // deliberate, since those two are "Enabled" parameters, not "Bypass" ones.
    distortionBypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, ParamIDs::algoBypass.getParamID(), distortionBypassLed);
    filterEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, ParamIDs::filterEnabled.getParamID(), filterEnableLed);
    airEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, ParamIDs::airEnabled.getParamID(), airEnableLed);
    filterSectionBypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, ParamIDs::filterBypass.getParamID(), filterSectionBypassLed);
    modelBypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, ParamIDs::modelBypass.getParamID(), modelBypassLed);

    addAndMakeVisible(distortionBypassLed);
    addAndMakeVisible(filterEnableLed);
    addAndMakeVisible(airEnableLed);
    addAndMakeVisible(filterSectionBypassLed);
    addAndMakeVisible(modelBypassLed);

    for (auto* btn : { &lowLatBtn, &highLatBtn })
    {
        btn->setRadioGroupId(44);
        btn->setClickingTogglesState(true);
        addAndMakeVisible(btn);
    }
    bool isHF = *p.apvts.getRawParameterValue(ParamIDs::qualityMode.getParamID()) > 0.5f;
    lowLatBtn .setToggleState(!isHF, juce::dontSendNotification);
    highLatBtn.setToggleState( isHF, juce::dontSendNotification);
    lowLatBtn.onClick = [this, &apvts = p.apvts] {
        if (auto* param = apvts.getParameter(ParamIDs::qualityMode.getParamID()))
            param->setValueNotifyingHost(0.0f);
        lowLatBtn .setToggleState(true,  juce::dontSendNotification);
        highLatBtn.setToggleState(false, juce::dontSendNotification);
    };
    highLatBtn.onClick = [this, &apvts = p.apvts] {
        if (auto* param = apvts.getParameter(ParamIDs::qualityMode.getParamID()))
            param->setValueNotifyingHost(1.0f);
        lowLatBtn .setToggleState(false, juce::dontSendNotification);
        highLatBtn.setToggleState(true,  juce::dontSendNotification);
    };

    distortionDriveKnob.getSlider().textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100)) + "%";
    };
    distortionMixKnob.getSlider().textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100)) + "%";
    };
    distortionOutputKnob.getSlider().textFromValueFunction = [](double v) {
        return (v >= 0.0 ? "+" : "") + juce::String(v, 1) + " dB";
    };
    modelOutputKnob.getSlider().textFromValueFunction = [](double v) {
        return (v >= 0.0 ? "+" : "") + juce::String(v, 1) + " dB";
    };
    modelDriveKnob.getSlider().textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v / ParamIDs::kModelDriveMax * 100)) + "%";
    };

    const float kStartAngle = -140.0f * juce::MathConstants<float>::pi / 180.0f;
    const float kEndAngle   =  140.0f * juce::MathConstants<float>::pi / 180.0f;
    for (auto* k : { &distortionDriveKnob, &distortionOutputKnob, &distortionMixKnob,
                     &modelDriveKnob, &modelOutputKnob })
        k->getSlider().setRotaryParameters(kStartAngle, kEndAngle, true);

    textureImage = juce::ImageCache::getFromMemory(
        BinaryData::small_texture_jpg, BinaryData::small_texture_jpgSize);

    updateFrequencyResponseDisplay();

    // Bypass overlays — added LAST so they paint on top of each section's own
    // controls; see BypassOverlay.h.
    addAndMakeVisible(distortionOverlay);
    addAndMakeVisible(filterOverlay);
    addAndMakeVisible(modelOverlay);

    startTimerHz(30);
    setSize(kTotalW, kTotalH);

    // setSize() above triggers the first resized(), so bounds/siblings are valid now.
    distortionOverlay.refreshFromParameter();
    filterOverlay.refreshFromParameter();
    modelOverlay.refreshFromParameter();
}

ExpandedSaturationEditor::~ExpandedSaturationEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void ExpandedSaturationEditor::updateFrequencyResponseDisplay()
{
    const int modelIndex = static_cast<int>(
        *pluginProcessor.apvts.getRawParameterValue(ParamIDs::modelSwitch.getParamID()));
    const LNLModel& model = pluginProcessor.getAudioEngine().getModelManager().getModel(modelIndex);
    frequencyResponseDisplay.setData(model.frFrequencies, model.frMagnitudeDb);
}

void ExpandedSaturationEditor::refreshMeters()
{
    auto& eng = pluginProcessor.getAudioEngine();
    inputMeter.pushLevels(eng.getInputPeakL(),   eng.getInputPeakR());
    outputMeter.pushLevels(eng.getOutputPeakL(), eng.getOutputPeakR());
}

// ── Static chassis helpers (ported from SaturationModel1) ──────────────────────

void ExpandedSaturationEditor::drawScrew(juce::Graphics& g, float cx, float cy)
{
    const float r = 5.5f;
    juce::ColourGradient grad(juce::Colour(0xffaaaaaa), cx - r * 0.22f, cy - r * 0.25f,
                              juce::Colour(0xff222222), cx + r * 0.7f, cy + r * 0.7f, true);
    grad.addColour(0.40, juce::Colour(0xff666666));
    g.setGradientFill(grad);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colour(0xff111111));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);
    g.setColour(juce::Colour(0xb3111111));
    g.drawLine(cx - 3.0f, cy, cx + 3.0f, cy, 1.0f);
    g.drawLine(cx, cy - 3.0f, cx, cy + 3.0f, 1.0f);
}

void ExpandedSaturationEditor::drawMountingHole(juce::Graphics& g, float cx, float cy)
{
    const float r1 = 7.0f, r2 = 4.0f;
    juce::ColourGradient grad(juce::Colour(0xff2a2a2a), cx - r1 * 0.1f, cy - r1 * 0.12f,
                              juce::Colour(0xff080808), cx + r1 * 0.7f, cy + r1 * 0.7f, true);
    g.setGradientFill(grad);
    g.fillEllipse(cx - r1, cy - r1, r1 * 2.0f, r1 * 2.0f);
    g.setColour(juce::Colour(0xff1a1a1a));
    g.drawEllipse(cx - r1, cy - r1, r1 * 2.0f, r1 * 2.0f, 1.0f);
    g.setColour(juce::Colour(0xff050505));
    g.fillEllipse(cx - r2, cy - r2, r2 * 2.0f, r2 * 2.0f);
    g.setColour(juce::Colour(0xff111111));
    g.drawEllipse(cx - r2, cy - r2, r2 * 2.0f, r2 * 2.0f, 0.5f);
}

void ExpandedSaturationEditor::drawSectionLabel(juce::Graphics& g, const juce::String& text, int x, int y, int w)
{
    g.setColour(juce::Colour(0xfff0f0f0));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
    g.drawText(text, x, y, w, 18, juce::Justification::centred, false);
}

// ── paint ────────────────────────────────────────────────────────────────────

void ExpandedSaturationEditor::paint(juce::Graphics& g)
{
    const float fW  = (float)kFaceW;
    const float fH  = (float)kTotalH;
    const float ear = (float)kEar;

    g.fillAll(juce::Colour(0xff0a0a0a));

    // ── Ears (both sides of the faceplate, full height) ───────────────────────
    for (float ex : { (float)kMeterW, (float)(kMeterW + kEar + kFaceW) })
    {
        juce::ColourGradient grad(juce::Colour(0xff363636), ex, 0.0f,
                                  juce::Colour(0xff363636), ex + ear, 0.0f, false);
        grad.addColour(0.15, juce::Colour(0xff555555));
        grad.addColour(0.40, juce::Colour(0xff404040));
        grad.addColour(0.60, juce::Colour(0xff484848));
        grad.addColour(0.85, juce::Colour(0xff505050));
        g.setGradientFill(grad);
        g.fillRect(ex, 0.0f, ear, fH);

        drawMountingHole(g, ex + ear * 0.5f, 31.0f);
        drawMountingHole(g, ex + ear * 0.5f, fH - 31.0f);
    }

    // ── Faceplate ──────────────────────────────────────────────────────────────
    {
        juce::ColourGradient faceGrad(juce::Colour(0xff1e1e1e), (float)kFaceX, 0.0f,
                                      juce::Colour(0xff111111), (float)kFaceX, fH, false);
        faceGrad.addColour(0.50, juce::Colour(0xff161616));
        g.setGradientFill(faceGrad);
        g.fillRect((float)kFaceX, 0.0f, fW, fH);

        if (textureImage.isValid())
        {
            int tw = juce::jmax(20, textureImage.getWidth()  * 2 / 5);
            int th = juce::jmax(20, textureImage.getHeight() * 2 / 5);
            auto tiledTex = textureImage.rescaled(tw, th, juce::Graphics::lowResamplingQuality);
            g.setTiledImageFill(tiledTex, kFaceX, 0, 0.18f);
            g.fillRect((float)kFaceX, 0.0f, fW, fH);
        }

        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRect((float)kFaceX, 0.0f, fW, fH, 1.0f);
    }

    // Corner screws framing the whole panel
    drawScrew(g, kFaceX + 14.0f,      14.0f);
    drawScrew(g, kFaceX + fW - 14.0f, 14.0f);
    drawScrew(g, kFaceX + 14.0f,      fH - 14.0f);
    drawScrew(g, kFaceX + fW - 14.0f, fH - 14.0f);

    // ── Column dividers (signal-flow separators) ──────────────────────────────
    auto drawDivider = [&](float x)
    {
        juce::ColourGradient divGrad(juce::Colours::transparentBlack, x, (float)kRackY,
                                     juce::Colours::transparentBlack, x, (float)(kRackY + kH), false);
        divGrad.addColour(0.20, juce::Colour(0xff3a5050));
        divGrad.addColour(0.80, juce::Colour(0xff3a5050));
        g.setGradientFill(divGrad);
        for (float dy = (float)kRackY + 16.0f; dy < (float)(kRackY + kH) - 16.0f; dy += 1.0f)
        {
            float t = (dy - ((float)kRackY + 16.0f)) / (float)(kH - 32);
            float alpha = (t < 0.2f) ? t / 0.2f : (t > 0.8f) ? (1.0f - t) / 0.2f : 1.0f;
            g.setColour(juce::Colour(0xff3a5050).withAlpha(alpha));
            g.drawHorizontalLine((int)dy, x, x + 1.0f);
        }
    };
    drawDivider((float)kDistX + kDistW + kColGap * 0.5f);
    drawDivider((float)kFiltX + kFiltW + kColGap * 0.5f);

    // ── Section labels — sit just above each section's own controls, in the
    // control strip (kRackY+), not up in the display row. FilterComponent draws
    // its own "FILTER" label internally at the same relative position, so it's
    // not repeated here. ─────────────────────────────────────────────────────
    drawSectionLabel(g, "DISTORTION", kDistX,  kRackY + 14, kDistW);
    drawSectionLabel(g, "MODELING",   kModelX, kRackY + 14, kModelW);

    // "BYP" captions beside each section's bypass LED, so it's clear what
    // that specific light does (vs. the Filter section's separate Filter/Air
    // enable LEDs, which already have their own knob labels).
    g.setColour(juce::Colour(0xff6a4a4a));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::bold));
    g.drawText("BYP", kDistX  + 18, kRackY + 12, 40, 16, juce::Justification::centredLeft, false);
    g.drawText("BYP", kFiltX  + 18, kRackY + 12, 40, 16, juce::Justification::centredLeft, false);
    g.drawText("BYP", kModelX + 18, kRackY + 12, 40, 16, juce::Justification::centredLeft, false);

    // "LATENCY" label directly above the stacked High/Low buttons
    g.setColour(juce::Colour(0xff4a6a6a));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    g.drawText("LATENCY", kModelBtnX - 10, kModelKnobY - 16, kModelBtnStripW + 20, 14,
               juce::Justification::centred, false);

    // ── Brand nameplate — fills the distortion column's otherwise-empty top slot
    // (Filter and Modeling each have a shape display there; Distortion doesn't). ─
    drawNameplate(g);
}

void ExpandedSaturationEditor::drawNameplate(juce::Graphics& g)
{
    const float npX = (float)kNameplateX;
    const float npY = (float)kTopSlotY;
    const float npW = (float)kTopSlotItemW;
    const float npH = (float)kTopSlotH;
    const int inpX = (int)npX, inpY = (int)npY, inpW = (int)npW, inpH = (int)npH;

    g.saveState();
    {
        juce::Path npClip;
        npClip.addRoundedRectangle(npX, npY, npW, npH, 6.0f);
        g.reduceClipRegion(npClip);

        juce::ColourGradient silver(
            juce::Colour(0xffd8d8d8), npX, npY,
            juce::Colour(0xff909090), npX, npY + npH, false);
        silver.addColour(0.22, juce::Colour(0xffc4c4c4));
        silver.addColour(0.50, juce::Colour(0xffababab));
        silver.addColour(0.72, juce::Colour(0xffb6b6b6));
        g.setGradientFill(silver);
        g.fillRect(npX, npY, npW, npH);

        juce::ColourGradient spec(
            juce::Colours::transparentWhite, npX, npY + npH * 0.10f,
            juce::Colours::transparentWhite, npX, npY + npH * 0.34f, false);
        spec.addColour(0.5, juce::Colours::white.withAlpha(0.14f));
        g.setGradientFill(spec);
        g.fillRect(npX, npY, npW, npH * 0.34f);

        g.setColour(juce::Colour(0xffe8e8e8));
        g.drawHorizontalLine(inpY,           npX + 1.0f, npX + npW - 1.0f);
        g.drawVerticalLine  (inpX,           npY + 1.0f, npY + npH - 1.0f);
        g.setColour(juce::Colour(0xff646464));
        g.drawHorizontalLine(inpY + inpH - 1, npX + 1.0f, npX + npW - 1.0f);
        g.drawVerticalLine  (inpX + inpW - 1, npY + 1.0f, npY + npH - 1.0f);
    }
    g.restoreState();

    g.setColour(juce::Colour(0xff828282));
    g.drawRoundedRectangle(npX + 0.5f, npY + 0.5f, npW - 1.0f, npH - 1.0f, 6.0f, 1.0f);

    drawScrew(g, npX + 12.0f,       npY + 12.0f);
    drawScrew(g, npX + npW - 12.0f, npY + 12.0f);
    drawScrew(g, npX + 12.0f,       npY + npH - 12.0f);
    drawScrew(g, npX + npW - 12.0f, npY + npH - 12.0f);

    auto drawLine = [&](const juce::String& text, int y, int h)
    {
        g.setColour(juce::Colour(0x72505050));
        g.drawText(text, inpX + 1, y + 1, inpW, h, juce::Justification::centred, false);
        g.setColour(juce::Colour(0x28505050));
        g.drawText(text, inpX + 2, y + 2, inpW, h, juce::Justification::centred, false);
        g.setColour(juce::Colour(0xff101010));
        g.drawText(text, inpX, y, inpW, h, juce::Justification::centred, false);
    };

    // "ANALOG DRIVE" — the plugin name: big, bold, wide, tightly tracked for a
    // chunky wordmark feel rather than the more spaced-out look elsewhere.
    juce::Font titleFont(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::bold);
    titleFont = titleFont.withHorizontalScale(1.15f).withExtraKerningFactor(0.02f);
    g.setFont(titleFont);
    const int titleLineH = 28;

    // "Saturation Sounds" — company byline, smaller, beneath the title
    const int subtitleH   = 14;
    const int subtitleGap = 6;

    const int blockH = titleLineH * 2 + subtitleGap + subtitleH;
    const int textY  = inpY + (inpH - blockH) / 2;

    drawLine("ANALOG", textY,               titleLineH);
    drawLine("DRIVE",  textY + titleLineH,  titleLineH);

    juce::Font subtitleFont(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain);
    subtitleFont = subtitleFont.withExtraKerningFactor(0.08f);
    g.setFont(subtitleFont);
    drawLine("Saturation Sounds", textY + titleLineH * 2 + subtitleGap, subtitleH);
}

// ── resized ──────────────────────────────────────────────────────────────────

void ExpandedSaturationEditor::resized()
{
    const int presetBarW = juce::jmin(300, kFaceW - 40);
    presetBar.setBounds(kFaceX + (kFaceW - presetBarW) / 2, kPresetBarY, presetBarW, kPresetBarH);

    inputMeter.setBounds (0,                        0, kMeterW, kTotalH);
    outputMeter.setBounds(kMeterW + kEar + kFaceW + kEar, 0, kMeterW, kTotalH);

    // ── Distortion column ──────────────────────────────────────────────────────
    distortionSelector.setBounds(kDistX, kRackY + 34, kDistW, 28);
    {
        const int knobY = kRackY + 74, knobH = 78, knobW = kDistW / 3;
        distortionDriveKnob .setBounds(kDistX,               knobY, knobW, knobH);
        distortionOutputKnob.setBounds(kDistX + knobW,       knobY, knobW, knobH);
        distortionMixKnob   .setBounds(kDistX + knobW * 2,   knobY, kDistW - knobW * 2, knobH);
    }
    distortionBypassLed.setBounds(kDistX, kRackY + 14, 14, 14);

    // ── Filter column (unchanged internal layout, calibrated for 196 width) ────
    filterComponent.setBounds(kFiltX, kRackY, kFiltW, kH);
    constexpr int ledSz = 16, ledY = 84;
    filterEnableLed.setBounds(kFiltX + 1,  kRackY + ledY, ledSz, ledSz);
    airEnableLed   .setBounds(kFiltX + 95, kRackY + ledY, ledSz, ledSz);
    filterSectionBypassLed.setBounds(kFiltX, kRackY + 14, 14, 14);

    // ── Modeling column ────────────────────────────────────────────────────────
    modelSwitch.setBounds(kModelX + kModelW / 2 - 90, kRackY + 34, 180, 44);
    {
        // Drive / [latency buttons] / Output, left to right — the two latency
        // buttons sit in the gap between the knobs, stacked vertically with
        // High on top and Low on the bottom.
        const int knobH = 72;  // kModelKnobY + 72 = kRackY+166, leaving a 6px bottom margin like the rest of the strip

        modelDriveKnob .setBounds(kModelX,                              kModelKnobY, kModelKnobW, knobH);
        modelOutputKnob.setBounds(kModelBtnX + kModelBtnStripW,         kModelKnobY,
                                  kModelW - kModelKnobW - kModelBtnStripW, knobH);

        // Buttons themselves are ~1/3 the size of the reserved strip (60x32 ->
        // 20x11), centered within it — the strip width itself stays put since
        // the knobs on either side are already sized against it.
        constexpr int btnW = 26, btnH = 15, btnGap = 6;
        const int btnX = kModelBtnX + (kModelBtnStripW - btnW) / 2;
        const int stackY = kModelKnobY + (knobH - (btnH * 2 + btnGap)) / 2;
        highLatBtn.setBounds(btnX, stackY,                   btnW, btnH);
        lowLatBtn .setBounds(btnX, stackY + btnH + btnGap,   btnW, btnH);
    }
    modelBypassLed.setBounds(kModelX, kRackY + 14, 14, 14);

    // ── Top-row displays, centered over their respective columns ───────────────
    filterShapeDisplay.setBounds(kFiltX, kDisplayTop, kFiltW, kDisplayH);
    frequencyResponseDisplay.setBounds(kModelX + (kModelW - 300) / 2, kDisplayTop,
                                        juce::jmin(300, kModelW), kDisplayH);

    // Distortion column's top slot: nameplate (drawn in paint()) on the left,
    // transfer-curve display on the right.
    transferCurveDisplay.setBounds(kCurveX, kTopSlotY, kTopSlotItemW, kTopSlotH);

    // ── Bypass overlays — cover each section's controls, but NOT its label/LED
    // row (kRackY..kRackY+32), so the bypass LED stays visible/clickable to
    // un-bypass even while its own section is darkened. ───────────────────────
    const int overlayY = kRackY + 32, overlayH = kH - 32;
    distortionOverlay.setBounds(kDistX,  overlayY, kDistW,  overlayH);
    filterOverlay.setBounds    (kFiltX,  overlayY, kFiltW,  overlayH);
    modelOverlay.setBounds     (kModelX, overlayY, kModelW, overlayH);
}
