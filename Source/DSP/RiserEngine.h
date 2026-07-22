// ==========================================
// File: RiserEngine.h
// ライザーシンセコア (計画書フェーズ2準拠)
//
//  - 3オシレーター (MorphWavetable: Sine→Tri→Square→Saw→FM + カスタムWT)
//    + ノイズ (White/Pink/Brown → 専用TPTバンドパスでピッチスイープ)
//  - DAWトランスポート同期 (選択案A): ノートオンをトリガーに AudioPlayHead の
//    PPQ を基準として指定Bar数 (1,2,4,8,16) で Progress 0.0→1.0 を進める。
//    ホスト停止中は BPM ベースの内部クロックへ自動フォールバック。
//  - トリガーはノート保持型: Progress完走後ホールド、ノートオフでリリース。
//  - 9系統マルチENVカーブ (CurveStore) を32サンプル毎のコントロールティックで
//    評価し、サンプル精度の一次平滑でジッパーノイズを排除。
//  - SoAデータレイアウト (phase[osc][voice] 等の連続配列) のタイトループで
//    自動ベクトル化を促す (SPECTRA8同様のスカラー設計)。
//  - render() 内のメモリアロケーション/ロックは一切無し。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

#include "Wavetable.h"
#include "CurveData.h"
#include "ZdfFilter.h"

class RiserEngine
{
public:
    static constexpr int kNumOscs = 3;
    static constexpr int kMaxUnison = 7;
    static constexpr int kNumFilters = 4;
    static constexpr int kCtrlInterval = 32; // コントロールレート (サンプル)

    // ---- ブロック毎にプロセッサーが収集して渡すパラメーター (POD) ----
    struct Params
    {
        float lift = 1.0f;           // マスターLIFTノブ (全カーブ強度)

        struct Osc
        {
            bool  on = false;
            float wave = 0.75f;      // 0..1 モーフ位置
            float level = 0.8f;
            float coarse = 0.0f;     // 半音
            int   unison = 1;        // 1..7
            float detune = 12.0f;    // cents
            float spread = 0.7f;     // 0..1
            float range = 24.0f;     // ピッチカーブ ±半音レンジ
        };
        std::array<Osc, kNumOscs> osc;

        int   noiseType = 0;         // 0=White 1=Pink 2=Brown
        float noiseLevel = 0.0f;
        float noisePitch = 500.0f;   // BP中心周波数の基準 (Hz)
        float noiseRes = 2.0f;
        float noiseRangeOct = 5.0f;  // ノイズピッチカーブ ±オクターブレンジ

        struct Flt
        {
            bool  on = false;
            int   type = 0;          // TptSvf::Type
            float cutoff = 1000.0f;
            float res = 0.9f;
            float env = 0.0f;        // -1..+1 (カーブ適用量, ±5oct)
        };
        std::array<Flt, kNumFilters> flt;

        float attackMs = 3.0f;
        float releaseMs = 200.0f;
    };

    RiserEngine() = default;

    void setWavetable(const MorphWavetable* wt) noexcept { wavetable = wt; }

