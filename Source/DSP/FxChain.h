// ==========================================
// File: FxChain.h
// 6スロット直列FXチェーン (Granular の FxChain を移植・拡張)
//
// FX6種:
//  - ADAA Saturation : アンチエイリアス歪み (アルゴリズム10種)
//  - Ensemble Chorus : 4ボイス
//  - Tape Delay      : テンポ同期・入力ダッキング付き
//  - Freeze          : スペクトル的な止め
//  - Shimmer Reverb  : 16ch FDN + オクターブシフター
//  - Beat Ducking    : テンポ同期ポンプ (独立スロット・PPQ同期) ← LIFT-X追加
//
// LIFT-X での変更点:
//  - Chorus/Delay/Freeze の固定長 std::array を prepareToPlay での
//    事前確保 std::vector に変更し、SR 44.1-192kHz で必要長を保証
//    (processBlock 内でのアロケーションは従来通り一切無し)
//  - OctaveShifter のウィンドウ長をSR依存化
//  - FXカーブ (マルチENV) によるスロットWet量変調はプロセッサー側で
//    Params::amount に合成してから渡す
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <random>
#include <vector>

namespace lfx
{
    // ------------------------------------------
    // ユーティリティ
    // ------------------------------------------
    inline float antiDenormal(float x) noexcept
    {
        return (std::abs(x) < 1.0e-20f) ? 0.0f : x;
    }

    inline float safeLoopSaturate(float x) noexcept
    {
        if (x > 1.5f) return 1.5f + std::tanh(x - 1.5f) * 0.1f;
        if (x < -1.5f) return -1.5f + std::tanh(x + 1.5f) * 0.1f;
        return x;
    }

    inline int findNearestPrime(int n)
    {
        auto isPrime = [](int num)
        {
            if (num <= 1) return false;
            if (num <= 3) return true;
            if (num % 2 == 0 || num % 3 == 0) return false;
            for (int i = 5; i * i <= num; i += 6)
                if (num % i == 0 || num % (i + 2) == 0) return false;
            return true;
        };
        if (n <= 2) return 2;
        int up = n;
        while (!isPrime(up)) up++;
        int down = n;
        while (down > 2 && !isPrime(down)) down--;
        return ((up - n) < (n - down)) ? up : down;
    }

    class ChaosLFO
    {
        float phase1 = 0.0f, phase2 = 0.0f, inc1 = 0.0f, inc2 = 0.0f;
    public:
        void setFrequency(float freq, float sampleRate)
        {
            const float safeFreq = (freq > 0.0f) ? freq : 0.0f;
            inc1 = safeFreq / sampleRate;
            inc2 = (safeFreq * 1.41421356f) / sampleRate;
        }
        void setPhase(float p) { phase1 = p; phase2 = p * 1.618f; }
        inline float process() noexcept
        {
            if (inc1 <= 0.0f) return 0.0f;
            phase1 += inc1; if (phase1 >= 1.0f) phase1 -= 1.0f;
            phase2 += inc2; if (phase2 >= 1.0f) phase2 -= 1.0f;
            return (std::sin(phase1 * juce::MathConstants<float>::twoPi)
                  + std::sin(phase2 * juce::MathConstants<float>::twoPi)) * 0.5f;
        }
    };

    // Shimmer用オクターブシフター (SR依存ウィンドウ)
    class OctaveShifter
    {
        static constexpr int kBufSize = 16384;
        std::array<float, kBufSize> buffer {};
        int writeIdx = 0;
        float phase = 0.0f;
        float windowSize = 2048.0f;
    public:
        void prepare(double sr) noexcept
        {
            buffer.fill(0.0f);
            writeIdx = 0;
            phase = 0.0f;
            windowSize = juce::jlimit(1024.0f, 8192.0f, (float)(sr * 0.046));
        }

        float process(float in) noexcept
        {
            buffer[(size_t)writeIdx] = in;

            phase += 1.0f / windowSize;
            if (phase >= 1.0f) phase -= 1.0f;

            float phaseB = phase + 0.5f;
            if (phaseB >= 1.0f) phaseB -= 1.0f;

            const float delayA = (1.0f - phase) * windowSize;
            const float delayB = (1.0f - phaseB) * windowSize;

            auto getInterpolated = [&](float d) noexcept
            {
                float readPos = (float)writeIdx - d;
                if (readPos < 0.0f) readPos += (float)kBufSize;
                const int idx1 = (int)readPos;
                const float frac = readPos - (float)idx1;
                const int idx2 = (idx1 + 1) % kBufSize;
                return buffer[(size_t)idx1] * (1.0f - frac) + buffer[(size_t)idx2] * frac;
            };

            const float outA = getInterpolated(delayA);
            const float outB = getInterpolated(delayB);
            const float winA = 0.5f * (1.0f - std::cos(phase * juce::MathConstants<float>::twoPi));
            const float winB = 0.5f * (1.0f - std::cos(phaseB * juce::MathConstants<float>::twoPi));

            writeIdx = (writeIdx + 1) % kBufSize;
            return outA * winA + outB * winB;
        }
    };

