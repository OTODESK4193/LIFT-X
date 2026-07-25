// ==========================================
// File: RiserEngine.h
// ライザーシンセコア (v0.2)
//
//  - 3オシレーター: WAVE選択 (ビルトイン Sine/Tri/Square/Saw/FM または
//    OSC毎のカスタムWavetable + POSITIONノブ)
//  - ピッチは StartKey→EndKey の絶対指定。MIDIノートはトリガー専用で、
//    Pitch ENVカーブ (下=StartKey / 上=EndKey) がピッチを決定する。
//  - ソース毎 (OSC1-3/Noise) の SOLO/MUTE
//  - モジュレーションENV: Level/Detune/Spread/(Noise Res) を
//    バイポーラ加算式 (中央=ノブ値, ±レンジ半分) で変調
//  - フィルター4系統 (ZDF/TPT) × ソース別ルーティング:
//    各フィルターにつき OSC1-3/Noise を個別に通す/バイパスできるため、
//    フィルター状態は [フィルター4][ソース4] の16基を保持
//  - DAWトランスポート同期 (PPQ絶対時間, Bar数 1-16) / ノート保持型トリガー
//  - render() 内のメモリアロケーション/ロックは一切無し
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
    static constexpr int kNumSources = 4;   // OSC1-3 + Noise
    static constexpr int kMaxUnison = 7;
    static constexpr int kNumFilters = 4;
    static constexpr int kCtrlInterval = 32;

    // WAVEコンボの並び (0-4=ビルトイン, 5=カスタムWT)
    enum WaveMode { Sine = 0, Triangle, Square, Saw, FM, CustomWT };

    // ---- ブロック毎にプロセッサーが収集して渡すパラメーター (POD) ----
    struct Params
    {
        // LIFT = 全マルチENVの評価位置 (カーブのX座標)
        //  Manual: ノブ値が評価位置。動かさない限りENVは変化しない
        //  Auto  : Progress(0→1)が評価位置。ライザーとして自動進行
        float lift = 1.0f;
        bool  liftAuto = false;

        struct Osc
        {
            bool  on = false;
            bool  solo = false;
            bool  mute = false;
            int   waveMode = Saw;    // WaveMode
            float pos = 0.0f;        // WTポジション (CustomWT時のみ有効)
            float level = 0.8f;
            float coarse = 0.0f;     // 半音 (Start/EndKeyへのオフセット)
            int   unison = 1;
            float detune = 12.0f;    // cents
            float spread = 0.7f;
            int   keyStart = 36;     // C2
            int   keyEnd = 84;       // C6
        };
        std::array<Osc, kNumOscs> osc;

        bool  noiseSolo = false;
        bool  noiseMute = false;
        int   noiseType = 0;
        float noiseLevel = 0.0f;
        float noisePitch = 500.0f;
        float noiseRes = 2.0f;
        float noiseRangeOct = 5.0f;

        struct Flt
        {
            bool  on = false;
            int   type = 0;
            float cutoff = 1000.0f;
            float res = 0.9f;
            float env = 0.0f;                     // -1..+1 (±5oct)
            std::array<bool, kNumSources> route { true, true, true, true }; // ソース別ルーティング
        };
        std::array<Flt, kNumFilters> flt;

        float attackMs = 3.0f;
        float releaseMs = 200.0f;
    };

    RiserEngine() = default;

    void setWavetable(int osc, const MorphWavetable* wt) noexcept
    {
        if (osc >= 0 && osc < kNumOscs)
            wavetables[(size_t)osc] = wt;
    }

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& row : filters)
            for (auto& f : row)
                f.prepare(sampleRate);
        noiseFilter.prepare(sampleRate);
        noiseFilter.setType(TptSvf::BandPass);
        smCoef = 1.0f - std::exp(-1.0f / (0.004f * (float)sr));      // τ≒4ms (サンプル単位平滑)
        declickCoef = 1.0f - std::exp(-1.0f / (0.002f * (float)sr)); // τ≒2ms (リトリガーデクリック)
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
        snapNext = true;
        declickGain = 1.0f;
        liftSm = 1.0f;
        for (auto& po : phase) po.fill(0.0f);
        pitchSm.fill(60.0f);
        levelSm.fill(0.0f);
        pitchTarget.fill(60.0f);
        levelTarget.fill(0.0f);
        posSm.fill(0.0f);
        detSm.fill(12.0f);
        sprSm.fill(0.7f);
        resSm.fill(0.9f);
        noiseResSm = 2.0f;
        for (auto& row : filters)
            for (auto& f : row)
                f.reset();
        noiseFilter.reset();
        pinkB.fill(0.0f);
        brownState = 0.0f;
        uiProgress.store(0.0f, std::memory_order_relaxed);
    }

    // ---- MIDI ----
    void noteOn(int note, float velocity, double ppqNow, bool hostPlaying) noexcept
    {
        // リトリガー時 (発音中の再ノートオン) は位相リセットの不連続を
        // 2msのデクリックランプで隠す (ブチ切れ/クリック対策)
        if (ampEnv > 0.02f)
            declickGain = 0.0f;
        else
        {
            // 完全な新規発音: フィルターの残留状態をクリア (前回の残響リング防止)
            for (auto& row : filters)
                for (auto& f : row)
                    f.reset();
            noiseFilter.reset();
        }

        curNote = note;
        noteHeld = true;
        hostSync = hostPlaying;
        startPpq = ppqNow;
        progress = 0.0;
        velGain = 0.25f + 0.75f * juce::jlimit(0.0f, 1.0f, velocity);
        ctrlCount = 0;      // 次サンプルで即コントロールティック
        snapNext = true;    // 平滑をターゲットへスナップ (古い値からのグライド防止)
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

    // ---- トランスポート同期 (ブロック毎・render前) ----
    //  bars: 1/32〜16小節 (小数対応)
    void syncTransport(bool playing, bool hasPpq, double ppq,
                       double bpm, double qnPerBar, double bars) noexcept
    {
        const double safeBpm = (bpm > 20.0 && bpm < 999.0) ? bpm : 120.0;
        totalQn = juce::jmax(0.03125, bars * qnPerBar);
        progInc = (safeBpm / 60.0) / (sr * totalQn);

        if (noteHeld && hostSync)
        {
            if (playing && hasPpq)
                progress = juce::jlimit(0.0, 1.0, (ppq - startPpq) / totalQn);
            else
                hostSync = false;
        }
    }

    bool isNoteActive() const noexcept { return noteHeld || ampEnv > 1.0e-4f; }
    float getProgressF() const noexcept { return (float)progress; }

    std::atomic<float> uiProgress { 0.0f };

    // ---- レンダリング (L/R は加算ミックス) ----
    void render(float* outL, float* outR, int numSamples, const Params& p,
                const CurveStore& curves) noexcept
    {
        if (numSamples <= 0) return;

        if (!noteHeld && ampEnv <= 1.0e-4f)
        {
            if (progress > 0.0) { progress = 0.0; uiProgress.store(0.0f, std::memory_order_relaxed); }
            ampEnv = 0.0f;
            return;
        }

        // SOLO判定 (いずれかのソースがSOLOなら、SOLO以外は無効)
        bool anySolo = p.noiseSolo;
        for (const auto& o : p.osc) anySolo = anySolo || o.solo;
        std::array<bool, kNumSources> active {};
        for (int o = 0; o < kNumOscs; ++o)
            active[(size_t)o] = p.osc[(size_t)o].on && !p.osc[(size_t)o].mute
                                && (!anySolo || p.osc[(size_t)o].solo);
        active[3] = p.noiseLevel > 0.0001f && !p.noiseMute && (!anySolo || p.noiseSolo);

        const float attCoef = 1.0f - std::exp(-1.0f / (juce::jmax(0.1f, p.attackMs) * 0.001f * (float)sr));
        const float relCoef = 1.0f - std::exp(-1.0f / (juce::jmax(1.0f, p.releaseMs) * 0.001f * (float)sr));
        const float invSr = 1.0f / (float)sr;

        for (int i = 0; i < numSamples; ++i)
        {
            if (ctrlCount == 0)
                controlTick(p, curves);
            ctrlCount = (ctrlCount + 1) % kCtrlInterval;

            if (noteHeld && progress < 1.0)
            {
                progress += progInc;
                if (progress > 1.0) progress = 1.0;
            }

            float l = 0.0f, r = 0.0f;

            // ---- OSC1-3 (各ソース独立にフィルタールーティング) ----
            for (int o = 0; o < kNumOscs; ++o)
            {
                if (!active[(size_t)o]) continue;
                const MorphWavetable* wt = wavetables[(size_t)o];
                if (wt == nullptr) continue;

                const auto& po = p.osc[(size_t)o];

                pitchSm[(size_t)o] += smCoef * (pitchTarget[(size_t)o] - pitchSm[(size_t)o]);
                levelSm[(size_t)o] += smCoef * (levelTarget[(size_t)o] - levelSm[(size_t)o]);
                if (levelSm[(size_t)o] <= 0.0002f && levelTarget[(size_t)o] <= 0.0001f) continue;

                const float freq = 440.0f * std::exp2((pitchSm[(size_t)o] - 69.0f) / 12.0f);
                const float inc0 = freq * invSr;
                const int uni = juce::jlimit(1, kMaxUnison, po.unison);
                const float norm = levelSm[(size_t)o] / std::sqrt((float)uni);

                // POSITION はサンプル単位平滑 (ノブ操作時のジッパー防止)
                posSm[(size_t)o] += smCoef * (po.pos - posSm[(size_t)o]);
                const bool useCustom = (po.waveMode == CustomWT);
                const float morph = useCustom ? posSm[(size_t)o] : (float)po.waveMode * 0.25f;

                float* ph = phase[(size_t)o].data();
                const float* cf = centsFac[(size_t)o].data();
                const float* gl = gainL[(size_t)o].data();
                const float* gr = gainR[(size_t)o].data();

                float lo = 0.0f, ro = 0.0f;
                for (int v = 0; v < uni; ++v)
                {
                    float inc = inc0 * cf[v];
                    if (inc > 0.45f) inc = 0.45f;
                    float pv = ph[v] + inc;
                    if (pv >= 1.0f) pv -= 1.0f;
                    ph[v] = pv;
                    const float s = wt->sample(pv, morph, inc, useCustom) * norm;
                    lo += s * gl[v];
                    ro += s * gr[v];
                }

                // ソース別フィルターチェーン
                for (int j = 0; j < kNumFilters; ++j)
                    if (p.flt[(size_t)j].on && p.flt[(size_t)j].route[(size_t)o])
                        filters[(size_t)j][(size_t)o].processStereo(lo, ro);

                l += lo;
                r += ro;
            }

            // ---- ノイズ (ソース3) ----
            if (active[3])
            {
                levelSm[3] += smCoef * (levelTarget[3] - levelSm[3]);
                const float nz = nextNoise(p.noiseType) * levelSm[3];
                float nl = nz, nr = nz;
                noiseFilter.processStereo(nl, nr);

                for (int j = 0; j < kNumFilters; ++j)
                    if (p.flt[(size_t)j].on && p.flt[(size_t)j].route[3])
                        filters[(size_t)j][3].processStereo(nl, nr);

                l += nl;
                r += nr;
            }

            // ---- アンプエンベロープ + デクリック ----
            const float target = noteHeld ? 1.0f : 0.0f;
            ampEnv += (noteHeld ? attCoef : relCoef) * (target - ampEnv);
            declickGain += declickCoef * (1.0f - declickGain);

            const float g = ampEnv * velGain * declickGain;
            outL[i] += l * g;
            outR[i] += r * g;
        }

        uiProgress.store((float)progress, std::memory_order_relaxed);
    }

private:
    // ---- コントロールティック: 全カーブ評価とターゲット更新 ----
    void controlTick(const Params& p, const CurveStore& curves) noexcept
    {
        // ENV評価位置: Auto=Progress / Manual=LIFTノブ (ティックレート平滑)
        const float posTarget = juce::jlimit(0.0f, 1.0f,
            p.liftAuto ? (float)progress : p.lift);
        if (snapNext)
            liftSm = posTarget;   // ノートオン直後は評価位置も即スナップ (開始チャープ防止)
        else
            liftSm += 0.3f * (posTarget - liftSm);
        const float evalPos = liftSm;

        // バイポーラ偏差 (-1..1)
        auto bip = [&curves, evalPos](int idx) noexcept
        {
            return (curves.read(idx).evaluate(evalPos) - 0.5f) * 2.0f;
        };
        // ユニポーラ (0..1)
        auto uni = [&curves, evalPos](int idx) noexcept
        {
            return curves.read(idx).evaluate(evalPos);
        };

        for (int o = 0; o < kNumOscs; ++o)
        {
            const auto& po = p.osc[(size_t)o];

            // PITCH: StartKey→EndKey のユニポーラ補間 (+COARSE)
            const float ky = uni(CurveStore::oscCurve(o, 0));
            pitchTarget[(size_t)o] = (float)po.keyStart
                                   + ((float)po.keyEnd - (float)po.keyStart) * ky
                                   + po.coarse;

            // LEVEL: バイポーラ加算 (±0.5)
            levelTarget[(size_t)o] = juce::jlimit(0.0f, 1.0f,
                po.level + bip(CurveStore::oscCurve(o, 1)) * 0.5f);

            // DETUNE: ±50ct / SPREAD: ±0.5 (ティックレート平滑→ユニゾンテーブル再計算)
            const float detTgt = juce::jlimit(0.0f, 100.0f,
                po.detune + bip(CurveStore::oscCurve(o, 2)) * 50.0f);
            const float sprTgt = juce::jlimit(0.0f, 1.0f,
                po.spread + bip(CurveStore::oscCurve(o, 3)) * 0.5f);
            detSm[(size_t)o] += 0.35f * (detTgt - detSm[(size_t)o]);
            sprSm[(size_t)o] += 0.35f * (sprTgt - sprSm[(size_t)o]);
            const float detEff = detSm[(size_t)o];
            const float sprEff = sprSm[(size_t)o];

            const int uniN = juce::jlimit(1, kMaxUnison, po.unison);
            for (int v = 0; v < kMaxUnison; ++v)
            {
                const float off = (uniN <= 1) ? 0.0f : (2.0f * (float)v / (float)(uniN - 1) - 1.0f);
                centsFac[(size_t)o][(size_t)v] = std::exp2(off * detEff / 1200.0f);
                const float pan = 0.5f + off * 0.5f * sprEff;
                const float th = pan * juce::MathConstants<float>::halfPi;
                gainL[(size_t)o][(size_t)v] = std::cos(th);
                gainR[(size_t)o][(size_t)v] = std::sin(th);
            }
        }

        // ノイズ: PITCH (バイポーラoct) / LEVEL / RES
        {
            const float target = p.noisePitch
                * std::exp2(bip(CurveStore::NoisePitch) * p.noiseRangeOct);
            noiseCutSm += 0.5f * (target - noiseCutSm);

            levelTarget[3] = juce::jlimit(0.0f, 1.0f,
                p.noiseLevel + bip(CurveStore::NoiseLevel) * 0.5f);

            const float resTgt = juce::jlimit(0.5f, 12.0f,
                p.noiseRes + bip(CurveStore::NoiseRes) * 5.75f);
            noiseResSm += 0.35f * (resTgt - noiseResSm);
            noiseFilter.setCoef(noiseCutSm, noiseResSm);
        }

        // フィルター: バイポーラ ±5oct × ENV AMT
        for (int j = 0; j < kNumFilters; ++j)
        {
            if (!p.flt[(size_t)j].on) continue;
            const float target = p.flt[(size_t)j].cutoff
                * std::exp2(p.flt[(size_t)j].env * bip(CurveStore::Filter1 + j) * 5.0f);
            cutSm[(size_t)j] += 0.5f * (target - cutSm[(size_t)j]);
            resSm[(size_t)j] += 0.35f * (p.flt[(size_t)j].res - resSm[(size_t)j]);

            for (int s = 0; s < kNumSources; ++s)
            {
                filters[(size_t)j][(size_t)s].setType(p.flt[(size_t)j].type);
                filters[(size_t)j][(size_t)s].setCoef(cutSm[(size_t)j], resSm[(size_t)j]);
            }
        }

        // ノートオン直後は平滑をスナップ (古い値からのグライド防止)
        if (snapNext)
        {
            snapNext = false;
            pitchSm = pitchTarget;
            levelSm = levelTarget;
            for (int o = 0; o < kNumOscs; ++o)
                posSm[(size_t)o] = p.osc[(size_t)o].pos;
        }
    }

    // ---- ノイズジェネレーター ----
    inline float nextNoise(int type) noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        const float w = ((float)(rngState & 0xffffff) / 8388608.0f) - 1.0f;

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
        case 2: // Brown
            brownState = 0.995f * brownState + w * 0.05f;
            return brownState * 3.0f;
        default: // White
            return w;
        }
    }

    // ---- 状態 ----
    std::array<const MorphWavetable*, kNumOscs> wavetables { nullptr, nullptr, nullptr };
    double sr = 44100.0;

    bool noteHeld = false;
    bool hostSync = false;
    bool snapNext = true;
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
    std::array<float, kNumSources> levelTarget {};
    std::array<float, kNumSources> levelSm {};
    std::array<float, kNumOscs> posSm {};
    std::array<float, kNumOscs> detSm {};
    std::array<float, kNumOscs> sprSm {};
    float smCoef = 0.01f;
    float declickCoef = 0.02f;
    float declickGain = 1.0f;
    float liftSm = 1.0f;

    // [フィルター][ソース] = 16基 (ソース別ルーティング用)
    std::array<std::array<TptSvf, kNumSources>, kNumFilters> filters;
    std::array<float, kNumFilters> cutSm { 1000.0f, 1000.0f, 1000.0f, 1000.0f };
    std::array<float, kNumFilters> resSm { 0.9f, 0.9f, 0.9f, 0.9f };

    TptSvf noiseFilter;
    float noiseCutSm = 500.0f;
    float noiseResSm = 2.0f;

    juce::uint32 rngState = 0x9e3779b9;
    std::array<float, 7> pinkB {};
    float brownState = 0.0f;

    JUCE_DECLARE_NON_COPYABLE(RiserEngine)
};
