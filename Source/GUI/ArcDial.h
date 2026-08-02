// ==========================================
// File: ArcDial.h
// アークダイアル LookAndFeel (Granular由来・ライトテーマ調整版)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"   // LiftFonts (埋め込みフォントのヘルパー)

class ArcDialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ArcDialLookAndFeel();

    // 埋め込みフォントの解決 (太字/等幅の指定で振り分ける)
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

private:
    juce::Typeface::Ptr interRegular, interBold, monoRegular, monoBold;
};