    struct VelvetNoiseDiffuser
    {
        std::vector<float> buffer;
        int writePos = 0;
        int bufferMask = 0;
        struct Tap { int delay; float gain; };
        std::vector<Tap> taps;
        float amount = 0.0f;

        void prepare(float fs)
        {
            const int size = 16384;
            buffer.assign((size_t)size, 0.0f);
            bufferMask = size - 1;
            writePos = 0;

            taps.clear();
            const int numTaps = 48;
            const float durationMs = 50.0f;
            const float totalSamples = durationMs * fs * 0.001f;
            const float grid = totalSamples / (float)numTaps;

            std::mt19937 gen(12345);
            std::uniform_real_distribution<float> distOffset(0.0f, grid - 1.0f);
            std::uniform_int_distribution<> sign(0, 1);
            const float normGain = 1.0f / std::sqrt((float)numTaps);

            for (int i = 0; i < numTaps; ++i)
            {
                Tap t;
                t.delay = (int)((float)i * grid + distOffset(gen));
                t.delay = juce::jlimit(1, size - 1, t.delay);
                t.gain = (sign(gen) == 0 ? 1.0f : -1.0f) * normGain;
                taps.push_back(t);
            }
        }
        void setAmount(float a) { amount = a; }
        inline float process(float in) noexcept
        {
            if (buffer.empty()) return in;
            buffer[(size_t)writePos] = in;

            float out = 0.0f;
            if (amount >= 0.01f)
            {
                for (const auto& t : taps)
                {
                    const int r = (writePos - t.delay) & bufferMask;
                    out += buffer[(size_t)r] * t.gain;
                }
            }

            writePos = (writePos + 1) & bufferMask;

            if (amount < 0.01f) return in;
            return in * (1.0f - amount) + out * amount;
        }
    };

    static inline void matrixHadamard(float* x) noexcept
    {
        for (int i = 0; i < 16; i += 2) { const float a = x[i]; const float b = x[i + 1]; x[i] = a + b; x[i + 1] = a - b; }
        for (int i = 0; i < 16; i += 4)
        {
            const float a0 = x[i]; const float b0 = x[i + 2]; x[i] = a0 + b0; x[i + 2] = a0 - b0;
            const float a1 = x[i + 1]; const float b1 = x[i + 3]; x[i + 1] = a1 + b1; x[i + 3] = a1 - b1;
        }
        for (int i = 0; i < 16; i += 8)
            for (int j = 0; j < 4; ++j)
            { const float a = x[i + j]; const float b = x[i + j + 4]; x[i + j] = a + b; x[i + j + 4] = a - b; }
        for (int i = 0; i < 8; ++i) { const float a = x[i]; const float b = x[i + 8]; x[i] = a + b; x[i + 8] = a - b; }
        for (int i = 0; i < 16; ++i) x[i] *= 0.25f;
    }

    // ------------------------------------------
    // Shimmer Reverb (16ch FDN + OctaveShifter)
    // ------------------------------------------
    class ShimmerReverb
    {
        static constexpr int NUM_CHANNELS = 16;
        std::vector<std::vector<float>> delayBuffers;
        std::array<int, NUM_CHANNELS> writePos {};
        std::array<int, NUM_CHANNELS> delayLengths {};
        std::array<float, NUM_CHANNELS> dampStates {};
        std::array<float, NUM_CHANNELS> dcX1 {}, dcY1 {};
        float curAmount = 0.0f;
        std::array<ChaosLFO, NUM_CHANNELS> lfos;

        VelvetNoiseDiffuser velvetL, velvetR;
        OctaveShifter shifter;

    public:
        ShimmerReverb() { delayBuffers.resize(NUM_CHANNELS); }

        void prepareToPlay(double sr)
        {
            velvetL.prepare((float)sr);
            velvetR.prepare((float)sr);
            shifter.prepare(sr);

            const int maxDelaySamples = (int)(sr * 2.0);

            static const float LFO_RATIOS[16] = {
                1.000f, 0.618f, 1.272f, 0.786f, 1.618f, 0.382f, 1.414f, 0.528f,
                1.175f, 0.854f, 1.324f, 0.472f, 1.089f, 0.927f, 1.236f, 0.691f };
            static const float baseDelaysMs[16] = {
                31.0f, 37.0f, 41.0f, 43.0f, 47.0f, 53.0f, 59.0f, 61.0f,
                67.0f, 71.0f, 73.0f, 79.0f, 83.0f, 89.0f, 97.0f, 101.0f };

            for (int i = 0; i < NUM_CHANNELS; ++i)
            {
                delayBuffers[(size_t)i].assign((size_t)maxDelaySamples, 0.0f);
                const float targetSamps = baseDelaysMs[i] * 0.001f * (float)sr;
                delayLengths[(size_t)i] = findNearestPrime((int)targetSamps);
                if (delayLengths[(size_t)i] > maxDelaySamples - 100) delayLengths[(size_t)i] = maxDelaySamples - 100;
                writePos[(size_t)i] = 0;
                dampStates[(size_t)i] = 0.0f;
                dcX1[(size_t)i] = dcY1[(size_t)i] = 0.0f;
                curAmount = 0.0f;
                lfos[(size_t)i].setFrequency(0.5f * LFO_RATIOS[i], (float)sr);
                lfos[(size_t)i].setPhase((float)i / (float)NUM_CHANNELS);
            }
        }

