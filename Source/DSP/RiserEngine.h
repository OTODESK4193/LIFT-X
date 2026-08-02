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
#include "ScaleQuantizer.h"

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
        // REVERSE: ENV評価位置を 1-pos に反転する。全カーブが逆再生になるため、
        //  ライザー↔ダウナーがそのまま入れ替わる (ピッチ/レベル/フィルター/FX全て)。
        bool  reverse = false;

        // ---- Pitch ENV スケール量子化 (Key/Scale はグローバル, 適用可否はOSC毎) ----
        bool scaleOn = false;    // マスターOn/Off (Configタブ)
        int  scaleKey = 0;       // 0..11 (C..B)
        int  scaleType = 1;      // ScaleQuantizer::getScales() のインデックス

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
            bool  scaleQ = true;     // このOSCにスケール量子化を適用するか
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

    //  maxBlockSize: ホストが渡してくる想定最大ブロック長。
    //   LIFT を手動/オートメーションで動かす場合、APVTS はブロック先頭の値しか
    //   返さないため評価位置が「ブロック単位の階段」で届く。これを均すには
    //   平滑の時定数がブロック長より長い必要があるので、ここで受け取る。
    void prepare(double sampleRate, int maxBlockSize = 512) noexcept
    {
        sr = juce::jmax(8000.0, sampleRate);
        blockSamples = juce::jmax(16, maxBlockSize);
        for (auto& row : filters)
            for (auto& f : row)
                f.prepare(sr);
        noiseFilter.prepare(sr);
        noiseFilter.setType(TptSvf::BandPass);

        // ---- サンプル単位平滑 ----
        smCoef      = 1.0f - std::exp(-1.0f / (0.004f  * (float)sr)); // τ≒4ms (通常ピッチ/レベル)
        smCoefFast  = 1.0f - std::exp(-1.0f / (0.0012f * (float)sr)); // τ≒1.2ms (スケール量子化時のピッチ)

        // リトリガー用デクリック: 1.5msのリニアランプ (0/1へ厳密に到達させる)
        declickSamples = juce::jmax(8, (int)(sr * 0.0015));
        declickInc = 1.0f / (float)declickSamples;

        // ---- コントロールティック単位平滑 (SR非依存化) ----
        //  従来は固定係数 (0.3/0.35/0.5) だったため、ティック間隔が短くなる
        //  高SR (96/192kHz) では時定数が最大4.4倍速くなり、44.1kHzと音が変わっていた。
        //  ここで実時間の時定数から係数を算出し、全SRで同一挙動にする。
        //  (44.1kHz での従来値と一致する τ を採用)
        const double tickRate = sr / (double)kCtrlInterval;   // ティック/秒
        auto tickCoef = [tickRate](double tauSec) noexcept
        {
            return (float)(1.0 - std::exp(-1.0 / (juce::jmax(1.0e-5, tauSec) * tickRate)));
        };
        modTickCoef  = tickCoef(0.00207);   // 旧 0.35 @44.1k
        cutTickCoef  = tickCoef(0.00145);   // 旧 0.50 @44.1k

        // ---- LIFT (ENV評価位置) の平滑: AUTO と MANUAL で時定数を分ける ----
        //  AUTO  : progress はサンプル単位で連続的に進むため、入力に段差が無い。
        //          τ を伸ばすと Steps 系カーブの段差まで鈍ってしまうので、
        //          従来どおり 2.42ms のまま速く保つ。
        //  MANUAL: LIFT ノブ/オートメーションはブロック単位の階段で届く。
        //          τ がブロック長より短いと段差がそのまま残りジッパーノイズになる。
        //          (512サンプル @48kHz = 10.7ms に対し従来は 2.42ms しかなかった)
        //          ブロック長の1.5倍を目安に伸ばして段差を埋める。
        liftTickCoefAuto = tickCoef(0.00242);   // 旧 0.30 @44.1k 相当

        const double blockSec = (double)blockSamples / sr;
        liftTickCoefManual = tickCoef(juce::jmax(0.00242, blockSec * 1.5));

        // ---- ノイズ: 固定44.1kHz仮想レートでの生成 (スペクトルをSR非依存化) ----
        //  Pink(Kellett)/Brown の係数は44.1kHz設計。高SRでそのまま回すと
        //  折れ点が周波数軸上で持ち上がり「明るいピンク/ブラウン」になってしまう。
        //  White も帯域がNyquistまで広がるため可聴帯域のパワーが下がる。
        //  → 生成を44.1kHz固定クロックで行い、線形補間でホストSRへ引き伸ばす。
        //    sr==44100 のときは step==1 で従来と完全に同一挙動。
        noiseStep = (float)(kNoiseBaseRate / sr);
        //  線形補間による分散低下 (オーバーサンプル時 2/3 に漸近) を補正
        const float varFactor = juce::jlimit(0.05f, 1.0f,
            juce::jmin(1.0f, noiseStep) + (1.0f - juce::jmin(1.0f, noiseStep)) * (2.0f / 3.0f));
        noiseInterpGain = 1.0f / std::sqrt(varFactor);

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
        declickState = Declick::Idle;
        pendingNote = false;
        pendNote = -1;
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
        // ユニゾン係数キャッシュを無効化 (次のティックで必ず再計算させる)
        lastUni.fill(-1);
        lastDet.fill(-1.0e9f);
        lastSpr.fill(-1.0e9f);
        maxCentsFac.fill(1.0f);
        curveHint.fill(0);
        cutSm.fill(1000.0f);
        noiseResSm = 2.0f;
        noiseCutSm = 500.0f;
        for (auto& row : filters)
            for (auto& f : row)
                f.reset();
        noiseFilter.reset();
        pinkB.fill(0.0f);
        brownState = 0.0f;
        noisePhase = 0.0f;
        noiseCur = noiseNext = 0.0f;
        uiProgress.store(0.0f, std::memory_order_relaxed);
        for (auto& u : uiPitch) u.store(60.0f, std::memory_order_relaxed);
    }

    // ---- MIDI ----
    //  発音中の再ノートオン (連打・リトリガー) は 2段階デクリックで処理する。
    //
    //  旧実装は declickGain を「いきなり 0 に落として」からフェードインしていた。
    //  フェードイン自体は滑らかでも、0へ落とす瞬間が振幅の段差になるため、
    //  それがそのままプチッというクリックになっていた (連打時に顕著)。
    //
    //  新実装:
    //    1) FadeOut : 現在の音を約1.5msで無音までリニアに絞る (段差なし)
    //    2) 無音になった時点で位相/進行/平滑スナップ/フィルター状態をリセット
    //       → 不連続が起きる処理はすべて出力が0のあいだに済ませる
    //    3) FadeIn  : 約1.5msで復帰
    //  リセットが遅延する分の約1.5msは知覚できない。
    void noteOn(int note, float velocity, double ppqNow, bool hostPlaying) noexcept
    {
        // 十分に鳴っている最中なら、必ずフェードアウトを経由させる
        if (ampEnv > kRetrigThresh || declickState == Declick::FadeOut)
        {
            // 連打でフェードアウト中に更にノートオンが来た場合は最新の内容で上書き
            pendingNote = true;
            pendNote = note;
            pendVel = velocity;
            pendPpq = ppqNow;
            pendHost = hostPlaying;
            declickState = Declick::FadeOut;
            return;
        }

        // ほぼ無音 (-54dB以下) からの発音は段差にならないため即時適用
        applyNoteOn(note, velocity, ppqNow, hostPlaying);
        declickState = Declick::Idle;
        declickGain = 1.0f;
    }

    void noteOff(int note) noexcept
    {
        if (note == curNote)
            noteHeld = false;
    }

    void allNotesOff() noexcept { noteHeld = false; }

