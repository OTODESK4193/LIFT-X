// ==========================================
// File: PluginProcessor.h
// LIFT-X プロセッサー層 (v0.2)
//  - DSPコアとGUIの完全分離 / processBlock内アロケーション・ロック禁止
//  - DAWフェイルセーフ: SR/ブロックサイズ不一致時の即時ゼロクリア+リセット
//  - マルチENV(31系統)はAPVTS外のCurveStoreで管理 (オートメーション隔離)
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
                            private juce::AsyncUpdater
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
    double getTailLengthSeconds() const override { return 6.0; }

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
    //  ノートオンで録音開始、リリース完了+テール1.5秒で確定。
    //  GUIはバージョン増加を検知してコピーを取る (書き込み中の参照は表示専用)。
    const float* getCaptureL() const noexcept { return mCapL.data(); }
    const float* getCaptureR() const noexcept { return mCapR.data(); }
    int getCaptureLength() const noexcept { return mCapLenPub.load(std::memory_order_relaxed); }
    int getCaptureVersion() const noexcept { return mCapVersion.load(std::memory_order_relaxed); }
    bool isCapturing() const noexcept { return mCapActive.load(std::memory_order_relaxed); }
    double getPreparedSampleRate() const noexcept { return mPreparedSampleRate; }

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
        static const double b[10] = { 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0,
                                      1.0, 2.0, 4.0, 8.0, 16.0 };
        return b[juce::jlimit(0, 9, idx)];
    }

    // ENV評価位置 (GUIのプレイヘッド/ModBand用): Auto=Progress / Manual=LIFTノブ
    float getEnvPosition() const noexcept;

    // ---- プリセット (メッセージスレッド専用) ----
    static juce::File getUserPresetDir();
    void saveUserPreset(const juce::String& name, const juce::String& subCategory);
    bool loadUserPreset(const juce::File& file);
    void loadFactoryPreset(int index);
    void initPreset();
    void stepPreset(int delta);   // ◀▶: Factory+Userの結合リストを順送り
    juce::String getCurrentPresetName() const { return mCurrentPresetName; }

    // ---- RANDOM (メッセージスレッド専用) ----
    //  MAINタブ + OSC ENVタブのパラメーターとカーブを「音楽的に破綻しない範囲」で
    //  ランダマイズする。MASTERエリア / FX / CONFIG / FILTER は一切変更しない。
    void randomizeMainAndOsc();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();

    // ---- Key/Scale変更時の Start/End キー自動スナップ ----
    //  scaleOn / scaleKey / scaleType / osc{N}Scale のいずれかが変わったら、
    //  対象OSCのStart/Endキーをスケールの最寄り音へスナップする。
    //  スナップ後はユーザーが自由に変更でき、次にKey/Scaleを触るまで再スナップしない。
    //  プリセット/ステート復元では発火しない (適用直後にスナップ状態を記録するため)。
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
    void handleAsyncUpdate() override;
    void snapKeysToScale();
    void rememberSnapState();

    SnapState mLastSnapState;
    void gatherEngineParams(RiserEngine::Params& ep) const noexcept;
    void gatherFxParams(FxChain::Params& fp, double bpm, double ppq, bool playing) const noexcept;
    void applyStateTree(juce::ValueTree state);   // APVTS+カーブ+WTパスを適用
    juce::ValueTree buildStateTree();             // 現在の全ステートをツリー化

    juce::String mCurrentPresetName;
    int mCurrentFactoryIndex = -1;   // Factoryプリセット由来なら 0.. / それ以外 -1
    juce::File mCurrentUserFile;     // Userプリセット由来ならそのファイル

    // ---- DSPモジュール ----
    std::array<MorphWavetable, RiserEngine::kNumOscs> mWavetables;
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

    // ---- ライザー出力キャプチャ (prepareToPlayで事前確保) ----
    //  最大30秒。ただし高SRでのメモリ肥大を防ぐためサンプル数の上限も設ける。
    //  30秒×192kHz×2ch = 46MB になっていたため、上限を設けて約24MBへ抑える。
    //   44.1 / 48 / 88.2 / 96kHz : 30秒フル (上限に当たらない)
    //   176.4kHz : 約17秒 / 192kHz : 約15.6秒
    static constexpr double kMaxCaptureSeconds = 30.0;
    static constexpr int    kMaxCaptureSamples = 3000000;   // 1chあたり (=24MB/2ch)
    std::vector<float> mCapL, mCapR;
    int mCapWrite = 0;
    int mCapRiserLen = 0;      // 設定Bar分のサンプル数 (本編はここで打ち切り)
    bool mCapturing = false;
    bool mWasActive = false;
    int mTailRemain = -1;
    std::atomic<int> mCapLenPub { 0 };
    std::atomic<int> mCapVersion { 0 };
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
                           *coarse, *uni, *det, *spread, *keyStart, *keyEnd, *scaleQ;
    };
    std::array<OscPtrs, RiserEngine::kNumOscs> pOsc {};

    std::atomic<float> *pNoiseSolo = nullptr, *pNoiseMute = nullptr,
                       *pNoiseType = nullptr, *pNoiseLevel = nullptr,
                       *pNoisePitch = nullptr, *pNoiseRes = nullptr, *pNoiseRange = nullptr;

    struct FltPtrs
    {
        std::atomic<float> *on, *type, *cutoff, *res, *env;
        std::array<std::atomic<float>*, RiserEngine::kNumSources> route {};
    };
    std::array<FltPtrs, RiserEngine::kNumFilters> pFlt {};

    std::array<std::atomic<float>*, FxChain::kNumSlots> pFxType {};

    // FXのソース別ルーティング [効果 0:Sat 1:Cho 2:Dly 3:Rev 4:Duck][ソース 0-3]
    std::array<std::array<std::atomic<float>*, RiserEngine::kNumSources>, 5> pFxRoute {};

    std::atomic<float> *pSatAmt = nullptr, *pSatAlgo = nullptr, *pSatDrive = nullptr,
                       *pSatPre = nullptr, *pSatTrim = nullptr;
    std::atomic<float> *pChoAmt = nullptr, *pChoRate = nullptr, *pChoDepth = nullptr, *pChoWidth = nullptr;
    std::atomic<float> *pDlyAmt = nullptr, *pDlyTime = nullptr, *pDlyFb = nullptr,
                       *pDlyDuck = nullptr, *pDlyDamp = nullptr;
    std::atomic<float> *pRevAmt = nullptr, *pRevDecay = nullptr, *pRevShimmer = nullptr,
                       *pRevDamp = nullptr, *pRevMod = nullptr;
    std::atomic<float> *pDuckAmt = nullptr, *pDuckRate = nullptr, *pDuckShape = nullptr;
    std::atomic<float> *pLimOn = nullptr, *pLimCeiling = nullptr, *pLimRelease = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE(LiftXAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiftXAudioProcessor)
};