        void process(float& inOutL, float& inOutR, float amount,
                     float decay, float shimmer, float damp, float mod) noexcept
        {
            // wet量スムージング (急な変調復帰時のFDN蓄積解放バースト防止)
            curAmount += 0.008f * (amount - curAmount);
            if (amount <= 0.0f && curAmount < 1.0e-4f) return;
            amount = curAmount;

            velvetL.setAmount(amount * 0.8f);
            velvetR.setAmount(amount * 0.8f);

            const float vL = velvetL.process(inOutL);
            const float vR = velvetR.process(inOutR);

            const float monoIn = (vL + vR) * 0.5f;
            const float shimmerSig = shifter.process(monoIn);

            const float shimmerMix = shimmer * 0.7f;
            const float injectL = vL + shimmerSig * shimmerMix;
            const float injectR = vR + shimmerSig * shimmerMix;

            float delayOutputs[16] = {};
            float feedbackInputs[16] = {};

            const float lfoDepth = mod * 15.0f;
            const float dampFactor = 0.05f + damp * 0.6f;
            const float feedback = std::min(0.98f, 0.5f + decay * 0.48f);

            for (int i = 0; i < NUM_CHANNELS; ++i)
            {
                const float lfoMod = lfos[(size_t)i].process() * lfoDepth;
                float readPos = (float)writePos[(size_t)i] - ((float)delayLengths[(size_t)i] + lfoMod);
                const int bufSize = (int)delayBuffers[(size_t)i].size();
                if (bufSize == 0) continue;

                while (readPos < 0.0f) readPos += (float)bufSize;
                while (readPos >= (float)bufSize) readPos -= (float)bufSize;

                const int idx1 = (int)readPos;
                const float frac = readPos - (float)idx1;
                int idx2 = idx1 + 1;
                if (idx2 >= bufSize) idx2 = 0;

                const float rawRead = delayBuffers[(size_t)i][(size_t)idx1] * (1.0f - frac)
                                    + delayBuffers[(size_t)i][(size_t)idx2] * frac;

                dampStates[(size_t)i] = rawRead * (1.0f - dampFactor) + dampStates[(size_t)i] * dampFactor;

                // DCブロッカー (フィードバックへのDC/サブソニック蓄積防止)
                const float dcIn = dampStates[(size_t)i];
                const float hp = dcIn - dcX1[(size_t)i] + 0.9975f * dcY1[(size_t)i];
                dcX1[(size_t)i] = dcIn;
                dcY1[(size_t)i] = hp;

                delayOutputs[i] = hp;
                feedbackInputs[i] = hp;
            }

            matrixHadamard(feedbackInputs);

            float sumL = 0.0f, sumR = 0.0f;
            for (int i = 0; i < NUM_CHANNELS; ++i)
            {
                const int bufSize = (int)delayBuffers[(size_t)i].size();
                if (bufSize == 0) continue;

                const float inSig = (i % 2 == 0) ? injectL : injectR;
                // in×(1-fb)+loop×fb : 定常ゲイン1収束 (蓄積暴発防止)
                float v_n = inSig * (1.0f - feedback) + feedbackInputs[i] * feedback;
                v_n = antiDenormal(safeLoopSaturate(v_n));

                delayBuffers[(size_t)i][(size_t)writePos[(size_t)i]] = v_n;
                if (++writePos[(size_t)i] >= bufSize) writePos[(size_t)i] = 0;

                if (i < 8) sumL += delayOutputs[i];
                else       sumR += delayOutputs[i];
            }

            sumL *= 0.25f;
            sumR *= 0.25f;

            auto softClipOutput = [](float x) noexcept
            {
                const float ax = std::abs(x);
                if (ax > 1.0f) return x > 0.0f ? 1.0f : -1.0f;
                return x * (1.5f - 0.5f * x * x);
            };

            inOutL = inOutL * (1.0f - amount) + softClipOutput(sumL) * amount * 1.2f;
            inOutR = inOutR * (1.0f - amount) + softClipOutput(sumR) * amount * 1.2f;
        }
    };

    // ------------------------------------------
    // Ensemble Chorus (4ボイス / SR依存事前確保)
    // ------------------------------------------
    class EnsembleChorus
    {
    public:
        void prepareToPlay(double sr)
        {
            sampleRate = sr;
            // 最大遅延 ~30ms + マージン → 0.25s 分を確保
            halfSize = juce::jmax(4096, (int)(sr * 0.25));
            delayBuffer.assign((size_t)halfSize * 2, 0.0f);
            writeIndex = 0;
            lfoPhase1 = lfoPhase2 = 0.0f;
        }

