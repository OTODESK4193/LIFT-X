// ==========================================
// File: ArcDial.h
// アークダイアル LookAndFeel (Granular由来・ライトテーマ調整版)
// ==========================================
#pragma once

#include <JuceHeader.h>

// ============================================================================
//  埋め込みフォント
//   ・UI全般        : Inter          (Regular / Bold)
//   ・数値表示      : JetBrains Mono (Regular / Bold)
//  どちらも SIL OFL 1.1。Source/Assets/Fonts/ に実体、ライセンスも同梱。
//
//  UI全般の差し替えは LookAndFeel::getTypefaceForFont() で行うため、
//  既存の juce::FontOptions(size, bold) という呼び出しは1箇所も変えずに
//  Inter へ切り替わる。等幅にしたい箇所だけ LiftFonts::mono() を使う。
// ============================================================================
namespace LiftFonts
{
    // getTypefaceForFont() がこの名前を見て JetBrains Mono を返す
    inline const char* kMonoName = "LIFTX Mono";

    inline juce::Font mono(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(kMonoName, height,
                                            bold ? juce::Font::bold : juce::Font::plain));
    }
}

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