    // ---- prepareToPlay: 事前アロケーション/係数計算のみ ----
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& f : filters) f.prepare(sampleRate);
        noiseFilter.prepare(sampleRate);
        noiseFilter.setType(TptSvf::BandPass);
        pitchCoef = 1.0f - std::exp(-1.0f / (0.004f * (float)sr)); // τ≒4ms
        hardReset();
    }

    void hardReset() noexcept
    {
        noteHeld = false;
        curNote = -1;
        ampEnv = 0.0f;
        progress = 0.0;
        progInc = 0.0;
        hostSync = false;
        ctrlCount = 0;
        fxEnvSm = 0.0f;
        for (auto& po : phase) po.fill(0.0f);
        for (auto& ps : pitchSm) ps = 0.0f;
        for (auto& f : filters) f.reset();
        noiseFilter.reset();
        pinkB.fill(0.0f);
        brownState = 0.0f;
        uiProgress.store(0.0f, std::memory_order_relaxed);
    }

    // ---- MIDI (processBlock 冒頭で呼ばれる) ----
    void noteOn(int note, float velocity, double ppqNow, bool hostPlaying) noexcept
    {
        curNote = note;
        noteHeld = true;
        hostSync = hostPlaying;
        startPpq = ppqNow;
        progress = 0.0;
        velGain = 0.25f + 0.75f * juce::jlimit(0.0f, 1.0f, velocity);
        // ユニゾン位相を軽くばらしてコムを回避 (決定的・アロケ無し)
        for (int o = 0; o < kNumOscs; ++o)
            for (int v = 0; v < kMaxUnison; ++v)
                phase[(size_t)o][(size_t)v] = std::fmod(0.137f * (float)(v + 1) * (float)(o + 1), 1.0f);
    }

    void noteOff(int note) noexcept
    {
        if (note == curNote)
            noteHeld = false;
    }

    void allNotesOff() noexcept { noteHeld = false; }

    // ---- トランスポート同期 (ブロック毎・render前に呼ぶ) ----
    //  bars: 1,2,4,8,16 / qnPerBar: 拍子から求めた1小節の4分音符数
    void syncTransport(bool playing, bool hasPpq, double ppq,
                       double bpm, double qnPerBar, int bars) noexcept
    {
        const double safeBpm = (bpm > 20.0 && bpm < 999.0) ? bpm : 120.0;
        totalQn = juce::jmax(0.25, (double)bars * qnPerBar);
        progInc = (safeBpm / 60.0) / (sr * totalQn); // 1サンプルあたりのProgress

        if (noteHeld && hostSync)
        {
            if (playing && hasPpq)
            {
                // PPQ絶対時間からProgressを再同期 (ドリフト無し・ループ/ジャンプ耐性)
                progress = juce::jlimit(0.0, 1.0, (ppq - startPpq) / totalQn);
            }
            else
            {
                hostSync = false; // 再生停止 → 内部クロックで続行
            }
        }
    }

    bool isNoteActive() const noexcept { return noteHeld || ampEnv > 1.0e-4f; }
    float getFxEnvValue() const noexcept { return fxEnvSm; }

    // GUI用 (VBlankアニメーション)
    std::atomic<float> uiProgress { 0.0f };

    // ---- レンダリング (加算ミックス。L/R は事前クリア済みバッファ) ----
    void render(float* outL, float* outR, int numSamples, const Params& p,
                const CurveStore& curves) noexcept
    {
        if (wavetable == nullptr || numSamples <= 0) return;

        // 無音時は完全スキップ (リリース完了後にProgressをリセット)
        if (!noteHeld && ampEnv <= 1.0e-4f)
        {
            if (progress > 0.0) { progress = 0.0; uiProgress.store(0.0f, std::memory_order_relaxed); }
            ampEnv = 0.0f;
            return;
        }

        const float attCoef = 1.0f - std::exp(-1.0f / (juce::jmax(0.1f, p.attackMs) * 0.001f * (float)sr));
        const float relCoef = 1.0f - std::exp(-1.0f / (juce::jmax(1.0f, p.releaseMs) * 0.001f * (float)sr));
        const float baseHz = 440.0f * std::exp2(((float)curNote - 69.0f) / 12.0f);
        const float invSr = 1.0f / (float)sr;

        for (int i = 0; i < numSamples; ++i)
        {
            // ---- コントロールティック (32サンプル毎) ----
            if (ctrlCount == 0)
                controlTick(p, curves);
            ctrlCount = (ctrlCount + 1) % kCtrlInterval;

            // ---- Progress 前進 (ホールド型: 1.0で停止) ----
            if (noteHeld && progress < 1.0)
            {
                progress += progInc;
                if (progress > 1.0) progress = 1.0;
            }

            float l = 0.0f, r = 0.0f;

            // ---- オシレーター (SoA: phase[osc][voice] 連続アクセス) ----
            for (int o = 0; o < kNumOscs; ++o)
            {
                const auto& po = p.osc[(size_t)o];
                if (!po.on || po.level <= 0.0001f) continue;

                // サンプル精度のピッチ平滑 (ジッパーノイズ対策)
                pitchSm[(size_t)o] += pitchCoef * (pitchTarget[(size_t)o] - pitchSm[(size_t)o]);

                const float freq = baseHz * std::exp2((po.coarse + pitchSm[(size_t)o]) / 12.0f);
                const float inc0 = freq * invSr;
                const int uni = juce::jlimit(1, kMaxUnison, po.unison);
                const float norm = po.level / std::sqrt((float)uni);

                float* ph = phase[(size_t)o].data();
                const float* cf = centsFac[(size_t)o].data();
                const float* gl = gainL[(size_t)o].data();
                const float* gr = gainR[(size_t)o].data();

                for (int v = 0; v < uni; ++v)
                {
                    float inc = inc0 * cf[v];
                    if (inc > 0.45f) inc = 0.45f; // 超高域の暴走防止
                    float pv = ph[v] + inc;
                    if (pv >= 1.0f) pv -= 1.0f;
                    ph[v] = pv;
                    const float s = wavetable->sample(pv, po.wave, inc) * norm;
                    l += s * gl[v];
                    r += s * gr[v];
                }
            }

            // ---- ノイズ (BPフィルターでピッチスイープ) ----
            if (p.noiseLevel > 0.0001f)
            {
                const float nz = nextNoise(p.noiseType) * p.noiseLevel;
                float nl = nz, nr = nz;
                noiseFilter.processStereo(nl, nr);
                l += nl;
                r += nr;
            }

            // ---- 4系統 ZDF/TPT フィルター (直列) ----
            for (int j = 0; j < kNumFilters; ++j)
                if (p.flt[(size_t)j].on)
                    filters[(size_t)j].processStereo(l, r);

            // ---- アンプエンベロープ (ノート保持型) ----
            const float target = noteHeld ? 1.0f : 0.0f;
            ampEnv += (noteHeld ? attCoef : relCoef) * (target - ampEnv);

            const float g = ampEnv * velGain;
            outL[i] += l * g;
            outR[i] += r * g;
        }

        uiProgress.store((float)progress, std::memory_order_relaxed);
    }