        void process(float& inOutL, float& inOutR, float amount,
                     float rate, float depth, float width) noexcept
        {
            if (delayBuffer.empty()) return;

            // amount<=0 でも LFO/バッファ更新は継続 (陳腐化バースト防止)
            const float lfoRate1 = rate;
            const float lfoRate2 = rate * 1.5f;

            lfoPhase1 += lfoRate1 / (float)sampleRate;
            if (lfoPhase1 >= 1.0f) lfoPhase1 -= 1.0f;
            lfoPhase2 += lfoRate2 / (float)sampleRate;
            if (lfoPhase2 >= 1.0f) lfoPhase2 -= 1.0f;

            const float mix = amount * 0.5f;
            float outL = 0.0f, outR = 0.0f;

            auto hermite = [](float frac, float y0, float y1, float y2, float y3) noexcept
            {
                const float c0 = y1;
                const float c1 = 0.5f * (y2 - y0);
                const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                return ((c3 * frac + c2) * frac + c1) * frac + c0;
            };

            float mods[4];
            mods[0] = std::sin(lfoPhase1 * juce::MathConstants<float>::twoPi);
            mods[1] = -mods[0];
            mods[2] = std::sin(lfoPhase2 * juce::MathConstants<float>::twoPi);
            mods[3] = -mods[2];

            for (int i = 0; i < 4; ++i)
            {
                const float delayMs = 12.0f + (mods[i] * 5.0f * depth) + ((float)i * 3.0f);
                const float delaySamps = delayMs * (float)(sampleRate / 1000.0);

                float readPos = (float)writeIndex - delaySamps;
                if (readPos < 0.0f) readPos += (float)halfSize;

                int idx1 = (int)readPos;
                if (idx1 >= halfSize) idx1 = 0;
                if (idx1 < 0) idx1 += halfSize;
                const float frac = readPos - (float)idx1;

                int idx0 = idx1 - 1; if (idx0 < 0) idx0 += halfSize;
                int idx2 = idx1 + 1; if (idx2 >= halfSize) idx2 -= halfSize;
                int idx3 = idx1 + 2; if (idx3 >= halfSize) idx3 -= halfSize;

                const float chL = hermite(frac, delayBuffer[(size_t)idx0 * 2], delayBuffer[(size_t)idx1 * 2],
                                          delayBuffer[(size_t)idx2 * 2], delayBuffer[(size_t)idx3 * 2]);
                const float chR = hermite(frac, delayBuffer[(size_t)idx0 * 2 + 1], delayBuffer[(size_t)idx1 * 2 + 1],
                                          delayBuffer[(size_t)idx2 * 2 + 1], delayBuffer[(size_t)idx3 * 2 + 1]);

                if (i == 0 || i == 2)
                {
                    outL += chL * 0.7f + chR * 0.3f;
                    outR += chR * 0.3f - chL * 0.3f;
                }
                else
                {
                    outR += chR * 0.7f + chL * 0.3f;
                    outL += chL * 0.3f - chR * 0.3f;
                }
            }

            delayBuffer[(size_t)writeIndex * 2] = inOutL;
            delayBuffer[(size_t)writeIndex * 2 + 1] = inOutR;

            if (++writeIndex >= halfSize) writeIndex = 0;

            // WIDTH: Mid/Side バランス
            float wetL = outL * 0.25f;
            float wetR = outR * 0.25f;
            const float mono = (wetL + wetR) * 0.5f;
            wetL = mono + (wetL - mono) * width;
            wetR = mono + (wetR - mono) * width;

            if (amount > 0.0f)
            {
                inOutL = inOutL * (1.0f - mix) + wetL * mix;
                inOutR = inOutR * (1.0f - mix) + wetR * mix;
            }
        }

    private:
        double sampleRate = 44100.0;
        std::vector<float> delayBuffer;
        int halfSize = 0;
        int writeIndex = 0;
        float lfoPhase1 = 0.0f, lfoPhase2 = 0.0f;
    };

    // ------------------------------------------
    // Tape Delay (テンポ同期・ダッキング付き / SR依存事前確保)
    // ------------------------------------------
    class TapeDelay
    {
    public:
        void prepareToPlay(double sr)
        {
            sampleRate = sr;
            // 最大 1/2音符 @ 40BPM = 3s → 4.5s 分を確保 (192kHzでも安全)
            halfSize = juce::jmax(8192, (int)(sr * 4.5));
            delayBuffer.assign((size_t)halfSize * 2, 0.0f);
            writeIndex = 0;
            envelope = 0.0f;
            currentDelaySamps = 0.0f;
            curAmount = 0.0f;
            curFeedback = 0.0f;
            lpStateL = lpStateR = 0.0f;
            dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
        }

