// ==========================================
// File: FxChain.h
// 5スロット直列FXチェーン (Granular の FxChain を移植・拡張 / v0.2)
//
// FX5種 (スロットは適用順序の並び替え用):
//  - ADAA Saturation : アンチエイリアス歪み (アルゴリズム10種)
//  - Ensemble Chorus : 4ボイス
//  - Tape Delay      : テンポ同期・入力ダッキング付き
//  - Shimmer Reverb  : 16ch FDN + オクターブシフター
//  - Beat Ducking    : テンポ同期ポンプ (PPQ同期, 1Bar〜1/64 付点/三連対応)
//
// LIFT-X v0.2 での変更点:
//  - Freeze を削除、スロット数 5 (SPECTRA8と同じ「順序を選ぶ」方式)
//  - AMT等のパラメーターはFX毎に保持し、マルチENVカーブによる
//    バイポーラ加算変調をプロセッサー側で合成してから渡す
//  - Delay Time / Duck Rate はカーブで拍長を±2オクターブ変調可能
//    (dlyBeats/duckBeats として合成済みの拍数を受け取る)
//  - Chorus/Delay の固定長バッファは prepareToPlay でのSR依存事前確保
//    (processBlock 内でのアロケーションは一切無し)
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

            // wet量平滑の時定数を実時間で固定 (τ≒20ms) してSR非依存にする
            amtCoef = (float)(1.0 - std::exp(-1.0 / (0.020 * sr)));
            fullyCleared = true;   // prepare直後は全バッファが 0
            clearCh = 0;
            clearIdx = 0;
        }

        void process(float& inOutL, float& inOutR, float amount,
                     float decay, float shimmer, float damp, float mod) noexcept
        {
            // wet量スムージング (急な変調復帰時のFDN蓄積解放バースト防止)。
            //  係数は prepareToPlay で SR から算出する (旧実装は 0.008f 固定で、
            //  192kHz では 44.1kHz の 4.35倍速い平滑になっていた)。
            curAmount += amtCoef * (amount - curAmount);

            if (amount <= 0.0f && curAmount < 1.0e-4f)
            {
                // 完全に切れている間は FDN を回さない。ただし単に return すると
                // 遅延バッファに古い残響が残ったままになり、AMT を戻した瞬間に
                // それが蘇ってしまう。ここで少しずつゼロクリアしておく。
                // (一度に memset すると数MBになりブロックを踏み外すため分割する)
                if (!fullyCleared) clearStep();
                return;
            }
            // 再びアクティブになったのでクリア進捗をリセット
            fullyCleared = false;
            clearCh = 0;
            clearIdx = 0;

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

    private:
        // FDN遅延バッファの分割ゼロクリア。
        //  リバーブが完全にOFFのあいだ、1呼び出しあたり kChunk サンプルずつ消す。
        //  44.1kHz で全16ch (計約1.4Mサンプル) を約0.5秒かけて掃除し終える。
        //  掃除中も出力には一切影響しない (どのみち無音のため)。
        //  ※ Velvet/OctaveShifter はフィードバックを持たない前段なので、
        //    古い内容が残っていても数十msで自然に流れ出る。クリア対象外でよい。
        void clearStep() noexcept
        {
            constexpr int kChunk = 64;

            if (clearCh == 0 && clearIdx == 0)
            {
                dampStates.fill(0.0f);
                dcX1.fill(0.0f);
                dcY1.fill(0.0f);
            }

            if (clearCh >= NUM_CHANNELS) { fullyCleared = true; return; }

            auto& buf = delayBuffers[(size_t)clearCh];
            const int n = (int)buf.size();
            if (n <= 0) { ++clearCh; return; }

            const int end = juce::jmin(n, clearIdx + kChunk);
            for (int i = clearIdx; i < end; ++i)
                buf[(size_t)i] = 0.0f;

            clearIdx = end;
            if (clearIdx >= n)
            {
                clearIdx = 0;
                if (++clearCh >= NUM_CHANNELS) fullyCleared = true;
            }
        }

        float amtCoef = 0.008f;    // prepareToPlay でSRから算出 (τ≒20ms)
        bool  fullyCleared = true;
        int   clearCh = 0;
        int   clearIdx = 0;
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

            // 44.1kHz での旧固定値と一致する時定数を実時間で固定 (SR非依存化)
            //  attack 0.001  @44.1k → τ ≒ 22.7ms
            //  release 0.0002 @44.1k → τ ≒ 113ms
            duckAtkCoef = (float)(1.0 - std::exp(-1.0 / (0.0227 * sr)));
            duckRelCoef = (float)(1.0 - std::exp(-1.0 / (0.1134 * sr)));
        }

        void process(float& inOutL, float& inOutR, float amount, double bpm,
                     float timeBeats, float feedback, float duck, float damp) noexcept
        {
            if (delayBuffer.empty()) return;

            const float amountSmoothCoef = 1.0f - std::exp(-1.0f / (0.015f * (float)sampleRate));
            const float fbSmoothCoef = 1.0f - std::exp(-1.0f / (0.015f * (float)sampleRate));
            curAmount   += amountSmoothCoef * (amount - curAmount);
            curFeedback += fbSmoothCoef * (juce::jlimit(0.0f, 0.95f, feedback) - curFeedback);

            // 入力検波 (ダッキング用)。係数は prepareToPlay で実時間から算出する。
            //  旧実装は attack=0.001 / release=0.0002 のハードコードで、
            //  192kHz では 44.1kHz の 4.35倍遅い検波になっていた。
            const float inSum = std::abs(inOutL) + std::abs(inOutR);
            if (inSum > envelope) envelope += duckAtkCoef * (inSum - envelope);
            else                  envelope += duckRelCoef * (inSum - envelope);

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
        float duckAtkCoef = 0.001f, duckRelCoef = 0.0002f;  // prepareToPlayでSRから算出
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

        // 位相を1サンプル進めて現在のダッキングゲインを返す。
        //  Duckingは入力に依存しない純ゲインなので、ソース別ルーティングでは
        //  このゲインを各バスへ直接掛ければよい (合計は従来と厳密に一致する)。
        float nextGain(float amount, double bpm, float cycleBeats, float shape) noexcept
        {
            const double safeBpm = (bpm > 0.0) ? bpm : 120.0;
            phase += (float)((safeBpm / 60.0) / (sampleRate * (double)juce::jmax(0.0625f, cycleBeats)));
            if (phase >= 1.0f) phase -= 1.0f;

            // 拍頭で深く沈み、シェイプに従って復帰するポンプカーブ
            const float dip = std::pow(1.0f - phase, juce::jlimit(0.5f, 8.0f, shape));
            const float target = 1.0f - juce::jlimit(0.0f, 1.0f, amount) * 0.95f * dip;

            gainSm += (target < gainSm ? dipCoef : relCoef) * (target - gainSm);
            return gainSm;
        }

        void process(float& l, float& r, float amount, double bpm,
                     float cycleBeats, float shape) noexcept
        {
            const float g = nextGain(amount, bpm, cycleBeats, shape);
            l *= g;
            r *= g;
        }

    private:
        double sampleRate = 44100.0;
        float phase = 0.0f;
        float gainSm = 1.0f;
        float dipCoef = 0.02f, relCoef = 0.004f;
    };

    // ------------------------------------------
    // Beat Stutter (テンポ同期ビートリピート)
    //
    //  ライザーで最も使われるのに今まで無かった演出。
    //  グレイン(拍分割)を2つで1周期とし、
    //   前半 = 生音をそのまま通しつつバッファへ記録
    //   後半 = 直前に記録したグレインを繰り返す
    //  という構成にしている。「同じ長さのグレインをその場で繰り返す」だけだと
    //  入力と出力が一致して無音の変化しか起きないため、必ず半周期ずらす。
    //
    //  RATE をカーブで動かせば「終盤で刻みが細かくなる」定番の演出が一発で作れる。
    //  PPQ同期なのでグリッドから外れない。
    // ------------------------------------------
    class BeatStutter
    {
    public:
        void prepareToPlay(double sampleRate)
        {
            sr = juce::jmax(8000.0, sampleRate);
            size = juce::jmax(8192, (int)(sr * 2.0));   // 最大2秒 (1拍@30BPM まで)
            bufL.assign((size_t)size, 0.0f);
            bufR.assign((size_t)size, 0.0f);
            writePos = 0;
            phase = 0.0f;
            readOffset = 0;
            captureStart = 0;
            wasRepeat = false;
            curAmt = 0.0f;
            amtCoef = (float)(1.0 - std::exp(-1.0 / (0.004 * sr)));   // 4ms
            // グレイン境界のクロスフェード長 (3ms)。
            //  短すぎるとクリックが残り、長すぎると刻みの輪郭が甘くなる。
            fadeLen = (float)(sr * 0.003);
        }

        void reset() noexcept
        {
            std::fill(bufL.begin(), bufL.end(), 0.0f);
            std::fill(bufR.begin(), bufR.end(), 0.0f);
            writePos = 0; readOffset = 0; captureStart = 0;
            phase = 0.0f; wasRepeat = false; curAmt = 0.0f;
        }

        // ブロック頭でホストPPQへ位相同期 (2グレイン = 1周期)
        void syncTo(double ppq, float grainBeats) noexcept
        {
            const double cycle = (double)juce::jmax(0.03125f, grainBeats) * 2.0;
            phase = (float)(std::fmod(std::fmod(ppq, cycle) + cycle, cycle) / cycle);
        }

        void process(float& l, float& r, float amount, double bpm, float grainBeats) noexcept
        {
            if (bufL.empty()) return;

            curAmt += amtCoef * (amount - curAmt);

            const double safeBpm  = (bpm > 20.0 && bpm < 999.0) ? bpm : 120.0;
            const float  gBeats   = juce::jmax(0.03125f, grainBeats);
            const float  grainLen = juce::jlimit(32.0f, (float)(size / 2 - 2),
                                                 (float)(sr * 60.0 / safeBpm * (double)gBeats));

            // 入力は常に書き込む (AMT=0 でもバッファを新鮮に保つ)
            bufL[(size_t)writePos] = l;
            bufR[(size_t)writePos] = r;

            // 位相を1サンプル進める (1周期 = 2グレイン)
            phase += (float)((safeBpm / 60.0) / (sr * (double)gBeats * 2.0));
            if (phase >= 1.0f) phase -= 1.0f;

            const bool repeatPhase = (phase >= 0.5f);

            // 後半へ入った瞬間に「直前の1グレイン」を読み出し開始位置として確定
            if (repeatPhase && !wasRepeat)
            {
                captureStart = writePos - (int)grainLen;
                while (captureStart < 0) captureStart += size;
                readOffset = 0;
            }
            wasRepeat = repeatPhase;

            // ---- グレイン境界のクロスフェード ----
            //  生音 ⇔ バッファ再生をハードに切り替えると波形が不連続になり、
            //  AMT を上げるほど「プチプチ」というクリックになる。
            //  リピート区間の入口と出口に短いフェードを掛けて繋ぐ。
            //  隣接グレイン同士は元が同じライザーで相関が高いため、
            //  等パワーではなくリニアクロスフェードのほうが自然に繋がる。
            float blend = 0.0f;
            if (repeatPhase)
            {
                const float t = (phase - 0.5f) * 2.0f;                 // 区間内 0..1
                const float fadeFrac = juce::jlimit(0.02f, 0.45f, fadeLen / grainLen);
                blend = juce::jlimit(0.0f, 1.0f,
                                     juce::jmin(t, 1.0f - t) / fadeFrac);
            }

            const float g = curAmt * blend;

            if (g > 0.0005f)
            {
                int rp = captureStart + readOffset;
                while (rp >= size) rp -= size;
                l += (bufL[(size_t)rp] - l) * g;
                r += (bufR[(size_t)rp] - r) * g;
            }

            // 読み出し位置はリピート区間中だけ進める
            if (repeatPhase && ++readOffset >= (int)grainLen)
                readOffset = 0;

            if (++writePos >= size) writePos = 0;
        }

    private:
        double sr = 44100.0;
        int size = 0, writePos = 0, readOffset = 0, captureStart = 0;
        float phase = 0.0f, curAmt = 0.0f, amtCoef = 0.01f;
        float fadeLen = 128.0f;
        bool wasRepeat = false;
        std::vector<float> bufL, bufR;
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
// FxChain — 5スロット直列 (スロット=適用順序)
// ==========================================
class FxChain
{
public:
    FxChain() = default;

    static constexpr int kNumSlots = 5;
    static constexpr int kNumSources = 4;   // OSC1-3 + Noise (RiserEngine と対応)

    // ※ 末尾追加のみ (既存プリセットの fx*Type インデックス互換のため)
    enum FxType { None = 0, Saturation, Chorus, Delay, Reverb, Ducking, Stutter };
    static constexpr int kNumFxKinds = 6;   // None を除いた実効FX数

    static juce::StringArray getTypeNames()
    {
        return { "None", "Saturation", "Chorus", "Delay", "Reverb", "Ducking", "Stutter" };
    }
    static juce::StringArray getSatAlgoNames()
    {
        return { "Soft Tanh", "Hard Clip", "Triode", "Tape", "Transformer",
                 "JFET", "BJT", "Wavefold", "Exciter", "Cubic" };
    }
    static int satAlgoToType(int combo) noexcept
    {
        static constexpr int map[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10 };
        return map[juce::jlimit(0, 9, combo)];
    }

    // Delayタイム表 (拍数)
    static juce::StringArray getDelayTimeNames()
    {
        return { "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T" };
    }
    static float delayTimeToBeats(int idx) noexcept
    {
        static constexpr float beats[10] = { 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f,
                                         0.5f, 1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f };
        return beats[juce::jlimit(0, 9, idx)];
    }

    // Stutterのグレイン長 (Duckと同じ拍表を流用)
    static juce::StringArray getStutterRateNames() { return getDuckRateNames(); }
    static float stutterRateToBeats(int idx) noexcept { return duckRateToBeats(idx); }

    // Duckingレート表 (1サイクルの拍数, 1Bar〜1/64, 付点/三連対応)
    static juce::StringArray getDuckRateNames()
    {
        return { "1 Bar", "1/2.", "1/2", "1/2T", "1/4.", "1/4", "1/4T",
                 "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T",
                 "1/32.", "1/32", "1/32T", "1/64" };
    }
    static float duckRateToBeats(int idx) noexcept
    {
        static constexpr float beats[17] = {
            4.0f, 3.0f, 2.0f, 4.0f / 3.0f, 1.5f, 1.0f, 2.0f / 3.0f,
            0.75f, 0.5f, 1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f,
            0.1875f, 0.125f, 1.0f / 12.0f, 0.0625f };
        return beats[juce::jlimit(0, 16, idx)];
    }

    // パラメーターはFX毎 (AMT含む)。マルチENVカーブによる変調は
    // プロセッサー側で合成済みの値を渡す (ブロックレート更新+FX内部平滑)。
    struct Params
    {
        std::array<int, kNumSlots> type {};   // FxType (スロット=適用順序)
        double bpm = 120.0;
        double ppq = 0.0;
        bool   playing = false;

        // --- Saturation ---
        float satAmt = 0.0f;
        int   satAlgo = 0;
        float satDrive = 2.0f;      // 1..12
        float satPreHz = 20.0f;     // 20..2000
        float satTrimDb = 0.0f;     // -12..+12

        // --- Chorus ---
        float choAmt = 0.0f;
        float choRate = 0.8f;
        float choDepth = 0.5f;
        float choWidth = 1.0f;

        // --- Delay ---
        float dlyAmt = 0.0f;
        float dlyBeats = 0.5f;      // 合成済み拍数 (カーブで±2oct変調可)
        float dlyFeedback = 0.45f;
        float dlyDuck = 0.5f;
        float dlyDamp = 0.3f;

        // --- Reverb ---
        float revAmt = 0.0f;
        float revDecay = 0.7f;
        float revShimmer = 0.4f;
        float revDamp = 0.3f;
        float revMod = 0.4f;

        // --- Stutter ---
        float stutAmt = 0.0f;
        float stutBeats = 0.25f;    // 合成済みグレイン長 (拍)

        // --- Ducking ---
        float duckAmt = 0.0f;
        float duckBeats = 1.0f;     // 合成済み拍数
        float duckShape = 2.0f;     // 0.5..8

        // --- エフェクト種別ごとのソース別ルーティング ---
        //  route[効果][ソース] : 効果 = 0:Sat 1:Cho 2:Dly 3:Rev 4:Duck 5:Stutter
        //                       ソース = 0:OSC1 1:OSC2 2:OSC3 3:Noise
        //  false のソースはそのエフェクトを完全にバイパスして素通しする。
        std::array<std::array<bool, kNumSources>, kNumFxKinds> route {{
            { true, true, true, true }, { true, true, true, true },
            { true, true, true, true }, { true, true, true, true },
            { true, true, true, true }, { true, true, true, true } }};
    };

    void prepare(double sr)
    {
        sampleRate = sr;
        chorus.prepareToPlay(sr);
        delay.prepareToPlay(sr);
        reverb.prepareToPlay(sr);
        ducker.prepareToPlay(sr);
        stutter.prepareToPlay(sr);
        dcCoef = std::exp((float)(-1.0 / (0.004523 * sr)));
        for (int ch = 0; ch < 2; ++ch) { satState[ch].reset(); dcState[ch] = 0.0f; }

        // カーブ変調されるパラメーターのサンプル単位平滑 (τ≒10ms)
        //  ブロックレート更新による段差 (ジッパーノイズ) を除去する。
        //  ※ Delayのamt/fb/time、ReverbのcurAmount、DuckerのgainSm は各FX内部で平滑済み。
        modSmCoef = 1.0f - std::exp((float)(-1.0 / (0.010 * sr)));
        stutAmtSm = 0.0f;
        satAmtSm = satDriveSm = 0.0f;
        choAmtSm = choDepthSm = 0.0f;
        revAmtSm = revShimSm = 0.0f;
        duckAmtSm = duckShapeSm = 0.0f;
        modSmInit = false;
    }

    // ==========================================================
    // ソース別ルーティング対応の処理 (v0.4.1)
    //
    //  busL/busR : OSC1 / OSC2 / OSC3 / Noise の4系統ステレオバス (in-place処理)
    //
    //  設計:
    //   エフェクトのモジュール実体は各1個のまま共有する。ソース毎に独立した
    //   FXチェーンを持つとShimmerReverbだけで96kHz時11.7MB×4となり非現実的。
    //   代わりに「ルーティングされたバスの合計」をモジュールへ通し、
    //   モジュールが加えた変化量(差分)を対象バスへ均等配分して書き戻す。
    //
    //   ・全ソースONのとき、バス合計は従来の単一信号処理と数値的に一致する
    //     (既存プリセットの音が変わらない)
    //   ・Duckingは入力非依存の純ゲインなので、差分ではなくゲインを
    //     各バスへ直接掛ける (こちらの方が下流ルーティングとの相性が良い)
    //   ・スロット間ソフトクリップも「合計」に対して判定し、
    //     求まったゲインを全バスへ配分する (同じく従来と一致)
    // ==========================================================
    void process(float* const* busL, float* const* busR, int numSamples, const Params& p) noexcept
    {
        if (numSamples <= 0 || busL == nullptr || busR == nullptr) return;

        // 全スロット None のときのみ早期リターン
        // (amt==0 でも Delay/Reverb の内部バッファ更新は継続する)
        bool anyTyped = false;
        for (int s = 0; s < kNumSlots; ++s)
            if (p.type[(size_t)s] > 0) anyTyped = true;
        if (!anyTyped) return;

        const int satType = satAlgoToType(p.satAlgo);
        const float trimGain = juce::Decibels::decibelsToGain(juce::jlimit(-12.0f, 12.0f, p.satTrimDb));
        const float preAlpha = 1.0f / (1.0f + juce::MathConstants<float>::twoPi
                                       * juce::jlimit(20.0f, 2000.0f, p.satPreHz) / (float)sampleRate);

        // Ducking: 再生中はブロック頭でPPQへ位相同期
        if (p.playing)
        {
            ducker.syncTo(p.ppq, p.duckBeats);
            stutter.syncTo(p.ppq, p.stutBeats);
        }

        // スロット間ソフトクリップ
        auto interSlotClip = [](float x) noexcept -> float
        {
            const float t = 1.2f;
            const float a = std::abs(x);
            if (a <= t) return x;
            const float s = x < 0.0f ? -1.0f : 1.0f;
            return s * (t + (1.0f - t * 0.25f) * std::tanh((a - t) * 2.0f));
        };

        // 平滑の初期化 (最初のブロックはターゲットへスナップ)
        if (!modSmInit)
        {
            modSmInit = true;
            satAmtSm = p.satAmt;   satDriveSm = p.satDrive;
            choAmtSm = p.choAmt;   choDepthSm = p.choDepth;
            revAmtSm = p.revAmt;   revShimSm = p.revShimmer;
            duckAmtSm = p.duckAmt; duckShapeSm = p.duckShape;
            stutAmtSm = p.stutAmt;
        }

        // ---- スロットの解決をブロック先頭で1回だけ済ませる ----
        //  旧実装はサンプルごとに p.type[] の読み出し・ルーティング配列の解決・
        //  ソース本数のカウントを行っていた。これらはブロック内で不変なので、
        //  ここでまとめて求めておく (音は一切変わらない)。
        struct SlotPlan
        {
            int   type = 0;                       // FxType (0 = None)
            const bool* route = nullptr;          // 対象ソース [kNumSources]
            int   n = 0;                          // 対象ソース本数
            float inv = 0.0f;                     // 1/n (n>0 のとき)
        };
        std::array<SlotPlan, kNumSlots> plan {};
        for (int s = 0; s < kNumSlots; ++s)
        {
            const int t = p.type[(size_t)s];
            if (t <= 0) continue;
            const auto& rt = p.route[(size_t)juce::jlimit(0, kNumFxKinds - 1, t - 1)];
            int n = 0;
            for (int k = 0; k < kNumSources; ++k)
                if (rt[(size_t)k]) ++n;

            plan[(size_t)s].type  = t;
            plan[(size_t)s].route = rt.data();
            plan[(size_t)s].n     = n;
            plan[(size_t)s].inv   = (n > 0) ? 1.0f / (float)n : 0.0f;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            // カーブ変調パラメーターのサンプル単位平滑
            satAmtSm    += modSmCoef * (p.satAmt - satAmtSm);
            satDriveSm  += modSmCoef * (p.satDrive - satDriveSm);
            choAmtSm    += modSmCoef * (p.choAmt - choAmtSm);
            choDepthSm  += modSmCoef * (p.choDepth - choDepthSm);
            revAmtSm    += modSmCoef * (p.revAmt - revAmtSm);
            revShimSm   += modSmCoef * (p.revShimmer - revShimSm);
            duckAmtSm   += modSmCoef * (p.duckAmt - duckAmtSm);
            duckShapeSm += modSmCoef * (p.duckShape - duckShapeSm);
            stutAmtSm   += modSmCoef * (p.stutAmt - stutAmtSm);

            for (int s = 0; s < kNumSlots; ++s)
            {
                const auto& pl = plan[(size_t)s];
                if (pl.type > 0)
                {
                    const bool* rt = pl.route;

                    // ルーティング対象バスの合計を作る
                    float inL = 0.0f, inR = 0.0f;
                    for (int k = 0; k < kNumSources; ++k)
                        if (rt[k]) { inL += busL[k][i]; inR += busR[k][i]; }

                    if (pl.type == Ducking)
                    {
                        // 純ゲイン: 対象バスへ直接適用 (対象が0本でも位相は進める)
                        const float g = ducker.nextGain(duckAmtSm, p.bpm, p.duckBeats, duckShapeSm);
                        for (int k = 0; k < kNumSources; ++k)
                            if (rt[k]) { busL[k][i] *= g; busR[k][i] *= g; }
                    }
                    else
                    {
                        // 対象が0本でも in=0 でモジュールを回し、内部バッファ/LFOを
                        // 進め続ける (陳腐化バースト防止。従来の設計方針を踏襲)
                        float oL = inL, oR = inR;
                        switch (pl.type)
                        {
                        case Saturation:
                            if (satAmtSm > 0.0005f)
                                saturate(oL, oR, satAmtSm, satType, satDriveSm, preAlpha, trimGain);
                            break;
                        case Chorus:
                            chorus.process(oL, oR, choAmtSm, p.choRate, choDepthSm, p.choWidth);
                            break;
                        case Delay:
                            delay.process(oL, oR, p.dlyAmt, p.bpm, p.dlyBeats, p.dlyFeedback, p.dlyDuck, p.dlyDamp);
                            break;
                        case Reverb:
                            reverb.process(oL, oR, revAmtSm, p.revDecay, revShimSm, p.revDamp, p.revMod);
                            break;
                        case Stutter:
                            stutter.process(oL, oR, stutAmtSm, p.bpm, p.stutBeats);
                            break;
                        default: break;
                        }

                        if (pl.n > 0)
                        {
                            const float dLd = (oL - inL) * pl.inv;
                            const float dRd = (oR - inR) * pl.inv;
                            for (int k = 0; k < kNumSources; ++k)
                                if (rt[k]) { busL[k][i] += dLd; busR[k][i] += dRd; }
                        }
                    }
                }

                // スロット間ソフトクリップ: 合計に対して判定し、ゲインを全バスへ配分
                if (s < kNumSlots - 1)
                {
                    float sL = 0.0f, sR = 0.0f;
                    for (int k = 0; k < kNumSources; ++k) { sL += busL[k][i]; sR += busR[k][i]; }

                    const float cL = interSlotClip(sL);
                    const float cR = interSlotClip(sR);
                    if (cL != sL || cR != sR)
                    {
                        const float gL = (std::abs(sL) > 1.0e-12f) ? cL / sL : 1.0f;
                        const float gR = (std::abs(sR) > 1.0e-12f) ? cR / sR : 1.0f;
                        for (int k = 0; k < kNumSources; ++k)
                        {
                            busL[k][i] *= gL;
                            busR[k][i] *= gR;
                        }
                    }
                }
            }
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
    lfx::ShimmerReverb reverb;
    lfx::BeatDucker ducker;
    lfx::BeatStutter stutter;

    lfx::SaturationState satState[2];
    float dcState[2] = { 0.0f, 0.0f };
    float preX1[2] = { 0.0f, 0.0f };
    float preY1[2] = { 0.0f, 0.0f };
    float dcCoef = 0.995f;

    // カーブ変調パラメーターのサンプル単位平滑状態
    float modSmCoef = 0.002f;
    bool  modSmInit = false;
    float satAmtSm = 0.0f, satDriveSm = 2.0f;
    float choAmtSm = 0.0f, choDepthSm = 0.5f;
    float revAmtSm = 0.0f, revShimSm = 0.4f;
    float duckAmtSm = 0.0f, duckShapeSm = 2.0f;
    float stutAmtSm = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxChain)
};
