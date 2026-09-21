// ==========================================
// File: Limiter.h
// 最終段ブリックウォール・リミッター (SPECTRA8のBrickLimiterを移植・拡張)
//  - LIFT-X版: Ceiling(dB)とRelease(ms)をパラメーター化 (Config/Masterエリア)
//  - 瞬間アタック(そのサンプルが天井を超えるなら即座にゲインを下げて捕捉)
//  - リリースは緩やか(ポンピング抑制)。最終段にハードクリップの保険。
//  - レイテンシ0(先読みなし)のためPDC不要。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>

class BrickLimiter
{
public:
    void prepare(double sampleRate)
    {
        sr = juce::jmax(8000.0, sampleRate);
        atkCoef = (float)(1.0 - std::exp(-1.0 / (0.0005 * sr))); // τ = 0.5ms (トランジェント歪み抑制)
        setRelease(120.0f);
        gain = 1.0f;
    }

    void reset() { gain = 1.0f; }

    // releaseMs: 20..1000 (ブロック毎に呼んでも安価)
    void setRelease(float releaseMs)
    {
        releaseMs = juce::jlimit(20.0f, 1000.0f, releaseMs);
        relCoef = (float)(1.0 - std::exp(-1.0 / ((double)releaseMs * 0.001 * sr)));
    }

    // ceilingLin: リニアゲイン (dBから変換済み, 0.25..1.0)
    void process(float* left, float* right, int numSamples, float ceilingLin) noexcept
    {
        const float ceil = juce::jlimit(0.05f, 1.0f, ceilingLin);
        for (int n = 0; n < numSamples; ++n)
        {
            const float peak = juce::jmax(std::abs(left[n]), std::abs(right[n]));
            const float gNeeded = (peak > ceil) ? (ceil / peak) : 1.0f;

            if (gNeeded < gain)
                gain += atkCoef * (gNeeded - gain);      // 0.5ms 極小アタック平滑化
            else
                gain += relCoef * (gNeeded - gain);      // 緩やかリリース

            left[n]  = juce::jlimit(-ceil, ceil, left[n]  * gain);
            right[n] = juce::jlimit(-ceil, ceil, right[n] * gain);
        }
    }

private:
    double sr = 44100.0;
    float atkCoef = 0.05f;
    float relCoef = 0.0f;
    float gain = 1.0f;
};