        void process(float& inOutL, float& inOutR, float amount, double bpm,
                     float timeBeats, float feedback, float duck, float damp) noexcept
        {
            if (delayBuffer.empty()) return;

            const float amountSmoothCoef = 1.0f - std::exp(-1.0f / (0.015f * (float)sampleRate));
            const float fbSmoothCoef = 1.0f - std::exp(-1.0f / (0.015f * (float)sampleRate));
            curAmount   += amountSmoothCoef * (amount - curAmount);
            curFeedback += fbSmoothCoef * (juce::jlimit(0.0f, 0.95f, feedback) - curFeedback);

            const float inSum = std::abs(inOutL) + std::abs(inOutR);
            const float attack = 0.001f;
            const float release = 0.0002f;
            if (inSum > envelope) envelope += attack * (inSum - envelope);
            else envelope += release * (inSum - envelope);

            const float duckingGain = 1.0f - juce::jlimit(0.0f, 0.85f, envelope * 4.0f * duck);

            const double effectiveBpm = bpm > 0.0 ? bpm : 120.0;
            const double beatSec = 60.0 / effectiveBpm;

            float targetDelaySamps = (float)(sampleRate * beatSec * (double)timeBeats);
            targetDelaySamps = juce::jlimit(32.0f, (float)(halfSize - 2), targetDelaySamps);

            if (currentDelaySamps == 0.0f) currentDelaySamps = targetDelaySamps;
            const float delaySmoothCoef = 1.0f - std::exp(-1.0f / (0.02f * (float)sampleRate));
            currentDelaySamps += delaySmoothCoef * (targetDelaySamps - currentDelaySamps);

            float readPos = (float)writeIndex - currentDelaySamps;
            if (readPos < 0.0f) readPos += (float)halfSize;

            int idx1 = (int)readPos;
            if (idx1 >= halfSize) idx1 = 0;
            if (idx1 < 0) idx1 += halfSize;
            const float frac = readPos - (float)idx1;

            int idx2 = idx1 + 1;
            if (idx2 >= halfSize) idx2 -= halfSize;

            float dL = delayBuffer[(size_t)idx1 * 2] * (1.0f - frac) + delayBuffer[(size_t)idx2 * 2] * frac;
            float dR = delayBuffer[(size_t)idx1 * 2 + 1] * (1.0f - frac) + delayBuffer[(size_t)idx2 * 2 + 1] * frac;

            // DAMP: フィードバックループ内の高域減衰
            const float lpCoef = 1.0f - damp * 0.85f;
            lpStateL += lpCoef * (dL - lpStateL);
            lpStateR += lpCoef * (dR - lpStateR);
            dL = lpStateL;
            dR = lpStateR;

            // DCブロッカー
            const float hpL = dL - dcX1L + 0.9975f * dcY1L; dcX1L = dL; dcY1L = hpL; dL = hpL;
            const float hpR = dR - dcX1R + 0.9975f * dcY1R; dcX1R = dR; dcY1R = hpR; dR = hpR;

            const float fb = curFeedback;
            delayBuffer[(size_t)writeIndex * 2] =
                lfx::antiDenormal(lfx::safeLoopSaturate((inOutL * duckingGain) + (dL * fb)));
            delayBuffer[(size_t)writeIndex * 2 + 1] =
                lfx::antiDenormal(lfx::safeLoopSaturate((inOutR * duckingGain) + (dR * fb)));

            if (++writeIndex >= halfSize) writeIndex = 0;

            inOutL += dL * curAmount;
            inOutR += dR * curAmount;
        }

    private:
        double sampleRate = 44100.0;
        std::vector<float> delayBuffer;
        int halfSize = 0;
        int writeIndex = 0;
        float envelope = 0.0f;
        float currentDelaySamps = 0.0f;
        float lpStateL = 0.0f, lpStateR = 0.0f;
        float curAmount = 0.0f;
        float curFeedback = 0.0f;
        float dcX1L = 0.0f, dcY1L = 0.0f, dcX1R = 0.0f, dcY1R = 0.0f;
    };

    // ------------------------------------------
    // Freeze / Smear (SR依存事前確保)
    // ------------------------------------------
    class FreezeSmear
    {
    public:
        void prepareToPlay(double sr)
        {
            sampleRate = sr;
            // 最大ループ長 1000ms + マージン → 1.2s 分を確保
            halfSize = juce::jmax(4096, (int)(sr * 1.2));
            buffer.assign((size_t)halfSize * 2, 0.0f);
            writeIndex = 0;
            lpStateL = lpStateR = 0.0f;
        }

        void process(float& inOutL, float& inOutR, float amount,
                     float sizeMs, float feedback, float damp) noexcept
        {
            if (buffer.empty()) return;

            // amount==0 でもバッファ書き込みは継続 (残存データ爆発防止)
            const int delaySamps = juce::jlimit(64, halfSize - 2,
                                                (int)(sampleRate * (double)sizeMs * 0.001));
            int readIndex = writeIndex - delaySamps;
            if (readIndex < 0) readIndex += halfSize;

            float smL = buffer[(size_t)readIndex * 2];
            float smR = buffer[(size_t)readIndex * 2 + 1];

            const float lpCoef = 1.0f - damp * 0.85f;
            lpStateL += lpCoef * (smL - lpStateL);
            lpStateR += lpCoef * (smR - lpStateR);
            smL = lpStateL;
            smR = lpStateR;

            // クロスフェード蓄積器: in×(1-fb)+loop×fb (定常ゲイン1収束)
            const float fb = juce::jlimit(0.0f, 0.99f, feedback);
            buffer[(size_t)writeIndex * 2] =
                lfx::antiDenormal(lfx::safeLoopSaturate(inOutL * (1.0f - fb) + smL * fb));
            buffer[(size_t)writeIndex * 2 + 1] =
                lfx::antiDenormal(lfx::safeLoopSaturate(inOutR * (1.0f - fb) + smR * fb));

            if (++writeIndex >= halfSize) writeIndex = 0;

            if (amount > 0.0f)
            {
                inOutL = inOutL * (1.0f - amount) + smL * amount;
                inOutR = inOutR * (1.0f - amount) + smR * amount;
            }
        }

