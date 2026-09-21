// ==========================================
// File: PluginProcessor.h
// LIFT-X プロセッサー層 (v0.2)
//  - DSPコアとGUIの完全分離 / processBlock内アロケーション・ロック禁止
//  - DAWフェイルセーフ: SR/ブロックサイズ不一致時の即時ゼロクリア+リセット
//  - マルチENV(35系統)はAPVTS外のCurveStoreで管理 (オートメーション隔離)
//  - OSC毎のカスタムWavetable (SPECTRA8方式のグローバル設定でフォルダ永続化)
//  - MIDI Learn (StartKey/EndKey設定用の最終ノート通知)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "DSP/Wavetable.h"
#include "DSP/CurveData.h"
#include "DSP/ScaleQuantizer.h"
#include "DSP/RiserEngine.h"
#include "DSP/FxChain.h"
#include "DSP/Limiter.h"

class LiftXAudioProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::Timer
{
public:
    LiftXAudioProcessor();
    ~LiftXAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LIFT-X"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    // FXの実設定 (Reverb DECAY / Delay FB+TIME) から実際の残響長を見積もって返す。
    //  固定値だとホストのフリーズ/バウンス時に長いリバーブが切られてしまう。
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- GUIとの橋渡し ----
    CurveStore& getCurves() noexcept { return mCurves; }
    float getUiProgress() const noexcept { return mEngine.uiProgress.load(std::memory_order_relaxed); }

    // OSC毎のライブ絶対ピッチ (MIDIノート番号・小数)。PITCH RAIL表示用。
    float getUiPitch(int oscIdx) const noexcept { return mEngine.getUiPitch(oscIdx); }

    // ---- MIDI Learn (StartKey/EndKey設定用) ----
    //  GUI側はイベントカウンタの増加を監視し、最終ノート番号を取得する。
    int getNoteEventCount() const noexcept { return mNoteEvents.load(std::memory_order_relaxed); }
    int getLastNote() const noexcept { return mLastNote.load(std::memory_order_relaxed); }

    // ---- ライザー出力キャプチャ (波形表示 + WAV D&D用) ----
    //  ノートオンで録音開始。本編は「指定Bar分 or リリース完了」で打ち切り、
    //  以降は FX のテールが実際に鳴り止むまで録り続ける (無音検知方式)。
    //  固定秒数ではないため、Shimmer Reverb の長い減衰や FB 0.95 の Delay でも
    //  切れ際まで確実に収録できる。
    //  GUIはバージョン増加を検知してコピーを取る (書き込み中の参照は表示専用)。
    const float* getCaptureL() const noexcept { return mCapL.data(); }
    const float* getCaptureR() const noexcept { return mCapR.data(); }
    int getCaptureLength() const noexcept { return mCapLenPub.load(std::memory_order_relaxed); }
    int getCaptureVersion() const noexcept { return mCapVersion.load(std::memory_order_relaxed); }
    bool isCapturing() const noexcept { return mCapActive.load(std::memory_order_relaxed); }
    double getPreparedSampleRate() const noexcept { return juce::jmax(8000.0, mPreparedSampleRate); }

    // 本編(指定Bar分)の終端サンプル位置。GUIが波形上にテール開始線を引くのに使う。
    // 0 = まだ本編中 (テールへ入っていない)。
    int getCaptureBodyEnd() const noexcept { return mCapBodyEndPub.load(std::memory_order_relaxed); }

    // 直近のホストBPM (書き出しファイル名などGUI表示用)
    double getLastBpm() const noexcept { return mLastBpm.load(std::memory_order_relaxed); }

    // ---- アウトプットメーター (ヘッダー表示用) ----
    //  ブロック毎のピーク値。GUIは読むだけ (減衰はGUI側で行う)。
    float getOutPeak(int ch) const noexcept
    {
        return mOutPeak[(size_t)juce::jlimit(0, 1, ch)].load(std::memory_order_relaxed);
    }

    // ---- カスタムWavetable (OSC毎 / メッセージスレッド専用) ----
    bool loadCustomWavetable(int oscIdx, const juce::File& file);
    void clearCustomWavetable(int oscIdx);
    juce::String getCustomWavetablePath(int oscIdx) const
    {
        return apvts.state.getProperty("customWavetablePath" + juce::String(oscIdx + 1),
                                       juce::String()).toString();
    }
    bool hasCustomWavetable(int oscIdx) const
    {
        return mWavetables[(size_t)juce::jlimit(0, 2, oscIdx)].hasCustom();
    }
    const MorphWavetable& getWavetable(int oscIdx) const noexcept
    {
        return mWavetables[(size_t)juce::jlimit(0, 2, oscIdx)];
    }

