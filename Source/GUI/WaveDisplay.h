// ==========================================
// File: WaveDisplay.h
// OSC毎の波形表示 (ビルトイン/カスタムWTの現在ポジションの波形)
//  Golden Rule: paint内のPath再確保なし (メンバ再利用)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>

#include "ColorPalette.h"

class WaveDisplay : public juce::Component
{
public:
    static constexpr int kN = 128;

    WaveDisplay()
    {
        path.preallocateSpace(kN * 3 + 8);
        setInterceptsMouseClicks(false, false);
    }

    // 波形を kN ポイントへ書き出す関数を登録 (メッセージスレッドで呼ばれる)
    void setSource(std::function<void(float*, int)> fn)
    {
        source = std::move(fn);
        refresh();
    }

    void setAccent(juce::Colour c) { accent = c; repaint(); }

    void refresh()
    {
        if (source != nullptr)
        {
            source(buf.data(), kN);
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        LiftColors::paintWell(g, r, 5.0f);

        // 中央線
        g.setColour(LiftColors::grid);
        g.drawHorizontalLine((int)r.getCentreY(), r.getX() + 3.0f, r.getRight() - 3.0f);

        // 波形
        const auto a = r.reduced(4.0f, 5.0f);
        path.clear();
        for (int i = 0; i < kN; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float)i / (float)(kN - 1);
            const float y = a.getCentreY() - juce::jlimit(-1.0f, 1.0f, buf[(size_t)i]) * a.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }
        g.setColour(accent);
        g.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

private:
    std::array<float, kN> buf {};
    std::function<void(float*, int)> source;
    juce::Colour accent { 0xffb5ead7 };
    juce::Path path;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveDisplay)
};