    private:
        double sampleRate = 44100.0;
        std::vector<float> buffer;
        int halfSize = 0;
        int writeIndex = 0;
        float lpStateL = 0.0f, lpStateR = 0.0f;
    };

    // ------------------------------------------
    // Beat Ducking (テンポ同期ポンプ / LIFT-X追加・独立スロット)
    //  PPQ同期でグリッドに正確に張り付く。サイドチェイン入力不要。
    // ------------------------------------------
    class BeatDucker
    {
    public:
        void prepareToPlay(double sr)
        {
            sampleRate = sr;
            phase = 0.0f;
            gainSm = 1.0f;
            dipCoef = 1.0f - std::exp(-1.0f / (0.002f * (float)sr)); // 2ms (dip側)
            relCoef = 1.0f - std::exp(-1.0f / (0.012f * (float)sr)); // 12ms (復帰側)
        }

        // ブロック頭でホストPPQに位相を同期
        void syncTo(double ppq, float cycleBeats) noexcept
        {
            if (cycleBeats <= 0.0f) return;
            const double c = (double)cycleBeats;
            phase = (float)(std::fmod(std::fmod(ppq, c) + c, c) / c);
        }

        void process(float& l, float& r, float amount, double bpm,
                     float cycleBeats, float shape) noexcept
        {
            const double safeBpm = (bpm > 0.0) ? bpm : 120.0;
            phase += (float)((safeBpm / 60.0) / (sampleRate * (double)juce::jmax(0.0625f, cycleBeats)));
            if (phase >= 1.0f) phase -= 1.0f;

            // 拍頭で深く沈み、シェイプに従って復帰するポンプカーブ
            const float dip = std::pow(1.0f - phase, juce::jlimit(0.5f, 8.0f, shape));
            const float target = 1.0f - juce::jlimit(0.0f, 1.0f, amount) * 0.95f * dip;

            gainSm += (target < gainSm ? dipCoef : relCoef) * (target - gainSm);
            l *= gainSm;
            r *= gainSm;
        }

    private:
        double sampleRate = 44100.0;
        float phase = 0.0f;
        float gainSm = 1.0f;
        float dipCoef = 0.02f, relCoef = 0.004f;
    };

    // ------------------------------------------
    // ADAA Saturation
    // ------------------------------------------
    struct SaturationState
    {
        float tapeHysteresis = 0.0f;
        float lastX = 0.0f;
        float lastF = 0.0f;
        bool  active = false;

        void reset() noexcept { tapeHysteresis = 0.0f; lastX = 0.0f; lastF = 0.0f; active = false; }
    };

    inline float calcADAAFunc(float x, int type) noexcept
    {
        switch (type)
        {
        case 0: // Soft Tanh
            if (std::abs(x) > 10.0f) return std::abs(x) - 0.693147f;
            return std::log(std::cosh(x));
        case 1: // Hard Clip
            if (x < -1.0f) return -x - 0.5f;
            if (x > 1.0f)  return  x - 0.5f;
            return 0.5f * x * x;
        case 6: // BJT (Atan based)
        {
            const float k = 2.2f;
            const float scale = 0.58f;
            const float term1 = x * std::atan(k * x);
            const float term2 = (0.5f / k) * std::log(1.0f + k * k * x * x);
            return scale * (term1 - term2);
        }
        case 7: // Wavefold
            return -1.0f / juce::MathConstants<float>::pi * std::cos(x * juce::MathConstants<float>::pi);
        case 10: // Cubic
            return (0.5f * x * x) - (x * x * x * x * 0.08333333f);
        default: return 0.0f;
        }
    }

