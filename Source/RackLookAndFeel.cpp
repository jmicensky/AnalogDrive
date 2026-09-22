#include "RackLookAndFeel.h"

namespace {

const juce::Colour kValueBg    (0xff1a2626);
const juce::Colour kValueText  (0xff5eaaaa);
const juce::Colour kLabelDim   (0xffc8dada);
const juce::Colour kTeal       (0xff3a8888);
const juce::Colour kFilterBtn  (0xff1e2e2e);
const juce::Colour kFilterBtnA (0xff2a4a4a);
const juce::Colour kFilterBtnBorder  (0xff3a5050);
const juce::Colour kFilterBtnABorder (0xff44aaaa);

}  // namespace

RackLookAndFeel::RackLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,       kValueText);
    setColour(juce::Slider::textBoxBackgroundColourId, kValueBg);
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xff2a4040));
    setColour(juce::Slider::textBoxHighlightColourId,  kTeal.withAlpha(0.4f));
    setColour(juce::Slider::thumbColourId,             juce::Colour(0xff3a8888));

    setColour(juce::Label::textColourId,               juce::Colour(0xfff0f0f0));

    // ComboBox: bright red-on-black digital display style
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff000000));
    setColour(juce::ComboBox::textColourId,       juce::Colour(0xffff3300));
    setColour(juce::ComboBox::outlineColourId,    juce::Colour(0xff4a0000));
    setColour(juce::ComboBox::arrowColourId,      juce::Colour(0xffcc2200));

    setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff080808));
    setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffff3300));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2a0000));
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xffff6644));
}

void RackLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                       int x, int y, int width, int height,
                                       float sliderPos,
                                       float rotaryStartAngle, float rotaryEndAngle,
                                       juce::Slider& slider)
{
    const float cx = x + width  * 0.5f;
    const float cy = y + height * 0.5f;

    const float diameter = juce::jmin((float)width, (float)height) * 0.86f;
    const float skirtR   = diameter * 0.5f;
    const float domeR    = skirtR * 0.826f;   // matches JSX ratio: 0.38/0.46
    const float arcR     = domeR + skirtR * 0.225f; // proportional to JSX "+6 px" in 58-px context

    const float currentAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // ── Skirt ─────────────────────────────────────────────────────────────────
    {
        juce::ColourGradient grad(juce::Colour(0xff2c2c2c), cx, cy,
                                  juce::Colour(0xff131313), cx + skirtR, cy + skirtR, true);
        g.setGradientFill(grad);
        g.fillEllipse(cx - skirtR, cy - skirtR, skirtR * 2.0f, skirtR * 2.0f);
    }

    // ── Full travel track arc (dark grey) ─────────────────────────────────────
    {
        juce::Path track;
        track.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                     rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff242424));
        g.strokePath(track, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // ── Active arc (teal) ─────────────────────────────────────────────────────
    if (sliderPos > 0.002f)
    {
        juce::Path active;
        active.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                      rotaryStartAngle, currentAngle, true);
        g.setColour(juce::Colour(0xff3a8888));
        g.strokePath(active, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // ── Dome (dark sphere with offset highlight) ──────────────────────────────
    {
        float hlCx = cx - domeR * 0.14f;
        float hlCy = cy - domeR * 0.22f;
        juce::ColourGradient grad(juce::Colour(0xff606060), hlCx, hlCy,
                                  juce::Colour(0xff060606), cx + domeR * 0.7f, cy + domeR * 0.7f, true);
        grad.addColour(0.28, juce::Colour(0xff2a2a2a));
        grad.addColour(0.68, juce::Colour(0xff111111));
        g.setGradientFill(grad);
        g.fillEllipse(cx - domeR, cy - domeR, domeR * 2.0f, domeR * 2.0f);
    }

    // ── Specular highlight ────────────────────────────────────────────────────
    {
        float specR  = domeR * 0.40f;
        float specCx = cx - domeR * 0.14f;
        float specCy = cy - domeR * 0.22f;
        juce::ColourGradient spec(juce::Colours::white.withAlpha(0.22f), specCx, specCy,
                                  juce::Colours::white.withAlpha(0.0f),
                                  specCx + specR, specCy + specR, true);
        g.setGradientFill(spec);
        g.fillEllipse(specCx - specR, specCy - specR, specR * 2.0f, specR * 2.0f);
    }

    // ── Indicator line ────────────────────────────────────────────────────────
    // JUCE rotary convention: 0 = 12 o'clock, clockwise positive
    // x = cx + r*sin(angle),  y = cy - r*cos(angle)
    {
        float indLen  = domeR * 0.72f;
        float innerR  = domeR * 0.18f;
        float endX   = cx + std::sin(currentAngle) * indLen;
        float endY   = cy - std::cos(currentAngle) * indLen;
        float startX = cx + std::sin(currentAngle) * innerR;
        float startY = cy - std::cos(currentAngle) * innerR;

        g.setColour(juce::Colour(0xccffffff));
        g.drawLine(startX, startY, endX, endY, 1.6f);
    }

    // ── Scale tick marks (API/SSL style) ─────────────────────────────────────
    {
        constexpr int   numTicks  = 11;   // marks at 0%, 10%, 20% … 100%
        const float tickInner = arcR + 3.0f;
        const float majLen    = 8.0f;
        const float minLen    = 4.0f;
        const float labelRad  = tickInner + majLen + 6.0f;

        for (int i = 0; i < numTicks; ++i)
        {
            float t     = (float)i / (float)(numTicks - 1);
            float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            bool  major = (i == 0 || i == (numTicks - 1) / 2 || i == numTicks - 1);

            float sinA = std::sin(angle), cosA = std::cos(angle);
            float r2   = tickInner + (major ? majLen : minLen);

            g.setColour(major ? juce::Colour(0xddffffff) : juce::Colour(0x66cccccc));
            g.drawLine(cx + tickInner * sinA, cy - tickInner * cosA,
                       cx + r2        * sinA, cy - r2        * cosA,
                       major ? 1.2f : 0.7f);
        }

        // Compact value labels at start (0%), centre (50%), end (100%)
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 6.5f, juce::Font::plain));
        g.setColour(juce::Colour(0x99dddddd));

        for (int i : { 0, (numTicks - 1) / 2, numTicks - 1 })
        {
            float t     = (float)i / (float)(numTicks - 1);
            float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            float lx    = cx + labelRad * std::sin(angle);
            float ly    = cy - labelRad * std::cos(angle);

            double v  = slider.proportionOfLengthToValue((double)t);
            auto   txt = slider.getTextFromValue(v)
                                .upToFirstOccurrenceOf(" ", false, false);

            g.drawText(txt,
                       juce::roundToInt(lx) - 14,
                       juce::roundToInt(ly) - 5,
                       28, 10,
                       juce::Justification::centred, false);
        }
    }
}

