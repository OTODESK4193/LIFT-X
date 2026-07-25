// ==========================================
// File: ModBand.h
// マルチENVの変化幅をノブへ動的表示するためのヘルパー
//  (Granular ModMatrix の mod_active/mod_min/mod_max/mod_live 方式)
//
//  カーブ全域の min/max と現在Progress位置の値を実パラメーター値へ変換し、
//  ノブのプロパティへ 0..1 正規化で書き込む。ArcDialLookAndFeel が
//  アーク色より濃い帯 + 白いライブドットとして描画する。
//  値が変わったときだけ repaint するため30Hz更新でも軽量。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>

#include "../DSP/CurveData.h"

namespace ModBand
{
    // apply(base, bip): bip(-1..1, LIFT適用済み) から変調後の実パラメーター値を返す。
    //  加算式:   [](float b, float bip){ return b + bip * halfRange; }
    //  対数式:   [](float b, float bip){ return b * std::exp2(bip * oct); }
    //  ※ apply は bip に対して単調であること (min/max マッピングの前提)
    template <typename ApplyFn>
    inline void update(juce::Slider& knob, juce::RangedAudioParameter* param,
                       const CurveSnapshot& curve, float lift, float progress,
                       ApplyFn&& apply)
    {
        if (param == nullptr) return;

        float yMin = 1.0f, yMax = 0.0f;
        for (int i = 0; i <= 32; ++i)
        {
            const float y = curve.evaluate((float)i / 32.0f);
            yMin = juce::jmin(yMin, y);
            yMax = juce::jmax(yMax, y);
        }

        // 常時表示: 帯(変化幅)とライブドット(現在値)を常にノブへ表示する
        auto& props = knob.getProperties();
        const float base = param->convertFrom0to1(param->getValue());
        auto bipOf = [lift](float y) noexcept { return (y - 0.5f) * 2.0f * lift; };

        // レンジ外はクランプしてから正規化 (skew付きレンジでのNaN防止)
        const auto& rng = param->getNormalisableRange();
        auto norm = [&rng](float v) { return rng.convertTo0to1(juce::jlimit(rng.start, rng.end, v)); };

        const float v0 = norm(apply(base, bipOf(yMin)));
        const float v1 = norm(apply(base, bipOf(yMax)));
        const float live = norm(apply(base, bipOf(curve.evaluate(progress))));

        const float nMin = juce::jmin(v0, v1);
        const float nMax = juce::jmax(v0, v1);

        // 変化があったフレームのみ再描画
        const float oMin = (float)props.getWithDefault("mod_min", -1.0f);
        const float oMax = (float)props.getWithDefault("mod_max", -1.0f);
        const float oLive = (float)props.getWithDefault("mod_live", -1.0f);
        const bool wasActive = (bool)props.getWithDefault("mod_active", false);

        if (!wasActive
            || std::abs(nMin - oMin) > 0.003f
            || std::abs(nMax - oMax) > 0.003f
            || std::abs(live - oLive) > 0.003f)
        {
            props.set("mod_active", true);
            props.set("mod_min", nMin);
            props.set("mod_max", nMax);
            props.set("mod_live", live);
            knob.repaint();
        }
    }

    inline void clear(juce::Slider& knob)
    {
        auto& props = knob.getProperties();
        if ((bool)props.getWithDefault("mod_active", false))
        {
            props.set("mod_active", false);
            knob.repaint();
        }
    }
}