    inline float processSaturationSampleADAA(float x, int type, float drive, SaturationState& state) noexcept
    {
        if (drive <= 1.001f)
        {
            state.active = false;
            state.lastX = x;
            return x;
        }

        // Tape (履歴を使う特殊処理)
        if (type == 3)
        {
            const float g = x * drive;
            const float y = 0.92f * std::tanh(g + 0.08f * state.tapeHysteresis);
            state.tapeHysteresis = y;
            return y;
        }

        // ADAAを使わない静的カーブ群
        if (type == 2 || type == 4 || type == 5 || type == 8 || type == 9 || type == 10)
        {
            const float g = x * drive;
            switch (type)
            {
            case 2: { const float b = 0.25f; return std::tanh(g + b) - std::tanh(b); }        // Triode
            case 4: return g / (1.0f + 0.45f * std::abs(g));                                  // Transformer
            case 5: return (std::abs(g) < 1.0f) ? g - (g * g * g) / 3.0f
                                                : (g > 0 ? (2.0f / 3.0f) : -(2.0f / 3.0f));   // JFET
            case 8: { const float step = 1.0f / (1.0f + (25.0f - drive)); return std::round(g / step) * step; }
            case 9: { const float s = std::tanh(g); return s + 0.3f * (std::tanh(3.0f * g) - s); } // Exciter
            case 10: {
                const float c = juce::jlimit(-1.0f, 1.0f, g);
                return 1.5f * c - 0.5f * c * c * c;
            }
            default: return g;
            }
        }

        // ADAA (1次アンチエイリアス) を使う群 (0,1,6,7)
        const float g = x * drive;

        auto direct = [type](float v) noexcept
        {
            switch (type)
            {
            case 0: return std::tanh(v);
            case 1: return juce::jlimit(-1.0f, 1.0f, v);
            case 6: return std::atan(v * 2.2f) * 0.58f;
            case 7: return std::sin(v * juce::MathConstants<float>::pi);
            case 10: return v - (v * v * v) / 3.1f;
            default: return v;
            }
        };

        if (!state.active)
        {
            state.active = true;
            state.lastX = g;
            state.lastF = calcADAAFunc(g, type);
            return direct(g);
        }

        const float Fx = calcADAAFunc(g, type);
        const float delta = g - state.lastX;

        float output;
        if (std::abs(delta) < 1.0e-5f)
            output = direct(g);
        else
            output = (Fx - state.lastF) / delta;

        state.lastX = g;
        state.lastF = Fx;
        return output;
    }
} // namespace lfx

// ==========================================
// FxChain — 6スロット直列
// ==========================================
class FxChain
{
public:
    FxChain() = default;

    static constexpr int kNumSlots = 6; // FXの数と一致

    enum FxType { None = 0, Saturation, Chorus, Delay, Freeze, Reverb, Ducking };

    static juce::StringArray getTypeNames()
    {
        return { "None", "Saturation (ADAA)", "Ensemble Chorus", "Tape Delay",
                 "Freeze", "Shimmer Reverb", "Beat Ducking" };
    }
    static juce::StringArray getSatAlgoNames()
    {
        return { "Soft Tanh", "Hard Clip", "Triode", "Tape", "Transformer",
                 "JFET", "BJT", "Wavefold", "Exciter", "Cubic" };
    }
    static int satAlgoToType(int combo) noexcept
    {
        static const int map[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10 };
        return map[juce::jlimit(0, 9, combo)];
    }

    // Delayタイム表 (拍数)
    static juce::StringArray getDelayTimeNames()
    {
        return { "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T" };
    }
    static float delayTimeToBeats(int idx) noexcept
    {
        static const float beats[10] = { 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f,
                                         0.5f, 1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f };
        return beats[juce::jlimit(0, 9, idx)];
    }

    // Duckingレート表 (1サイクルの拍数)
    static juce::StringArray getDuckRateNames()
    {
        return { "1 Bar", "1/2", "1/4", "1/8" };
    }
    static float duckRateToBeats(int idx) noexcept
    {
        static const float beats[4] = { 4.0f, 2.0f, 1.0f, 0.5f };
        return beats[juce::jlimit(0, 3, idx)];
    }

    struct Params
    {
        std::array<int, kNumSlots> type {};
        std::array<float, kNumSlots> amount {};   // 0..1 (Wet量。FXカーブ変調は合成済みで渡す)
        double bpm = 120.0;
        double ppq = 0.0;        // ブロック頭のPPQ (Ducking同期用)
        bool   playing = false;

        // --- Saturation 詳細 ---
        int   satAlgo = 0;
        float satDrive = 2.0f;      // 1..12
        float satPreHz = 20.0f;     // 20..2000 (20≒スルー)
        float satTrimDb = 0.0f;     // -12..+12

        // --- Chorus 詳細 ---
        float choRate = 0.8f;       // Hz
        float choDepth = 0.5f;      // 0..1
        float choWidth = 1.0f;      // 0..1

        // --- Delay 詳細 ---
        int   dlyTime = 5;          // getDelayTimeNames() インデックス
        float dlyFeedback = 0.45f;  // 0..0.95
        float dlyDuck = 0.5f;       // 0..1
        float dlyDamp = 0.3f;       // 0..1

        // --- Freeze 詳細 ---
        float frzSize = 100.0f;     // ms
        float frzFeedback = 0.9f;   // 0..0.99
        float frzDamp = 0.2f;       // 0..1

        // --- Reverb 詳細 ---
        float revDecay = 0.7f;      // 0..1
        float revShimmer = 0.4f;    // 0..1
        float revDamp = 0.3f;       // 0..1
        float revMod = 0.4f;        // 0..1

        // --- Ducking 詳細 (LIFT-X追加) ---
        int   duckRate = 2;         // getDuckRateNames() インデックス
        float duckShape = 2.0f;     // 0.5..8
    };

