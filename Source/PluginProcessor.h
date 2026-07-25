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
#include "DSP/RiserEngine.h"
#include "DSP/FxChain.h"

class LiftXAudioProcessor : public juce::AudioProcessor
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

    // Bars選択肢 (1,2,4,8,16)
    static int barsFromChoice(int idx) noexcept
    {
        static const int b[5] = { 1, 2, 4, 8, 16 };
        return b[juce::jlimit(0, 4, idx)];
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();
    void gatherEngineParams(RiserEngine::Params& ep) const noexcept;
    void gatherFxParams(FxChain::Params& fp, double bpm, double ppq, bool playing) const noexcept;

    // ---- DSPモジュール ----
    std::array<MorphWavetable, RiserEngine::kNumOscs> mWavetables;
    RiserEngine mEngine;
    FxChain mFx;
    CurveStore mCurves;

    juce::AudioFormatManager mFormatManager;

    // ---- フェイルセーフ用 ----
    bool mPrepared = false;
    double mPreparedSampleRate = 0.0;
    int mPreparedBlockSize = 0;

    std::vector<float> mScratchR;
    juce::LinearSmoothedValue<float> mMasterSm;

    // ---- MIDI Learn ----
    std::atomic<int> mLastNote { -1 };
    std::atomic<int> mNoteEvents { 0 };

    // ---- ライザー出力キャプチャ (prepareToPlayで事前確保・最大30秒) ----
    static constexpr double kMaxCaptureSeconds = 30.0;
    std::vector<float> mCapL, mCapR;
    int mCapWrite = 0;
    bool mCapturing = false;
    bool mWasActive = false;
    int mTailRemain = -1;
    std::atomic<int> mCapLenPub { 0 };
    std::atomic<int> mCapVersion { 0 };
    std::atomic<bool> mCapActive { false };

    // ---- キャッシュ済みパラメーターポインタ ----
    std::atomic<float>* pLift = nullptr;
    std::atomic<float>* pLiftMode = nullptr;   // 0=Manual 1=Auto(Progress連動)
    std::atomic<float>* pBars = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pMaster = nullptr;

    struct OscPtrs
    {
        std::atomic<float> *on, *solo, *mute, *wave, *pos, *level,
                           *coarse, *uni, *det, *spread, *keyStart, *keyEnd;
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

    std::atomic<float> *pSatAmt = nullptr, *pSatAlgo = nullptr, *pSatDrive = nullptr,
                       *pSatPre = nullptr, *pSatTrim = nullptr;
    std::atomic<float> *pChoAmt = nullptr, *pChoRate = nullptr, *pChoDepth = nullptr, *pChoWidth = nullptr;
    std::atomic<float> *pDlyAmt = nullptr, *pDlyTime = nullptr, *pDlyFb = nullptr,
                       *pDlyDuck = nullptr, *pDlyDamp = nullptr;
    std::atomic<float> *pRevAmt = nullptr, *pRevDecay = nullptr, *pRevShimmer = nullptr,
                       *pRevDamp = nullptr, *pRevMod = nullptr;
    std::atomic<float> *pDuckAmt = nullptr, *pDuckRate = nullptr, *pDuckShape = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE(LiftXAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiftXAudioProcessor)
};
