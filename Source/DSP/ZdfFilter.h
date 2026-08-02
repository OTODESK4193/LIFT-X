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
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

class TptSvf
{
public:
    // 0-3 は SVF 本体。4=Vowel / 5=Comb は別クラスで処理する
    // (RiserEngine が type を見て振り分ける)
    enum Type { LowPass = 0, HighPass, BandPass, Notch, Vowel, Comb };

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

// ==========================================================
//  VowelFilter — フォルマント (母音) フィルター
//
//  3本のバンドパスを人間の母音フォルマント周波数へ配置し、
//  CUTOFF ノブで A→E→I→O→U をモーフさせる。
//  Pitch ENV ではなく Filter ENV で母音が動くため、
//  「あ→え→い」と変化していく声のようなライザーが作れる。
//
//  RES はフォルマントの鋭さ (Q)。高いほど「喋っている」感じが強くなる。
// ==========================================================
class VowelFilter
{
public:
    // 男声の代表的なフォルマント (F1, F2, F3) と各バンドの振幅
    static constexpr int kNumVowels = 5;   // A E I O U
    static constexpr int kNumBands  = 3;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& b : bands) { b.prepare(sr); b.setType(TptSvf::BandPass); }
        reset();
    }

    void reset() noexcept { for (auto& b : bands) b.reset(); }

    // pos01: 0=A .. 1=U へ連続モーフ / res: 0.5..12
    void setCoefs(float pos01, float res) noexcept
    {
        static constexpr float kF[kNumVowels][kNumBands] = {
            { 700.0f, 1220.0f, 2600.0f },   // A
            { 400.0f, 1700.0f, 2600.0f },   // E
            { 250.0f, 1750.0f, 2600.0f },   // I
            { 400.0f,  750.0f, 2400.0f },   // O
            { 250.0f,  600.0f, 2400.0f },   // U
        };
        static constexpr float kGain[kNumBands] = { 1.0f, 0.62f, 0.28f };

        const float fp = juce::jlimit(0.0f, 1.0f, pos01) * (float)(kNumVowels - 1);
        const int   i0 = juce::jlimit(0, kNumVowels - 1, (int)fp);
        const int   i1 = juce::jmin(kNumVowels - 1, i0 + 1);
        const float t  = fp - (float)i0;

        // フォルマント周波数は対数補間 (直線補間だと中間で不自然に低くなる)
        const float q = juce::jlimit(1.0f, 14.0f, res * 1.4f);
        for (int b = 0; b < kNumBands; ++b)
        {
            const float f = std::exp2(std::log2(kF[i0][b]) * (1.0f - t)
                                    + std::log2(kF[i1][b]) * t);
            bands[(size_t)b].setCoefs(TptSvf::computeCoefs(f, q, sr));
            gain[(size_t)b] = kGain[b];
        }
    }

    inline void processStereo(float& l, float& r) noexcept
    {
        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < kNumBands; ++b)
        {
            float bl = l, br = r;
            bands[(size_t)b].processStereo(bl, br);
            outL += bl * gain[(size_t)b];
            outR += br * gain[(size_t)b];
        }
        // バンドパス3本の合成はレベルが上がりやすいので控えめに戻す
        l = outL * 0.8f;
        r = outR * 0.8f;
    }

private:
    double sr = 44100.0;
    std::array<TptSvf, kNumBands> bands;
    std::array<float, kNumBands> gain { 1.0f, 0.6f, 0.3f };
};

// ==========================================================
//  CombFilter — フィードバック・コムフィルター (レゾネーター)
//
//  遅延長 = sr / freq のフィードバックコム。ノイズに通すと
//  「ノイズなのに音程を持つ」ライザーになる (Alien / Sci-Fi 系の定番)。
//  スケール量子化と組み合わせると、このプラグイン固有の質感が出せる。
//
//  CUTOFF ノブが共鳴周波数、RES がフィードバック量。
// ==========================================================
class CombFilter
{
public:
    void prepare(double sampleRate)
    {
        sr = juce::jmax(8000.0, sampleRate);
        // 最低共振周波数 20Hz ぶんの遅延を確保
        maxDelay = (int)(sr / 20.0) + 4;
        bufL.assign((size_t)maxDelay, 0.0f);
        bufR.assign((size_t)maxDelay, 0.0f);
        writePos = 0;
        // 共振周波数の変化を滑らかにする (τ≒8ms)。カーブで高速スイープしても
        // 遅延長がジャンプせず、テープのようなグライドになる。
        glideCoef = (float)(1.0 - std::exp(-1.0 / (0.008 * sr)));
        delaySm = 0.0f;
        reset();
    }

    void reset() noexcept
    {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        writePos = 0;
        delaySm = 0.0f;
    }

    // freqHz: 共振周波数 / res: 0.5..12 → フィードバック 0..0.97
    void setCoefs(float freqHz, float res) noexcept
    {
        const float f = juce::jlimit(20.0f, (float)(sr * 0.45), freqHz);
        targetDelay = juce::jlimit(2.0f, (float)(maxDelay - 2), (float)(sr / f));
        fb = juce::jlimit(0.0f, 0.97f, (juce::jlimit(0.5f, 12.0f, res) - 0.5f) / 11.5f * 0.97f);
    }

    inline void processStereo(float& l, float& r) noexcept
    {
        if (bufL.empty()) return;

        if (delaySm <= 0.0f) delaySm = targetDelay;     // 初回はスナップ
        delaySm += glideCoef * (targetDelay - delaySm);

        float readPos = (float)writePos - delaySm;
        while (readPos < 0.0f) readPos += (float)maxDelay;

        const int i0 = (int)readPos;
        const float frac = readPos - (float)i0;
        const int i1 = (i0 + 1 >= maxDelay) ? 0 : i0 + 1;

        const float dl = bufL[(size_t)i0] * (1.0f - frac) + bufL[(size_t)i1] * frac;
        const float dr = bufR[(size_t)i0] * (1.0f - frac) + bufR[(size_t)i1] * frac;

        // ループ内は必ずソフトクリップ + 非正規化数対策 (暴走防止)
        float wl = l + dl * fb;
        float wr = r + dr * fb;
        wl = (std::abs(wl) < 1.0e-20f) ? 0.0f : juce::jlimit(-4.0f, 4.0f, wl);
        wr = (std::abs(wr) < 1.0e-20f) ? 0.0f : juce::jlimit(-4.0f, 4.0f, wr);

        bufL[(size_t)writePos] = wl;
        bufR[(size_t)writePos] = wr;
        if (++writePos >= maxDelay) writePos = 0;

        // フィードバックが強いほど音量が上がるので正規化して返す
        const float norm = 1.0f - fb * 0.65f;
        l = wl * norm;
        r = wr * norm;
    }

private:
    double sr = 44100.0;
    int maxDelay = 0;
    int writePos = 0;
    float targetDelay = 100.0f, delaySm = 0.0f;
    float fb = 0.0f, glideCoef = 0.02f;
    std::vector<float> bufL, bufR;
};