    void prepare(double sr)
    {
        sampleRate = sr;
        chorus.prepareToPlay(sr);
        delay.prepareToPlay(sr);
        freeze.prepareToPlay(sr);
        reverb.prepareToPlay(sr);
        ducker.prepareToPlay(sr);
        dcCoef = std::exp((float)(-1.0 / (0.004523 * sr)));
        for (int ch = 0; ch < 2; ++ch) { satState[ch].reset(); dcState[ch] = 0.0f; }
    }

    void process(juce::AudioBuffer<float>& buf, const Params& p) noexcept
    {
        const int numSamples = buf.getNumSamples();
        const int channels = buf.getNumChannels();
        if (numSamples <= 0 || channels == 0) return;

        // 全スロット None のときのみ早期リターン
        // (amount==0 でも Delay/Freeze/Reverb の内部バッファ更新は継続する)
        bool anyTyped = false;
        for (int s = 0; s < kNumSlots; ++s)
            if (p.type[(size_t)s] > 0) anyTyped = true;
        if (!anyTyped) return;

        float* dL = buf.getWritePointer(0);
        float* dR = channels > 1 ? buf.getWritePointer(1) : dL;

        const int satType = satAlgoToType(p.satAlgo);
        const float trimGain = juce::Decibels::decibelsToGain(juce::jlimit(-12.0f, 12.0f, p.satTrimDb));
        const float preAlpha = 1.0f / (1.0f + juce::MathConstants<float>::twoPi
                                       * juce::jlimit(20.0f, 2000.0f, p.satPreHz) / (float)sampleRate);
        const float dlyBeats = delayTimeToBeats(p.dlyTime);
        const float duckBeats = duckRateToBeats(p.duckRate);

        // Ducking: 再生中はブロック頭でPPQへ位相同期 (グリッドに正確に張り付く)
        if (p.playing)
            ducker.syncTo(p.ppq, duckBeats);

        // スロット間ソフトクリップ
        auto interSlotClip = [](float x) noexcept -> float
        {
            const float t = 1.2f;
            const float a = std::abs(x);
            if (a <= t) return x;
            const float s = x < 0.0f ? -1.0f : 1.0f;
            return s * (t + (1.0f - t * 0.25f) * std::tanh((a - t) * 2.0f));
        };

        for (int i = 0; i < numSamples; ++i)
        {
            float l = dL[i];
            float r = dR[i];

            for (int s = 0; s < kNumSlots; ++s)
            {
                const int fxType = p.type[(size_t)s];
                if (fxType == None) continue;

                const float amt = p.amount[(size_t)s];

                switch (fxType)
                {
                case Saturation:
                    if (amt > 0.0005f)
                        saturate(l, r, amt, satType, p.satDrive, preAlpha, trimGain);
                    break;
                case Chorus:
                    chorus.process(l, r, amt, p.choRate, p.choDepth, p.choWidth);
                    break;
                case Delay:
                    delay.process(l, r, amt, p.bpm, dlyBeats, p.dlyFeedback, p.dlyDuck, p.dlyDamp);
                    break;
                case Freeze:
                    freeze.process(l, r, amt, p.frzSize, p.frzFeedback, p.frzDamp);
                    break;
                case Reverb:
                    reverb.process(l, r, amt, p.revDecay, p.revShimmer, p.revDamp, p.revMod);
                    break;
                case Ducking:
                    ducker.process(l, r, amt, p.bpm, duckBeats, p.duckShape);
                    break;
                default: break;
                }

                if (s < kNumSlots - 1)
                {
                    l = interSlotClip(l);
                    r = interSlotClip(r);
                }
            }

            dL[i] = l;
            if (channels > 1) dR[i] = r;
        }
    }

private:
    inline void saturate(float& l, float& r, float amount, int satType,
                         float drive, float preAlpha, float trimGain) noexcept
    {
        const float comp = 1.0f / std::sqrt(juce::jmax(1.0f, drive));
        const float mix = amount;

        auto one = [&](float x, int ch) noexcept
        {
            const float hpfOut = preAlpha * (preY1[ch] + x - preX1[ch]);
            preX1[ch] = x;
            preY1[ch] = hpfOut;

            float sat = lfx::processSaturationSampleADAA(hpfOut, satType, drive, satState[ch]);
            dcState[ch] = dcCoef * dcState[ch] + (1.0f - dcCoef) * sat;
            sat -= dcState[ch];
            return x * (1.0f - mix) + sat * comp * trimGain * mix;
        };

        l = one(l, 0);
        r = one(r, 1);
    }

    double sampleRate = 44100.0;

    lfx::EnsembleChorus chorus;
    lfx::TapeDelay delay;
    lfx::FreezeSmear freeze;
    lfx::ShimmerReverb reverb;
    lfx::BeatDucker ducker;

    lfx::SaturationState satState[2];
    float dcState[2] = { 0.0f, 0.0f };
    float preX1[2] = { 0.0f, 0.0f };
    float preY1[2] = { 0.0f, 0.0f };
    float dcCoef = 0.995f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxChain)
};