    // ---- グローバル設定 (SPECTRA8方式: セッションと独立してユーザー設定へ永続化) ----
    //   Windows: %APPDATA%/LIFT-X/LIFT-X.settings
    //   Wavetableフォルダの登録パスはここに置く (毎回登録し直さなくて済む)
    static juce::PropertiesFile& getGlobalSettings();
    static juce::String getGlobalWavetableDir();
    static void setGlobalWavetableDir(const juce::String& path);

    // Bars選択肢 (1/32〜16小節)
    static juce::StringArray getBarsNames()
    {
        return { "1/32", "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16" };
    }
    static double barsFromChoice(int idx) noexcept
    {
        static constexpr double b[10] = { 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0,
                                          1.0, 2.0, 4.0, 8.0, 16.0 };
        return b[juce::jlimit(0, 9, idx)];
    }

    // ENV評価位置 (GUIのプレイヘッド/ModBand用): Auto=Progress / Manual=LIFTノブ
    float getEnvPosition() const noexcept;

    // ---- PANIC: 全音停止 & FXバッファ/ディレイ/リバーブ即時クリア ----
    void triggerPanic() noexcept { mPanicRequested.store(true, std::memory_order_relaxed); }

    // ---- プリセット (メッセージスレッド専用) ----
    static juce::File getUserPresetDir();
    void saveUserPreset(const juce::String& name, const juce::String& subCategory);
    bool loadUserPreset(const juce::File& file);
    void loadFactoryPreset(int index);
    void initPreset();
    void stepPreset(int delta);   // ◀▶: Factory+Userの結合リストを順送り
    juce::String getCurrentPresetName() const { return mCurrentPresetName; }

    // 現在のプリセットの「実体」。PRESETタブのブラウザがハイライトを
    // 外部変更 (ヘッダーの ◀▶ / ステート復元) へ追従させるのに使う。
    int getCurrentFactoryIndex() const noexcept { return mCurrentFactoryIndex; }
    juce::File getCurrentUserFile() const { return mCurrentUserFile; }

    // ---- RANDOM (メッセージスレッド専用) ----
    //  MAINタブ + OSC ENVタブのパラメーターとカーブを「音楽的に破綻しない範囲」で
    //  ランダマイズする。MASTERエリア / FX / CONFIG / FILTER は一切変更しない。
    //  lockOsc   : OSCのパラメーター(波形/レベル/ユニゾン/キー等)を据え置く
    //  lockCurves: カーブ(ENVの形)を据え置く
    //  小節数は setBarsLocked() の状態を見て自動的に据え置かれる。
    void randomizeMainAndOsc(bool lockOsc = false, bool lockCurves = false);

    //  MUTATE: 完全なランダムではなく、現在の設定を少しだけ揺らす。
    //   気に入った音を壊さずに近傍を探索できる (amount 0.05〜0.5 程度)。
    void mutateMainAndOsc(float amount);