private:
    // 実際のノートオン適用 (出力が無音のあいだに呼ぶこと)
    void applyNoteOn(int note, float velocity, double ppqNow, bool hostPlaying) noexcept
    {
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

        // フィルターの残留状態をクリア (前回の残響リング防止)。
        // ここは必ず出力0の瞬間なので、リセットによる不連続は表に出ない。
        for (auto& row : filters)
            for (auto& f : row)
                f.reset();
        noiseFilter.reset();
    }

public:

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

    bool isNoteActive() const noexcept { return noteHeld || pendingNote || ampEnv > 1.0e-4f; }
    float getProgressF() const noexcept { return (float)progress; }

    // controlTick が実際にカーブを読んだ位置 (平滑後・REVERSE適用後)。
    //  FXチェーン側も同じ位置でカーブを評価するために公開する。
    //  以前はFX側が「生の progress / 生の LIFTノブ値」から独自に評価位置を
    //  組み立てていたため、MANUALで素早く動かすとオシレーターは滑らかに
    //  追従するのにFXだけブロック単位で跳ぶ、という不一致が起きていた。
    float getEvalPos() const noexcept { return liftSm; }

    std::atomic<float> uiProgress { 0.0f };

    // ---- GUI用ライブピッチ (PITCH RAIL表示) ----
    //  OSC毎の平滑後の絶対ピッチ (MIDIノート番号, 小数)。発音していないときは
    //  カーブ評価位置に対応する「静止ピッチ」を返すため、LIFT MANUALでノブを
    //  動かすだけでも表示が追従する。
    std::array<std::atomic<float>, kNumOscs> uiPitch { };
    float getUiPitch(int osc) const noexcept
    {
        return uiPitch[(size_t)juce::jlimit(0, kNumOscs - 1, osc)].load(std::memory_order_relaxed);
    }

    // ---- レンダリング ----
    //  busL/busR : OSC1 / OSC2 / OSC3 / Noise の4系統ステレオバス (加算書き込み)
    //  FXチェーンでソース別ルーティングを行うため、ミックスせずに分離したまま返す。
    //  呼び出し側は事前にバスをゼロクリアしておくこと。
    void render(float* const* busL, float* const* busR, int numSamples, const Params& p,
                const CurveStore& curves) noexcept
    {
        if (numSamples <= 0 || busL == nullptr || busR == nullptr) return;

        // 保留中のノートオンがある場合は早期リターンしない
        // (リリース中に連打された場合でも取りこぼさないため)
        if (!noteHeld && ampEnv <= 1.0e-4f && !pendingNote)
        {
            if (progress > 0.0) { progress = 0.0; uiProgress.store(0.0f, std::memory_order_relaxed); }
            ampEnv = 0.0f;
            // 発音していなくてもGUIのPITCH RAILを追従させるため、
            // ブロックあたり1回だけコントロールティックを回してターゲットを更新する。
            // (音は出さないので出力バッファには一切書き込まない)
            controlTick(p, curves);
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

            // ソース別の出力 (ミックスせずバスへ書き出す)
            std::array<float, kNumSources> srcL {};
            std::array<float, kNumSources> srcR {};

            // ---- OSC1-3 (各ソース独立にフィルタールーティング) ----
            for (int o = 0; o < kNumOscs; ++o)
            {
                if (!active[(size_t)o]) continue;
                const MorphWavetable* wt = wavetables[(size_t)o];
                if (wt == nullptr) continue;

                const auto& po = p.osc[(size_t)o];

                // スケール量子化時はステップ感を出すため速い時定数 (τ≒1.2ms) を使う。
                // クリック防止には十分な長さを確保している。
                const float pCoef = pitchQuant[(size_t)o] ? smCoefFast : smCoef;
                pitchSm[(size_t)o] += pCoef * (pitchTarget[(size_t)o] - pitchSm[(size_t)o]);
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

                // ミップはユニゾン全声部で共有する。
                //  デチューンは最大 ±50cent (約±3%) なので inc の差は1オクターブに
                //  遠く及ばず、最も高い声部を基準に選べばどの声部でも折り返さない。
                //  (旧実装は声部ごとに while ループでミップを探索しており、
                //   3OSC × 7声部 = 最大21回/サンプル 回っていた)
                const int mip = MorphWavetable::mipFor(
                    juce::jmin(0.45f, inc0 * maxCentsFac[(size_t)o]));

                float lo = 0.0f, ro = 0.0f;
                for (int v = 0; v < uni; ++v)
                {
                    float inc = inc0 * cf[v];
                    if (inc > 0.45f) inc = 0.45f;
                    float pv = ph[v] + inc;
                    if (pv >= 1.0f) pv -= 1.0f;
                    ph[v] = pv;
                    const float s = wt->sampleAtMip(pv, morph, mip, useCustom) * norm;
                    lo += s * gl[v];
                    ro += s * gr[v];
                }

                // ソース別フィルターチェーン
                for (int j = 0; j < kNumFilters; ++j)
                    if (p.flt[(size_t)j].on && p.flt[(size_t)j].route[(size_t)o])
                        filters[(size_t)j][(size_t)o].processStereo(lo, ro);

                srcL[(size_t)o] = lo;
                srcR[(size_t)o] = ro;
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

                srcL[3] = nl;
                srcR[3] = nr;
            }

            // ---- アンプエンベロープ ----
            const float target = noteHeld ? 1.0f : 0.0f;
            ampEnv += (noteHeld ? attCoef : relCoef) * (target - ampEnv);

            // ---- 2段階デクリック (リニアランプ: 0/1へ厳密に到達する) ----
            if (declickState == Declick::FadeOut)
            {
                declickGain -= declickInc;
                if (declickGain <= 0.0f)
                {
                    declickGain = 0.0f;
                    // 出力が完全に0のこの瞬間にリセットを実行する
                    if (pendingNote)
                    {
                        pendingNote = false;
                        applyNoteOn(pendNote, pendVel, pendPpq, pendHost);
                    }
                    declickState = Declick::FadeIn;
                }
            }
            else if (declickState == Declick::FadeIn)
            {
                declickGain += declickInc;
                if (declickGain >= 1.0f) { declickGain = 1.0f; declickState = Declick::Idle; }
            }

            const float g = ampEnv * velGain * declickGain;
            for (int s = 0; s < kNumSources; ++s)
            {
                busL[s][i] += srcL[(size_t)s] * g;
                busR[s][i] += srcR[(size_t)s] * g;
            }
        }

        uiProgress.store((float)progress, std::memory_order_relaxed);
    }

