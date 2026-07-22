// ==========================================
// File: PluginProcessor.h
// LIFT-X プロセッサー層
//  - DSPコアとGUIの完全分離 (DSPフォルダはGUIヘッダを一切includeしない)
//  - リアルタイム安全: processBlock内アロケーション/ロック禁止を徹底
//  - DAWフェイルセーフ: SR/ブロックサイズ不一致時の即時ゼロクリア+リセット
//  - カーブ(マルチENV)はAPVTS外のCurveStoreで管理し、ホストオートメーション
//    から完全隔離 (計画書のwithAutomatable(false)方針の強化版)
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
    const RiserEngine& getEngine() const noexcept { return mEngine; }
    float getUiProgress() const noexcept { return mEngine.uiProgress.load(std::memory_order_relaxed); }

    // ---- カスタムWavetable (メッセージスレッド専用 / SPECTRA8方式) ----
    bool loadCustomWavetable(const juce::File& file);
    void clearCustomWavetable();
    juce::String getCustomWavetablePath() const
    {
        return apvts.state.getProperty("customWavetablePath", juce::String()).toString();
    }
    bool hasCustomWavetable() const { return mWavetable.hasCustom(); }
    const MorphWavetable& getWavetable() const noexcept { return mWavetable; }

    // Bars選択肢 (計画書: 1,2,4,8,16)
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
    MorphWavetable mWavetable;
    RiserEngine mEngine;
    FxChain mFx;
    CurveStore mCurves;

    juce::AudioFormatManager mFormatManager;

    // ---- フェイルセーフ用 (Ableton Live のSR変更先行processBlock対策) ----
    bool mPrepared = false;
    double mPreparedSampleRate = 0.0;
    int mPreparedBlockSize = 0;

    // モノラルホスト用スクラッチ (prepareToPlayで事前確保)
    std::vector<float> mScratchR;

    // マスターゲイン平滑
    juce::LinearSmoothedValue<float> mMasterSm;

    // ---- キャッシュ済みパラメーターポインタ (processBlock内のルックアップ排除) ----
    std::atomic<float>* pLift = nullptr;
    std::atomic<float>* pBars = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pMaster = nullptr;

    struct OscPtrs { std::atomic<float> *on, *wave, *level, *coarse, *uni, *det, *spread, *range; };
    std::array<OscPtrs, RiserEngine::kNumOscs> pOsc {};

    std::atomic<float> *pNoiseType = nullptr, *pNoiseLevel = nullptr,
                       *pNoisePitch = nullptr, *pNoiseRes = nullptr, *pNoiseRange = nullptr;

    struct FltPtrs { std::atomic<float> *on, *type, *cutoff, *res, *env; };
    std::array<FltPtrs, RiserEngine::kNumFilters> pFlt {};

    struct FxSlotPtrs { std::atomic<float> *type, *amt, *env; };
    std::array<FxSlotPtrs, FxChain::kNumSlots> pFxSlot {};

    std::atomic<float> *pSatAlgo = nullptr, *pSatDrive = nullptr, *pSatPre = nullptr, *pSatTrim = nullptr;
    std::atomic<float> *pChoRate = nullptr, *pChoDepth = nullptr, *pChoWidth = nullptr;
    std::atomic<float> *pDlyTime = nullptr, *pDlyFb = nullptr, *pDlyDuck = nullptr, *pDlyDamp = nullptr;
    std::atomic<float> *pFrzSize = nullptr, *pFrzFb = nullptr, *pFrzDamp = nullptr;
    std::atomic<float> *pRevDecay = nullptr, *pRevShimmer = nullptr, *pRevDamp = nullptr, *pRevMod = nullptr;
    std::atomic<float> *pDuckRate = nullptr, *pDuckShape = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE(LiftXAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiftXAudioProcessor)
};