private:
    // ---- コントロールティック: カーブ評価とターゲット更新 ----
    void controlTick(const Params& p, const CurveStore& curves) noexcept
    {
        const float prog = (float)progress;
        const float lift = juce::jlimit(0.0f, 1.0f, p.lift);

        auto bipolar = [lift](float y) noexcept { return (0.5f + (y - 0.5f) * lift - 0.5f) * 2.0f; }; // -1..1

        // ピッチカーブ (Osc1-3)
        for (int o = 0; o < kNumOscs; ++o)
        {
            const float y = curves.read(CurveStore::PitchOsc1 + o).evaluate(prog);
            pitchTarget[(size_t)o] = bipolar(y) * p.osc[(size_t)o].range;

            // ユニゾンのデチューン係数とパンゲイン (等パワー)
            const int uni = juce::jlimit(1, kMaxUnison, p.osc[(size_t)o].unison);
            for (int v = 0; v < kMaxUnison; ++v)
            {
                const float off = (uni <= 1) ? 0.0f : (2.0f * (float)v / (float)(uni - 1) - 1.0f);
                centsFac[(size_t)o][(size_t)v] = std::exp2(off * p.osc[(size_t)o].detune / 1200.0f);
                const float pan = 0.5f + off * 0.5f * p.osc[(size_t)o].spread;
                const float th = pan * juce::MathConstants<float>::halfPi;
                gainL[(size_t)o][(size_t)v] = std::cos(th);
                gainR[(size_t)o][(size_t)v] = std::sin(th);
            }
        }

        // ノイズピッチカーブ → BP中心周波数
        {
            const float y = curves.read(CurveStore::PitchNoise).evaluate(prog);
            const float target = p.noisePitch * std::exp2(bipolar(y) * p.noiseRangeOct);
            noiseCutSm += 0.5f * (target - noiseCutSm);
            noiseFilter.setCoef(noiseCutSm, p.noiseRes);
        }

        // フィルターカーブ → カットオフ (±5オクターブ)
        for (int j = 0; j < kNumFilters; ++j)
        {
            if (!p.flt[(size_t)j].on) continue;
            const float y = curves.read(CurveStore::Filter1 + j).evaluate(prog);
            const float target = p.flt[(size_t)j].cutoff
                               * std::exp2(p.flt[(size_t)j].env * bipolar(y) * 5.0f);
            cutSm[(size_t)j] += 0.5f * (target - cutSm[(size_t)j]);
            filters[(size_t)j].setType(p.flt[(size_t)j].type);
            filters[(size_t)j].setCoef(cutSm[(size_t)j], p.flt[(size_t)j].res);
        }

        // FXカーブ (ユニポーラ 0..1)
        {
            const float y = curves.read(CurveStore::FxCurve).evaluate(prog) * lift;
            fxEnvSm += 0.3f * (y - fxEnvSm);
        }
    }

    // ---- ノイズジェネレーター (xorshift + Kellett Pink + 漏れ積分Brown) ----
    inline float nextNoise(int type) noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        const float w = ((float)(rngState & 0xffffff) / 8388608.0f) - 1.0f; // -1..1

        switch (type)
        {
        case 1: // Pink (Paul Kellett)
        {
            pinkB[0] = 0.99886f * pinkB[0] + w * 0.0555179f;
            pinkB[1] = 0.99332f * pinkB[1] + w * 0.0750759f;
            pinkB[2] = 0.96900f * pinkB[2] + w * 0.1538520f;
            pinkB[3] = 0.86650f * pinkB[3] + w * 0.3104856f;
            pinkB[4] = 0.55000f * pinkB[4] + w * 0.5329522f;
            pinkB[5] = -0.7616f * pinkB[5] - w * 0.0168980f;
            const float pink = pinkB[0] + pinkB[1] + pinkB[2] + pinkB[3]
                             + pinkB[4] + pinkB[5] + pinkB[6] + w * 0.5362f;
            pinkB[6] = w * 0.115926f;
            return pink * 0.11f;
        }
        case 2: // Brown (漏れ積分)
            brownState = 0.995f * brownState + w * 0.05f;
            return brownState * 3.0f;
        default: // White
            return w;
        }
    }

    // ---- 状態 ----
    const MorphWavetable* wavetable = nullptr;
    double sr = 44100.0;

    bool noteHeld = false;
    bool hostSync = false;
    int curNote = -1;
    float velGain = 1.0f;
    double startPpq = 0.0;
    double totalQn = 16.0;
    double progress = 0.0;
    double progInc = 0.0;

    float ampEnv = 0.0f;
    int ctrlCount = 0;

    // SoA レイアウト
    std::array<std::array<float, kMaxUnison>, kNumOscs> phase {};
    std::array<std::array<float, kMaxUnison>, kNumOscs> centsFac {};
    std::array<std::array<float, kMaxUnison>, kNumOscs> gainL {};
    std::array<std::array<float, kMaxUnison>, kNumOscs> gainR {};

    std::array<float, kNumOscs> pitchTarget {};
    std::array<float, kNumOscs> pitchSm {};
    float pitchCoef = 0.01f;

    std::array<TptSvf, kNumFilters> filters;
    std::array<float, kNumFilters> cutSm { 1000.0f, 1000.0f, 1000.0f, 1000.0f };

    TptSvf noiseFilter;
    float noiseCutSm = 500.0f;

    float fxEnvSm = 0.0f;

    juce::uint32 rngState = 0x9e3779b9;
    std::array<float, 7> pinkB {};
    float brownState = 0.0f;

    JUCE_DECLARE_NON_COPYABLE(RiserEngine)
};