    // ---- BARS ロック ----
    //  ON のあいだ、小節数はユーザーが決めた値のまま固定される。
    //   ・RANDOM でも変わらない
    //   ・プリセットを読み込んでも変わらない (◀▶ / ブラウザ / Init すべて)
    //  曲の尺に合わせて BARS を決めたあと、音色だけをプリセットで探したい
    //  という使い方のためのもの。
    //
    //  ※ セッション復元 (setStateInformation) には適用しない。
    //    保存したプロジェクトは保存時のとおりに戻る必要があるため。
    //  ※ ロック状態自体はプリセットではなくグローバル設定へ永続化する
    //    (プリセットに入れると「読み込むとロックが外れる」矛盾が起きる)。
    void setBarsLocked(bool shouldLock) noexcept { mLockBars = shouldLock; }
    bool isBarsLocked() const noexcept { return mLockBars; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();

    // ---- Key/Scale変更時の Start/End キー自動スナップ ----
    //  scaleOn / scaleKey / scaleType / osc{N}Scale のいずれかが変わったら、
    //  対象OSCのStart/Endキーをスケールの最寄り音へスナップする。
    //  スナップ後はユーザーが自由に変更でき、次にKey/Scaleを触るまで再スナップしない。
    //  プリセット/ステート復元では発火しない (適用直後にスナップ状態を記録するため)。
    //
    //  ※ RT安全性について:
    //   parameterChanged() はホストオートメーション経由だとオーディオスレッドから
    //   呼ばれる。以前はここで AsyncUpdater::triggerAsyncUpdate() を呼んでいたが、
    //   これは内部で MessageManager のキューにロックを取って push するため、
    //   オーディオスレッドでのロック取得 (優先度逆転) とアロケーションを招いていた。
    //   現在は atomic フラグを立てるだけにし、メッセージスレッドの Timer が拾う。
    struct SnapState
    {
        bool on = false;
        int  key = -1;
        int  type = -1;
        std::array<bool, RiserEngine::kNumOscs> oscApply { false, false, false };
        bool operator!=(const SnapState& o) const noexcept
        {
            return on != o.on || key != o.key || type != o.type || oscApply != o.oscApply;
        }
    };
    SnapState readSnapState() const;
    void parameterChanged(const juce::String& id, float newValue) override;
    void timerCallback() override;      // メッセージスレッド: mSnapDirty を拾う
    void snapKeysToScale();
    void rememberSnapState();

    SnapState mLastSnapState;
    // オーディオスレッドから立てられ、メッセージスレッドで消費される
    std::atomic<bool> mSnapDirty { false };
    // GUI/Panicボタンから立てられ、オーディオスレッドで消費される
    std::atomic<bool> mPanicRequested { false };

    // ---- BARS ロック (メッセージスレッド専用) ----
    //  captureBars() で読み込み前の値を控え、restoreBars() で書き戻す。
    //  ロックしていないときは -1 を返し、restoreBars 側が何もしない。
    bool  mLockBars = false;
    float captureBars() const;
    void  restoreBars(float savedNormalised);
    void gatherEngineParams(RiserEngine::Params& ep) const noexcept;
    void gatherFxParams(FxChain::Params& fp, double bpm, double ppq, bool playing) const noexcept;
    void applyStateTree(juce::ValueTree state);   // APVTS+カーブ+WTパスを適用
    juce::ValueTree buildStateTree();             // 現在の全ステートをツリー化

    juce::String mCurrentPresetName;
    int mCurrentFactoryIndex = -1;   // Factoryプリセット由来なら 0.. / それ以外 -1
    juce::File mCurrentUserFile;     // Userプリセット由来ならそのファイル

    // ---- DSPモジュール ----
    std::array<MorphWavetable, RiserEngine::kNumOscs> mWavetables;
    int mGcCounter = 0;
    RiserEngine mEngine;
    FxChain mFx;
    BrickLimiter mLimiter;
    CurveStore mCurves;

    juce::AudioFormatManager mFormatManager;

    // ---- フェイルセーフ用 ----
    bool mPrepared = false;
    double mPreparedSampleRate = 0.0;
    int mPreparedBlockSize = 0;
    int mMaxBlockSize = 0;      // 事前確保した最大ブロック長 (ホストの申告超過に耐える)

    // ソース別バス (OSC1-3 + Noise) — FXのソース別ルーティング用。
    //  prepareToPlay で mMaxBlockSize 分を事前確保する (processBlock内で確保しない)。
    std::array<std::vector<float>, RiserEngine::kNumSources> mBusL, mBusR;
    juce::LinearSmoothedValue<float> mMasterSm;

    // ---- MIDI Learn ----
    std::atomic<int> mLastNote { -1 };
    std::atomic<int> mNoteEvents { 0 };

    // 直近のホストBPM (getTailLengthSeconds はプレイヘッドを参照できないため保持)
    std::atomic<double> mLastBpm { 120.0 };

    // FXカーブ評価のセグメント探索ヒント (gatherFxParams は const なので mutable)
    mutable std::array<int, CurveStore::kNumCurves> mFxCurveHint {};

    // アウトプットメーター (ブロック毎のピーク)
    std::array<std::atomic<float>, 2> mOutPeak { };

    // ---- ライザー出力キャプチャ (prepareToPlayで事前確保) ----
    //  最大60秒。高SRでのメモリ肥大を防ぐためサンプル数の上限も併用する。
    //   44.1 / 48 / 88.2 / 96kHz : 60秒フル (上限に当たらない)
    //   176.4kHz : 約34秒 / 192kHz : 約31秒
    //  600万サンプル × 2ch × 4byte = 約48MB。
    static constexpr double kMaxCaptureSeconds = 60.0;
    static constexpr int    kMaxCaptureSamples = 6000000;   // 1chあたり (=48MB/2ch)

    //  テール終了判定: -90dBFS を kSilenceHoldSec 連続で下回ったら鳴り止んだとみなす。
    //  FDNリバーブは理論上は無限に減衰し続けるため、上限も併せて設ける。
    static constexpr float  kSilenceThresh   = 3.1623e-5f;  // -90 dBFS
    static constexpr double kSilenceHoldSec  = 0.10;        // 100ms 連続無音で確定
    //  テール単体の上限。実質の天井はキャプチャバッファ(60秒)側なので、
    //  ここは「万一無音判定が効かなかった場合」の保険として広めに取る。
    //  DECAY=1.0 の Shimmer (RT60≒21秒 → -90dB到達 約32秒) が自然減衰で
    //  収まり切るだけの余裕を確保している。
    static constexpr double kMaxTailSeconds  = 40.0;

    std::vector<float> mCapL, mCapR;
    int  mCapWrite = 0;
    int  mCapRiserLen = 0;     // 設定Bar分のサンプル数 (本編はここで打ち切り)
    int  mCapBodyEnd = 0;      // 本編終端 (テール開始位置)
    bool mCapturing = false;
    bool mCapInTail = false;   // 本編を録り終えてテール収録中か
    int  mCapSilentRun = 0;    // 連続して無音だったサンプル数
    int  mCapTailWritten = 0;  // テールとして書いたサンプル数
    int  mCapSilenceHold = 0;  // = kSilenceHoldSec * sr (prepareで算出)
    int  mCapTailCap = 0;      // = kMaxTailSeconds  * sr (prepareで算出)
    std::atomic<int>  mCapLenPub { 0 };
    std::atomic<int>  mCapBodyEndPub { 0 };
    std::atomic<int>  mCapVersion { 0 };
    std::atomic<bool> mCapActive { false };

    // ---- キャッシュ済みパラメーターポインタ ----
    std::atomic<float>* pLift = nullptr;
    std::atomic<float>* pLiftMode = nullptr;   // 0=Manual 1=Auto(Progress連動)
    std::atomic<float>* pReverse = nullptr;    // ENV評価位置の反転 (ライザー↔ダウナー)
    std::atomic<float>* pBars = nullptr;
    std::atomic<float>* pScaleOn = nullptr;    // Pitch ENV スケール量子化 マスターOn/Off
    std::atomic<float>* pScaleKey = nullptr;   // 0..11 (C..B)
    std::atomic<float>* pScaleType = nullptr;  // ScaleQuantizer インデックス
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pMaster = nullptr;

    struct OscPtrs
    {
        std::atomic<float> *on, *solo, *mute, *wave, *pos, *level,
                           *coarse, *fine, *uni, *det, *spread, *pan,
                           *keyStart, *keyEnd, *scaleQ;
    };
    std::array<OscPtrs, RiserEngine::kNumOscs> pOsc {};

    std::atomic<float> *pNoiseSolo = nullptr, *pNoiseMute = nullptr,
                       *pNoiseType = nullptr, *pNoiseLevel = nullptr,
                       *pNoisePitch = nullptr, *pNoiseRes = nullptr, *pNoiseRange = nullptr,
                       *pNoisePan = nullptr;

    struct FltPtrs
    {
        std::atomic<float> *on, *type, *cutoff, *res, *env;
        std::array<std::atomic<float>*, RiserEngine::kNumSources> route {};
    };
    std::array<FltPtrs, RiserEngine::kNumFilters> pFlt {};

    std::array<std::atomic<float>*, FxChain::kNumSlots> pFxType {};

    // FXのソース別ルーティング [効果 0:Sat 1:Cho 2:Dly 3:Rev 4:Duck][ソース 0-3]
    std::array<std::array<std::atomic<float>*, RiserEngine::kNumSources>,
               FxChain::kNumFxKinds> pFxRoute {};

    std::atomic<float> *pSatAmt = nullptr, *pSatAlgo = nullptr, *pSatDrive = nullptr,
                       *pSatPre = nullptr, *pSatTrim = nullptr;
    std::atomic<float> *pChoAmt = nullptr, *pChoRate = nullptr, *pChoDepth = nullptr, *pChoWidth = nullptr;
    std::atomic<float> *pDlyAmt = nullptr, *pDlyTime = nullptr, *pDlyFb = nullptr,
                       *pDlyDuck = nullptr, *pDlyDamp = nullptr;
    std::atomic<float> *pRevAmt = nullptr, *pRevDecay = nullptr, *pRevShimmer = nullptr,
                       *pRevDamp = nullptr, *pRevMod = nullptr;
    std::atomic<float> *pDuckAmt = nullptr, *pDuckRate = nullptr, *pDuckShape = nullptr;
    std::atomic<float> *pLimOn = nullptr, *pLimCeiling = nullptr, *pLimRelease = nullptr;

    // v0.5 追加: Key Follow / Velocity モジュレーション
    std::atomic<float> *pStutAmt = nullptr, *pStutRate = nullptr;
    std::atomic<float> *pHumanize = nullptr;
    std::atomic<float> *pKeyFollow = nullptr;
    std::atomic<float> *pVelToCutoff = nullptr, *pVelToNoise = nullptr, *pVelToDrive = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE(LiftXAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiftXAudioProcessor)
};