juce::Font RackLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::bold);
}

void RackLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                   bool /*isButtonDown*/,
                                   int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/,
                                   juce::ComboBox& /*box*/)
{
    // Black base — digital display
    g.setColour(juce::Colour(0xff000000));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 2.0f);

    // Faint red inner glow at top edge (CRT warmth)
    juce::ColourGradient glow(juce::Colour(0x22ee2200), 0.0f, 0.0f,
                              juce::Colours::transparentBlack, 0.0f, (float)height * 0.5f, false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(1.0f, 1.0f, (float)width - 2.0f, (float)height * 0.5f, 2.0f);

    // Dark red border
    g.setColour(juce::Colour(0xff3a0000));
    g.drawRoundedRectangle(0.5f, 0.5f, width - 1.0f, height - 1.0f, 2.0f, 0.8f);

    // Drop arrow in red
    float ax = width - 14.0f, ay = height * 0.5f - 3.0f;
    juce::Path arrow;
    arrow.addTriangle(ax, ay, ax + 8.0f, ay, ax + 4.0f, ay + 6.0f);
    g.setColour(juce::Colour(0xffaa1800));
    g.fillPath(arrow);
}

juce::Font RackLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold);
}

void RackLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 0, box.getWidth() - 26, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

void RackLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& /*bg*/,
                                           bool isHighlighted, bool /*isDown*/)
{
    bool active = button.getToggleState();
    auto b = button.getLocalBounds().toFloat();

    if (active)
    {
        // Depressed key: shadow on top-left, light strip on bottom-right
        g.setColour(juce::Colour(0xff706858));
        g.fillRoundedRectangle(b, 3.0f);

        g.setColour(juce::Colour(0xffb8b4a0));
        g.fillRoundedRectangle(b.getX() + 2.0f, b.getY() + 2.0f,
                               b.getWidth() - 2.0f, b.getHeight() - 2.0f, 2.5f);

        // Depressed face (darker cream, nudged down-right)
        g.setColour(juce::Colour(0xffd4d0bc));
        g.fillRoundedRectangle(b.getX() + 2.0f, b.getY() + 2.0f,
                               b.getWidth() - 4.0f, b.getHeight() - 4.0f, 2.0f);
    }
    else
    {
        // Raised key: shadow on bottom-right, highlight on top-left, cream face
        g.setColour(juce::Colour(0xff706858));
        g.fillRoundedRectangle(b, 3.0f);

        g.setColour(isHighlighted ? juce::Colour(0xfffdfbf0) : juce::Colour(0xfff8f4e6));
        g.fillRoundedRectangle(b.getX(), b.getY(),
                               b.getWidth() - 3.0f, b.getHeight() - 3.0f, 3.0f);

        // Cream face (slightly inset from highlight strip)
        g.setColour(isHighlighted ? juce::Colour(0xfff5f1e0) : juce::Colour(0xfff0ece0));
        g.fillRoundedRectangle(b.getX() + 1.0f, b.getY() + 1.0f,
                               b.getWidth() - 4.0f, b.getHeight() - 4.0f, 2.5f);
    }
}

void RackLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                     bool /*isHighlighted*/, bool /*isDown*/)
{
    bool active = button.getToggleState();
    g.setColour(active ? juce::Colour(0xff1a1810) : juce::Colour(0xff2e2a1e));
    g.setFont(getTextButtonFont(button, button.getHeight()));
    auto tb = button.getLocalBounds();
    if (active) tb = tb.translated(1, 1);
    g.drawText(button.getButtonText(), tb, juce::Justification::centred, false);
}

juce::Font RackLookAndFeel::getTextButtonFont(juce::TextButton&, int /*height*/)
{
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::plain);
}
