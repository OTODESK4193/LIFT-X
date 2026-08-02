// ==========================================
// File: ArcDial.cpp
// ==========================================
#include "ArcDial.h"
#include "ColorPalette.h"
#include "BinaryFonts.h"
#include <cmath>

juce::Typeface::Ptr ArcDialLookAndFeel::getTypefaceForFont(const juce::Font& f)
{
    const bool bold = f.isBold();

    // 等幅を明示的に要求している箇所 (LiftFonts::mono) だけ JetBrains Mono
    if (f.getTypefaceName() == LiftFonts::kMonoName)
        return bold ? monoBold : monoRegular;

    return bold ? interBold : interRegular;
}

ArcDialLookAndFeel::ArcDialLookAndFeel()
{
    // 埋め込みフォントの読み込み (エディタ生成時に1回だけ)
    interRegular = juce::Typeface::createSystemTypefaceFor(
        BinaryFonts::InterRegular_ttf, BinaryFonts::InterRegular_ttfSize);
    interBold = juce::Typeface::createSystemTypefaceFor(
        BinaryFonts::InterBold_ttf, BinaryFonts::InterBold_ttfSize);
    monoRegular = juce::Typeface::createSystemTypefaceFor(
        BinaryFonts::JetBrainsMonoRegular_ttf, BinaryFonts::JetBrainsMonoRegular_ttfSize);
    monoBold = juce::Typeface::createSystemTypefaceFor(
        BinaryFonts::JetBrainsMonoBold_ttf, BinaryFonts::JetBrainsMonoBold_ttfSize);

    setColour(juce::Slider::textBoxTextColourId, LiftColors::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, LiftColors::panel);
    setColour(juce::ComboBox::textColourId, LiftColors::text);
    setColour(juce::ComboBox::outlineColourId, LiftColors::panelLine);
    setColour(juce::ComboBox::arrowColourId, LiftColors::textDim);
    setColour(juce::PopupMenu::backgroundColourId, LiftColors::panel);
    setColour(juce::PopupMenu::textColourId, LiftColors::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, LiftColors::lavender.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, LiftColors::text);
    setColour(juce::ToggleButton::textColourId, LiftColors::text);
    setColour(juce::ToggleButton::tickColourId, LiftColors::mint);
    setColour(juce::ToggleButton::tickDisabledColourId, LiftColors::textDim);
    setColour(juce::Label::textColourId, LiftColors::textDim);
    setColour(juce::TextButton::buttonColourId, LiftColors::knobTrack);
    setColour(juce::TextButton::textColourOffId, LiftColors::text);
    setColour(juce::TextButton::textColourOnId, LiftColors::text);
}

void ArcDialLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
    const auto centreX = (float)x + (float)width * 0.5f;
    const auto centreY = (float)y + (float)height * 0.5f;
    const auto rx = centreX - radius;
    const auto ry = centreY - radius;
    const auto rw = radius * 2.0f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto arcThickness = 5.0f;

    // 1. 背景トラック (内側に落ち影を1px入れて彫り込み感を出す)
    g.setColour(LiftColors::bg.darker(0.35f));
    g.drawEllipse(rx, ry + 1.0f, rw, rw, arcThickness);
    g.setColour(LiftColors::knobTrack);
    g.drawEllipse(rx, ry, rw, rw, arcThickness);

    const auto& props = slider.getProperties();

    // アーク色: "accentId" プロパティがあれば現在テーマから解決 (テーマ連動)
    const int accentId = (int)props.getWithDefault("accentId", -1);
    const auto baseColour = accentId >= 0
        ? LiftColors::accentById(accentId)
        : slider.findColour(juce::Slider::rotarySliderFillColourId);

    // 1.5 マルチENVの変調レンジ帯 (Granular ModMatrix方式)
    //     GUI側が mod_active / mod_min / mod_max / mod_live を毎フレーム更新する。
    const bool modActive = props.getWithDefault("mod_active", false);
    if (modActive)
    {
        const float mMin = juce::jlimit(0.0f, 1.0f, (float)props.getWithDefault("mod_min", 0.0f));
        const float mMax = juce::jlimit(0.0f, 1.0f, (float)props.getWithDefault("mod_max", 1.0f));
        const auto aLo = rotaryStartAngle + juce::jmin(mMin, mMax) * (rotaryEndAngle - rotaryStartAngle);
        const auto aHi = rotaryStartAngle + juce::jmax(mMin, mMax) * (rotaryEndAngle - rotaryStartAngle);

        if (std::abs(aHi - aLo) > 0.001f)
        {
            // 変化幅の帯。以前は juce::Colours::white 固定だったため、
            // Sakura / Amber / Arctic など明度の高いテーマで浮いていた。
            // テーマの text 色ベースにして馴染ませる。
            juce::Path band;
            band.addArc(rx, ry, rw, rw, aLo, aHi, true);
            g.setColour(LiftColors::text.withAlpha(0.50f));
            g.strokePath(band, juce::PathStrokeType(arcThickness + 4.0f,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 2. 値アーク (セクション色のグラデーション)
    juce::Path p;
    p.addArc(rx, ry, rw, rw, rotaryStartAngle, angle, true);

    const auto lightColour = baseColour.brighter(0.6f);
    const auto darkColour = baseColour.darker(0.35f);

    juce::ColourGradient gradient(darkColour, rx, centreY, lightColour, rx + rw, centreY, false);
    g.setGradientFill(gradient);
    g.strokePath(p, juce::PathStrokeType(arcThickness, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    // 3. ソフトグロー
    g.setColour(baseColour.withAlpha(0.15f));
    g.strokePath(p, juce::PathStrokeType(arcThickness + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 4. ポインター
    juce::Path p2;
    const auto pointerLength = radius * 0.4f;
    p2.addRoundedRectangle(-1.5f, -radius + 1.5f, 3.0f, pointerLength, 1.5f);
    p2.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(LiftColors::text);
    g.fillPath(p2);

    // 5. ライブ変調ドット (変調適用後の現在値をアーク上に表示)
    if (modActive)
    {
        const float live = juce::jlimit(0.0f, 1.0f, (float)props.getWithDefault("mod_live", sliderPos));
        const auto aLive = rotaryStartAngle + live * (rotaryEndAngle - rotaryStartAngle);
        const float dotX = centreX + std::sin(aLive) * radius;
        const float dotY = centreY - std::cos(aLive) * radius;
        g.setColour(LiftColors::text.withAlpha(0.28f));
        g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
        g.setColour(LiftColors::text);
        g.fillEllipse(dotX - 2.6f, dotY - 2.6f, 5.2f, 5.2f);
    }
}
