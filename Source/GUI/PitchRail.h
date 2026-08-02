// ==========================================
// File: PitchRail.h
// OSC毎の Pitch ENV ライブ表示 (v0.4)
//
//  Pitch ENV は「絶対音程」を扱うため、他のパラメーターのような
//  ノブ帯 (ModBand) 方式が使えない。そこで専用の横バーで可視化する。
//
//   ├─ 左端 = Start Key / 右端 = End Key (上昇/下降どちらでも左→右が進行方向)
//   ├─ 現在ピッチまでを塗りつぶし + ライブドット
//   ├─ 中央に現在の音名をリアルタイム表示 (例 "E3")
//   └─ Scaleクオンタイズ有効時は、区間内のスケール構成音位置に目盛りを描画
//      → 「今どの音を踏んでいるか」「次にどこへ跳ぶか」が一目で分かる
//
//  更新は VBlankAttachment (画面リフレッシュ同期)。値が動いたときだけ repaint。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>

#include "../PluginProcessor.h"
#include "../DSP/ScaleQuantizer.h"
#include "ColorPalette.h"

class PitchRail : public juce::Component
{
public:
    PitchRail(LiftXAudioProcessor& p, int oscIndex)
        : proc(p), osc(juce::jlimit(0, RiserEngine::kNumOscs - 1, oscIndex)),
          vblank(this, [this](double) { poll(); })
    {
        setOpaque(false);
        setInterceptsMouseClicks(false, false);
        prefix = "osc" + juce::String(osc + 1);
    }

    void paint(juce::Graphics& g) override
    {
        const auto rF = getLocalBounds().toFloat().reduced(0.5f);

        // 背景トラック
        g.setColour(LiftColors::knobTrack);
        g.fillRoundedRectangle(rF, 4.0f);

        const int ks = lastStart, ke = lastEnd;
        const float span = (float)(ke - ks);
        const auto inner = rF.reduced(3.0f, 3.0f);

        // ---- スケール構成音の目盛り ----
        if (lastQuant && std::abs(span) >= 1.0f)
        {
            const int lo = juce::jmin(ks, ke);
            const int hi = juce::jmax(ks, ke);
            // 半音間隔が細かすぎるときは目盛りを省略 (視認性優先)
            const float pxPerSemi = inner.getWidth() / juce::jmax(1.0f, (float)(hi - lo));
            if (pxPerSemi >= 1.6f)
            {
                g.setColour(LiftColors::textDim.withAlpha(0.55f));
                for (int n = lo; n <= hi; ++n)
                {
                    if (std::abs(ScaleQuantizer::quantize((float)n, lastKey, lastScale) - (float)n) > 0.01f)
                        continue;   // スケール外
                    const float t = juce::jlimit(0.0f, 1.0f, ((float)n - (float)ks) / span);
                    const float x = inner.getX() + inner.getWidth() * t;
                    g.fillRect(x - 0.5f, inner.getY(), 1.0f, inner.getHeight());
                }
            }
        }

        // ---- 進行バー ----
        const float t = (std::abs(span) < 0.001f)
                      ? 0.0f
                      : juce::jlimit(0.0f, 1.0f, (lastPitch - (float)ks) / span);
        if (t > 0.001f)
        {
            auto fill = inner.withWidth(inner.getWidth() * t);
            g.setColour(LiftColors::accentOsc.withAlpha(0.32f));
            g.fillRoundedRectangle(fill, 3.0f);
        }

        // ---- ライブドット ----
        const float dotX = inner.getX() + inner.getWidth() * t;
        const float cy = inner.getCentreY();
        g.setColour(LiftColors::accentOsc);
        g.fillEllipse(dotX - 2.5f, cy - 2.5f, 5.0f, 5.0f);

        // ---- ラベル: 左に "PITCH" / 中央に現在音名 ----
        g.setFont(LiftFonts::mono(10.0f, true));
        g.setColour(LiftColors::textDim);
        g.drawText("PITCH", getLocalBounds().withTrimmedLeft(5), juce::Justification::centredLeft, false);

        g.setColour(LiftColors::text);
        g.drawText(noteText, getLocalBounds(), juce::Justification::centred, false);
    }

private:
    void poll()
    {
        auto* pStart = proc.apvts.getRawParameterValue(prefix + "KeyStart");
        auto* pEnd   = proc.apvts.getRawParameterValue(prefix + "KeyEnd");
        auto* pOscSc = proc.apvts.getRawParameterValue(prefix + "Scale");
        auto* pOn    = proc.apvts.getRawParameterValue("scaleOn");
        auto* pKey   = proc.apvts.getRawParameterValue("scaleKey");
        auto* pType  = proc.apvts.getRawParameterValue("scaleType");
        if (pStart == nullptr || pEnd == nullptr) return;

        const int   ks    = (int)pStart->load();
        const int   ke    = (int)pEnd->load();
        const bool  quant = (pOn != nullptr && pOn->load() > 0.5f)
                         && (pOscSc != nullptr && pOscSc->load() > 0.5f);
        const int   key   = pKey != nullptr ? (int)pKey->load() : 0;
        const int   scl   = pType != nullptr ? (int)pType->load() : 0;
        const float pitch = proc.getUiPitch(osc);

        const bool changed = ks != lastStart || ke != lastEnd
                          || quant != lastQuant || key != lastKey || scl != lastScale
                          || std::abs(pitch - lastPitch) > 0.02f;
        if (!changed) return;

        lastStart = ks; lastEnd = ke;
        lastQuant = quant; lastKey = key; lastScale = scl;
        lastPitch = pitch;

        // 音名 + セント (量子化中はぴったり整数になるのでセント表記は出さない)
        const int   nearest = juce::jlimit(0, 127, (int)std::lround(pitch));
        const float cents   = (pitch - (float)nearest) * 100.0f;
        juce::String txt = juce::MidiMessage::getMidiNoteName(nearest, true, true, 3);
        if (!quant && std::abs(cents) >= 5.0f)
            txt << (cents > 0.0f ? " +" : " ") << juce::String((int)std::lround(cents));
        noteText = txt;

        repaint();
    }

    LiftXAudioProcessor& proc;
    int osc = 0;
    juce::String prefix;

    int   lastStart = -1, lastEnd = -1, lastKey = -1, lastScale = -1;
    bool  lastQuant = false;
    float lastPitch = -999.0f;
    juce::String noteText { "-" };

    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchRail)
};
