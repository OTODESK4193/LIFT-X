// ==========================================
// File: ZdfFilter.h
// TPT (Topology-Preserving Transform) / ZDF ステートバリアブルフィルター
//  Andy Simper (Cytomic) の trapezoidal SVF 実装。
//  1サンプル遅延フィードバックによる不安定性が無く、
//  カーブによる高速なカットオフスイープでも破綻しない。
//  SR 44.1-192kHz 対応 (係数は毎コントロールティックで再計算)。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>

class TptSvf
{
public:
    enum Type { LowPass = 0, HighPass, BandPass, Notch };

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
        setCoef(1000.0f, 0.9f);
    }

    void reset() noexcept
    {
        ic1L = ic2L = ic1R = ic2R = 0.0f;
    }

    void setType(int t) noexcept { type = juce::jlimit(0, 3, t); }

    // ------------------------------------------------------------------
    //  係数セット。
    //  LIFT-X はフィルター1系統につき OSC1-3/Noise の4基を「同じ cutoff/res」で
    //  動かすため、各基で setCoef() を呼ぶと std::tan が4回とも同じ引数で
    //  計算されてしまう (4系統×4ソース = 16回/ティック)。
    //  computeCoefs() で1回だけ求めて setCoefs() で配る運用にすることで、
    //  tan の呼び出しが 1/4 になる。
    // ------------------------------------------------------------------
    struct Coefs
    {
        float k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    };

    // cutoffHz: 20..0.45*sr / res: 0.5..12 (Q)
    static Coefs computeCoefs(float cutoffHz, float res, double sampleRate) noexcept
    {
        const float maxHz = (float)(sampleRate * 0.45);
        cutoffHz = juce::jlimit(20.0f, maxHz, cutoffHz);
        const float g = std::tan(juce::MathConstants<float>::pi * cutoffHz / (float)sampleRate);

        Coefs c;
        c.k  = 1.0f / juce::jlimit(0.5f, 12.0f, res);
        c.a1 = 1.0f / (1.0f + g * (g + c.k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        return c;
    }

    void setCoefs(const Coefs& c) noexcept { k = c.k; a1 = c.a1; a2 = c.a2; a3 = c.a3; }

    // 単体で使う場合 (ノイズフィルター等)
    void setCoef(float cutoffHz, float res) noexcept
    {
        setCoefs(computeCoefs(cutoffHz, res, sr));
    }

    inline void processStereo(float& l, float& r) noexcept
    {
        l = tick(l, ic1L, ic2L);
        r = tick(r, ic1R, ic2R);
    }

private:
    inline float tick(float v0, float& ic1, float& ic2) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;   // band
        const float v2 = ic2 + a2 * ic1 + a3 * v3; // low
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        // 非正規化数対策
        if (std::abs(ic1) < 1.0e-20f) ic1 = 0.0f;
        if (std::abs(ic2) < 1.0e-20f) ic2 = 0.0f;

        switch (type)
        {
        case LowPass:  return v2;
        case HighPass: return v0 - k * v1 - v2;
        case BandPass: return v1;
        case Notch:    return v0 - k * v1;
        default:       return v2;
        }
    }

    double sr = 44100.0;
    int type = LowPass;
    float k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1L = 0.0f, ic2L = 0.0f, ic1R = 0.0f, ic2R = 0.0f;
};