private:
    // ---- コントロールティック: 全カーブ評価とターゲット更新 ----
    void controlTick(const Params& p, const CurveStore& curves) noexcept
    {
        // ENV評価位置: Auto=Progress / Manual=LIFTノブ (ティックレート平滑)
        //  REVERSE時は 1-pos として全カーブを逆から読む
        float posTarget = juce::jlimit(0.0f, 1.0f,
            p.liftAuto ? (float)progress : p.lift);
        if (p.reverse)
            posTarget = 1.0f - posTarget;
        if (snapNext)
            liftSm = posTarget;   // ノートオン直後は評価位置も即スナップ (開始チャープ防止)
        else
            liftSm += (p.liftAuto ? liftTickCoefAuto : liftTickCoefManual)
                    * (posTarget - liftSm);
        const float evalPos = liftSm;

        // バイポーラ偏差 (-1..1)。curveHint でセグメント探索を実質O(1)にする。
        auto bip = [this, &curves, evalPos](int idx) noexcept
        {
            return (curves.read(idx).evaluate(evalPos, &curveHint[(size_t)idx]) - 0.5f) * 2.0f;
        };
        // ユニポーラ (0..1)
        auto uni = [this, &curves, evalPos](int idx) noexcept
        {
            return curves.read(idx).evaluate(evalPos, &curveHint[(size_t)idx]);
        };

        for (int o = 0; o < kNumOscs; ++o)
        {
            const auto& po = p.osc[(size_t)o];

            // PITCH: StartKey→EndKey のユニポーラ補間 (+COARSE)
            const float ky = uni(CurveStore::oscCurve(o, 0));
            float basePitch = (float)po.keyStart
                            + ((float)po.keyEnd - (float)po.keyStart) * ky;

            const bool quant = p.scaleOn && po.scaleQ;
            if (quant)
                basePitch = ScaleQuantizer::quantize(basePitch, p.scaleKey, p.scaleType);
            pitchTarget[(size_t)o] = basePitch + po.coarse;
            pitchQuant[(size_t)o] = quant;

            // GUI (PITCH RAIL) へライブピッチを公開
            uiPitch[(size_t)o].store(pitchTarget[(size_t)o], std::memory_order_relaxed);

            // LEVEL: バイポーラ加算・フルレンジ (中央=ノブ値, 上端=MAX方向, 下端=MIN方向)
            //  ノブ0で下方向へ描いても変化なし (クランプ)。上端は必ずMAXへ到達可能。
            levelTarget[(size_t)o] = juce::jlimit(0.0f, 1.0f,
                po.level + bip(CurveStore::oscCurve(o, 1)) * 1.0f);

            // DETUNE: ±100ct / SPREAD: ±1.0 (フルレンジ, ティックレート平滑)
            const float detTgt = juce::jlimit(0.0f, 100.0f,
                po.detune + bip(CurveStore::oscCurve(o, 2)) * 100.0f);
            const float sprTgt = juce::jlimit(0.0f, 1.0f,
                po.spread + bip(CurveStore::oscCurve(o, 3)) * 1.0f);
            detSm[(size_t)o] += modTickCoef * (detTgt - detSm[(size_t)o]);
            sprSm[(size_t)o] += modTickCoef * (sprTgt - sprSm[(size_t)o]);
            if (snapNext) { detSm[(size_t)o] = detTgt; sprSm[(size_t)o] = sprTgt; }
            const float detEff = detSm[(size_t)o];
            const float sprEff = sprSm[(size_t)o];

            // ---- ユニゾンのデチューン係数 / パンゲイン ----
            //  旧実装は UNISON=1 でも常に kMaxUnison(7) 本を回していたため、
            //  3OSC × 7 × (exp2 + cos + sin) = 63回の超越関数を毎ティック
            //  計算していた。実際に使う本数だけに絞り、さらに
            //  「本数・デチューン・スプレッドのいずれも変化していなければ
            //   再計算そのものを飛ばす」ようにする。
            //  DETUNE/SPREAD にカーブを描いていない通常のプリセットでは、
            //  平滑が収束した時点で以降ずっとスキップされる。
            const int uniN = juce::jlimit(1, kMaxUnison, po.unison);
            const bool needRecalc = snapNext
                                 || uniN != lastUni[(size_t)o]
                                 || std::abs(detEff - lastDet[(size_t)o]) > 1.0e-4f
                                 || std::abs(sprEff - lastSpr[(size_t)o]) > 1.0e-5f;

            if (needRecalc)
            {
                lastUni[(size_t)o] = uniN;
                lastDet[(size_t)o] = detEff;
                lastSpr[(size_t)o] = sprEff;

                for (int v = 0; v < uniN; ++v)
                {
                    const float off = (uniN <= 1) ? 0.0f : (2.0f * (float)v / (float)(uniN - 1) - 1.0f);
                    centsFac[(size_t)o][(size_t)v] = std::exp2(off * detEff / 1200.0f);
                    const float pan = 0.5f + off * 0.5f * sprEff;
                    const float th = pan * juce::MathConstants<float>::halfPi;
                    gainL[(size_t)o][(size_t)v] = std::cos(th);
                    gainR[(size_t)o][(size_t)v] = std::sin(th);
                }

                // ミップ選択用: 最も高い声部のピッチ倍率 (= +detEff/2 cent 側)
                maxCentsFac[(size_t)o] = std::exp2(detEff / 1200.0f);
            }
        }

        // ノイズ: PITCH (バイポーラoct) / LEVEL / RES
        {
            // 20Hz..Nyquist手前へクランプ (平滑器が極端な値を保持しないように)
            const float target = juce::jlimit(20.0f, (float)(sr * 0.45),
                p.noisePitch * std::exp2(bip(CurveStore::NoisePitch) * p.noiseRangeOct));
            noiseCutSm += cutTickCoef * (target - noiseCutSm);

            levelTarget[3] = juce::jlimit(0.0f, 1.0f,
                p.noiseLevel + bip(CurveStore::NoiseLevel) * 1.0f);

            const float resTgt = juce::jlimit(0.5f, 12.0f,
                p.noiseRes + bip(CurveStore::NoiseRes) * 11.5f);
            noiseResSm += modTickCoef * (resTgt - noiseResSm);

            // ノートオン直後はターゲットへスナップ (前ノートの残値からのスイープ防止)
            if (snapNext) { noiseCutSm = target; noiseResSm = resTgt; }

            noiseFilter.setCoef(noiseCutSm, noiseResSm);
        }

        // フィルター: フルレンジ (20Hz..20kHz全域) × ENV AMT
        for (int j = 0; j < kNumFilters; ++j)
        {
            if (!p.flt[(size_t)j].on) continue;
            const float modAmount = juce::jlimit(-1.0f, 1.0f, p.flt[(size_t)j].env * bip(CurveStore::Filter1 + j));
            const float maxCutHz = (float)(sr * 0.45);
            const float baseCutHz = juce::jlimit(20.0f, maxCutHz, p.flt[(size_t)j].cutoff);
            const float logCut = std::log2(baseCutHz);
            const float logTarget = modAmount >= 0.0f ? logCut + modAmount * (std::log2(maxCutHz) - logCut)
                                                       : logCut + modAmount * (logCut - std::log2(20.0f));
            const float target = juce::jlimit(20.0f, maxCutHz, std::exp2(logTarget));

            const float resBip = bip(CurveStore::Filter1Res + j);
            const float resModAmount = juce::jlimit(-1.0f, 1.0f, p.flt[(size_t)j].env * resBip);
            const float baseRes = juce::jlimit(0.5f, 12.0f, p.flt[(size_t)j].res);
            const float resTarget = juce::jlimit(0.5f, 12.0f,
                resModAmount >= 0.0f ? baseRes + resModAmount * (12.0f - baseRes)
                                     : baseRes + resModAmount * (baseRes - 0.5f));

            cutSm[(size_t)j] += cutTickCoef * (target - cutSm[(size_t)j]);
            resSm[(size_t)j] += modTickCoef * (resTarget - resSm[(size_t)j]);

            // ノートオン直後はスナップ (前ノート終端からのグライド防止)
            if (snapNext) { cutSm[(size_t)j] = target; resSm[(size_t)j] = resTarget; }

            // 同一フィルターの4ソース分は係数が完全に同じなので、
            // std::tan を含む係数計算は1回だけ行って各基へ配る。
            const auto coefs = TptSvf::computeCoefs(cutSm[(size_t)j], resSm[(size_t)j], sr);
            for (int s = 0; s < kNumSources; ++s)
            {
                filters[(size_t)j][(size_t)s].setType(p.flt[(size_t)j].type);
                filters[(size_t)j][(size_t)s].setCoefs(coefs);
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
    //  内部は 44.1kHz 固定の仮想クロックで生成し、線形補間でホストSRへ伸ばす。
    //  これにより Pink/Brown のスペクトル形状と White の可聴帯域パワーが
    //  44.1 / 48 / 88.2 / 96 / 176.4 / 192kHz で完全に一致する。
    //  sr == 44100 のときは step == 1 となり、旧実装と同一の出力になる。
    inline float nextNoise(int type) noexcept
    {
        noisePhase += noiseStep;
        while (noisePhase >= 1.0f)
        {
            noisePhase -= 1.0f;
            noiseCur = noiseNext;
            noiseNext = genNoise(type);
        }
        return (noiseCur + (noiseNext - noiseCur) * noisePhase) * noiseInterpGain;
    }

    // 44.1kHz 基準の 1サンプル生成
    inline float genNoise(int type) noexcept
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
    std::array<bool,  kNumOscs> pitchQuant { false, false, false };
    std::array<float, kNumSources> levelTarget {};
    std::array<float, kNumSources> levelSm {};
    std::array<float, kNumOscs> posSm {};
    std::array<float, kNumOscs> detSm {};
    std::array<float, kNumOscs> sprSm {};

    // ユニゾン係数の再計算スキップ判定用 (前回計算時の本数/デチューン/スプレッド)
    std::array<int,   kNumOscs> lastUni { -1, -1, -1 };
    std::array<float, kNumOscs> lastDet { -1.0e9f, -1.0e9f, -1.0e9f };
    std::array<float, kNumOscs> lastSpr { -1.0e9f, -1.0e9f, -1.0e9f };
    // ミップ選択用: 最高声部のピッチ倍率 = exp2(detune/1200)
    std::array<float, kNumOscs> maxCentsFac { 1.0f, 1.0f, 1.0f };

    // カーブ評価のセグメント探索ヒント (mutable: controlTick から更新される)
    mutable std::array<int, CurveStore::kNumCurves> curveHint {};
    float smCoef = 0.01f;
    float smCoefFast = 0.03f;
    float liftSm = 1.0f;

    // ---- 2段階デクリック (リトリガー時のクリック対策) ----
    enum class Declick { Idle, FadeOut, FadeIn };
    static constexpr float kRetrigThresh = 0.002f;   // -54dB: これ以下は即時切替でも段差にならない
    Declick declickState = Declick::Idle;
    float declickGain = 1.0f;
    float declickInc = 0.02f;
    int   declickSamples = 64;
    // フェードアウト完了まで保留するノートオン情報
    bool   pendingNote = false;
    int    pendNote = -1;
    float  pendVel = 1.0f;
    double pendPpq = 0.0;
    bool   pendHost = false;

    // コントロールティック平滑係数 (prepare() でSR/ブロック長から算出)
    float liftTickCoefAuto   = 0.30f;   // AUTO:   progress連動 (速い)
    float liftTickCoefManual = 0.30f;   // MANUAL: ブロック長追従 (遅い)
    float modTickCoef  = 0.35f;
    float cutTickCoef  = 0.50f;
    int   blockSamples = 512;

    // [フィルター][ソース] = 16基 (ソース別ルーティング用)
    std::array<std::array<TptSvf, kNumSources>, kNumFilters> filters;
    std::array<float, kNumFilters> cutSm { 1000.0f, 1000.0f, 1000.0f, 1000.0f };
    std::array<float, kNumFilters> resSm { 0.9f, 0.9f, 0.9f, 0.9f };

    TptSvf noiseFilter;
    float noiseCutSm = 500.0f;
    float noiseResSm = 2.0f;

    // ノイズ: 44.1kHz固定クロック生成 + 線形補間 (SR非依存化)
    static constexpr double kNoiseBaseRate = 44100.0;
    juce::uint32 rngState = 0x9e3779b9;
    std::array<float, 7> pinkB {};
    float brownState = 0.0f;
    float noiseStep = 1.0f;
    float noisePhase = 0.0f;
    float noiseCur = 0.0f, noiseNext = 0.0f;
    float noiseInterpGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE(RiserEngine)
};
