// ==========================================
// File: PluginProcessor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstring>

// ---- グローバル設定ファイル (SPECTRA8方式) ----
namespace
{
    struct GlobalSettingsHolder
    {
        juce::ApplicationProperties props;
        GlobalSettingsHolder()
        {
            juce::PropertiesFile::Options o;
            o.applicationName     = "LIFT-X";
            o.filenameSuffix      = "settings";
            o.folderName          = "LIFT-X";
            o.osxLibrarySubFolder = "Application Support";
            o.storageFormat       = juce::PropertiesFile::storeAsXML;
            props.setStorageParameters(o);
        }
    };

    juce::NormalisableRange<float> logRange(float lo, float hi)
    {
        juce::NormalisableRange<float> r(lo, hi);
        r.setSkewForCentre(std::sqrt(lo * hi));
        return r;
    }

    // ---- 表示単位の簡素化 (内部解像度はフルのまま) ----
    juce::String pctStr(float v, int)   { return juce::String((int)std::round(v * 100.0f)) + "%"; }
    juce::String hzStr(float v, int)    { return v < 1000.0f ? juce::String((int)std::round(v)) + "Hz"
                                                             : juce::String(v / 1000.0f, 1) + "k"; }
    juce::String msStr(float v, int)    { return v < 1000.0f ? juce::String((int)std::round(v)) + "ms"
                                                             : juce::String(v / 1000.0f, 1) + "s"; }
    juce::String dbStr(float v, int)    { return juce::String(v, 1) + "dB"; }
    juce::String stStr(float v, int)    { return juce::String((int)std::round(v)) + "st"; }
    juce::String ctStr(float v, int)    { return juce::String((int)std::round(v)) + "ct"; }
    juce::String octStr(float v, int)   { return juce::String(v, 1) + "oct"; }
    juce::String plainStr(float v, int) { return juce::String(v, 1); }

    juce::AudioParameterFloatAttributes attr(juce::String (*fn)(float, int))
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [fn](float v, int len) { return fn(v, len); });
    }

    juce::String noteName(int v)
    {
        return juce::MidiMessage::getMidiNoteName(v, true, true, 3); // C3=60表記
    }
}

juce::PropertiesFile& LiftXAudioProcessor::getGlobalSettings()
{
    static GlobalSettingsHolder holder;
    return *holder.props.getUserSettings();
}

juce::String LiftXAudioProcessor::getGlobalWavetableDir()
{
    return getGlobalSettings().getValue("customWavetableDir", juce::String());
}

void LiftXAudioProcessor::setGlobalWavetableDir(const juce::String& path)
{
    auto& s = getGlobalSettings();
    s.setValue("customWavetableDir", path);
    s.saveIfNeeded();   // 即時ディスク書き込み
}

// ==========================================================
// コンストラクタ
// ==========================================================
LiftXAudioProcessor::LiftXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    mFormatManager.registerBasicFormats();
    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
        mEngine.setWavetable(i, &mWavetables[(size_t)i]);
    cacheParameterPointers();
}

LiftXAudioProcessor::~LiftXAudioProcessor() = default;

// ==========================================================
// パラメーターレイアウト (カーブ=マルチENVはCurveStoreで管理しここに置かない)
// ==========================================================
juce::AudioProcessorValueTreeState::ParameterLayout LiftXAudioProcessor::createParameterLayout()
{
    using FloatP = juce::AudioParameterFloat;
    using ChoiceP = juce::AudioParameterChoice;
    using BoolP = juce::AudioParameterBool;
    using IntP = juce::AudioParameterInt;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto add = [&params](auto p) { params.push_back(std::move(p)); };

    const auto noteAttr = juce::AudioParameterIntAttributes().withStringFromValueFunction(
        [](int v, int) { return noteName(v); });

    // ---- グローバル ----
    add(std::make_unique<FloatP>(juce::ParameterID{"lift", 1}, "LIFT",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, attr(pctStr)));
    add(std::make_unique<BoolP>(juce::ParameterID{"liftMode", 1}, "LIFT Auto", false));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"bars", 1}, "Bars",
        juce::StringArray{"1", "2", "4", "8", "16"}, 2));
    add(std::make_unique<FloatP>(juce::ParameterID{"attack", 1}, "Attack",
        logRange(0.1f, 500.0f), 3.0f, attr(msStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"release", 1}, "Release",
        logRange(5.0f, 4000.0f), 200.0f, attr(msStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"master", 1}, "Master",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, attr(dbStr)));

    // ---- オシレーター 1-3 ----
    for (int i = 1; i <= RiserEngine::kNumOscs; ++i)
    {
        const juce::String n(i);
        add(std::make_unique<BoolP>(juce::ParameterID{"osc" + n + "On", 1}, "Osc" + n + " On", i == 1));
        add(std::make_unique<BoolP>(juce::ParameterID{"osc" + n + "Solo", 1}, "Osc" + n + " Solo", false));
        add(std::make_unique<BoolP>(juce::ParameterID{"osc" + n + "Mute", 1}, "Osc" + n + " Mute", false));
        add(std::make_unique<ChoiceP>(juce::ParameterID{"osc" + n + "Wave", 1}, "Osc" + n + " Wave",
            juce::StringArray{"Sine", "Triangle", "Square", "Saw", "FM", "Wavetable"}, 3));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Pos", 1}, "Osc" + n + " WT Pos",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Level", 1}, "Osc" + n + " Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f, attr(pctStr)));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "Coarse", 1}, "Osc" + n + " Coarse",
            -24, 24, 0, juce::AudioParameterIntAttributes().withStringFromValueFunction(
                [](int v, int) { return juce::String(v) + "st"; })));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "Uni", 1}, "Osc" + n + " Unison",
            1, RiserEngine::kMaxUnison, 1));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Det", 1}, "Osc" + n + " Detune",
            juce::NormalisableRange<float>(0.0f, 100.0f), 12.0f, attr(ctStr)));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Spread", 1}, "Osc" + n + " Spread",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f, attr(pctStr)));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "KeyStart", 1}, "Osc" + n + " Start Key",
            0, 127, 36, noteAttr));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "KeyEnd", 1}, "Osc" + n + " End Key",
            0, 127, 84, noteAttr));
    }

    // ---- ノイズ ----
    add(std::make_unique<BoolP>(juce::ParameterID{"noiseSolo", 1}, "Noise Solo", false));
    add(std::make_unique<BoolP>(juce::ParameterID{"noiseMute", 1}, "Noise Mute", false));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"noiseType", 1}, "Noise Type",
        juce::StringArray{"White", "Pink", "Brown"}, 1));
    add(std::make_unique<FloatP>(juce::ParameterID{"noiseLevel", 1}, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"noisePitch", 1}, "Noise Pitch",
        logRange(20.0f, 20000.0f), 500.0f, attr(hzStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"noiseRes", 1}, "Noise Res",
        juce::NormalisableRange<float>(0.5f, 12.0f), 2.0f, attr(plainStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"noiseRange", 1}, "Noise Range",
        juce::NormalisableRange<float>(0.0f, 10.0f), 5.0f, attr(octStr)));

    // ---- フィルター 1-4 (ZDF/TPT + ソース別ルーティング) ----
    static const char* srcNames[4] = { "Osc1", "Osc2", "Osc3", "Noise" };
    for (int i = 1; i <= RiserEngine::kNumFilters; ++i)
    {
        const juce::String n(i);
        add(std::make_unique<BoolP>(juce::ParameterID{"flt" + n + "On", 1}, "Filter" + n + " On", i == 1));
        add(std::make_unique<ChoiceP>(juce::ParameterID{"flt" + n + "Type", 1}, "Filter" + n + " Type",
            juce::StringArray{"LowPass", "HighPass", "BandPass", "Notch"}, 0));
        add(std::make_unique<FloatP>(juce::ParameterID{"flt" + n + "Cutoff", 1}, "Filter" + n + " Cutoff",
            logRange(20.0f, 20000.0f), 1000.0f, attr(hzStr)));
        add(std::make_unique<FloatP>(juce::ParameterID{"flt" + n + "Res", 1}, "Filter" + n + " Res",
            juce::NormalisableRange<float>(0.5f, 12.0f), 0.9f, attr(plainStr)));
        add(std::make_unique<FloatP>(juce::ParameterID{"flt" + n + "Env", 1}, "Filter" + n + " Env",
            juce::NormalisableRange<float>(-1.0f, 1.0f), i == 1 ? 0.5f : 0.0f, attr(pctStr)));
        for (int s = 0; s < RiserEngine::kNumSources; ++s)
            add(std::make_unique<BoolP>(
                juce::ParameterID{"flt" + n + "Route" + srcNames[s], 1},
                "Filter" + n + " " + srcNames[s], true));
    }

    // ---- FXスロット 1-5 (適用順序) ----
    for (int i = 1; i <= FxChain::kNumSlots; ++i)
        add(std::make_unique<ChoiceP>(juce::ParameterID{"fx" + juce::String(i) + "Type", 1},
            "FX Slot" + juce::String(i), FxChain::getTypeNames(), 0));

    // ---- FXパラメーター (FX毎) ----
    add(std::make_unique<FloatP>(juce::ParameterID{"satAmt", 1}, "Sat Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"satAlgo", 1}, "Sat Algo", FxChain::getSatAlgoNames(), 0));
    add(std::make_unique<FloatP>(juce::ParameterID{"satDrive", 1}, "Sat Drive",
        juce::NormalisableRange<float>(1.0f, 12.0f), 2.0f, attr(plainStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"satPre", 1}, "Sat PreHPF",
        logRange(20.0f, 2000.0f), 20.0f, attr(hzStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"satTrim", 1}, "Sat Trim",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, attr(dbStr)));

    add(std::make_unique<FloatP>(juce::ParameterID{"choAmt", 1}, "Chorus Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"choRate", 1}, "Chorus Rate",
        logRange(0.05f, 8.0f), 0.8f, juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + "Hz"; })));
    add(std::make_unique<FloatP>(juce::ParameterID{"choDepth", 1}, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"choWidth", 1}, "Chorus Width",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, attr(pctStr)));

    add(std::make_unique<FloatP>(juce::ParameterID{"dlyAmt", 1}, "Delay Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"dlyTime", 1}, "Delay Time", FxChain::getDelayTimeNames(), 5));
    add(std::make_unique<FloatP>(juce::ParameterID{"dlyFb", 1}, "Delay FB",
        juce::NormalisableRange<float>(0.0f, 0.95f), 0.45f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"dlyDuck", 1}, "Delay Duck",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"dlyDamp", 1}, "Delay Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f, attr(pctStr)));

    add(std::make_unique<FloatP>(juce::ParameterID{"revAmt", 1}, "Reverb Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"revDecay", 1}, "Reverb Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"revShimmer", 1}, "Reverb Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"revDamp", 1}, "Reverb Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"revMod", 1}, "Reverb Mod",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f, attr(pctStr)));

    add(std::make_unique<FloatP>(juce::ParameterID{"duckAmt", 1}, "Duck Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"duckRate", 1}, "Duck Rate", FxChain::getDuckRateNames(), 5));
    add(std::make_unique<FloatP>(juce::ParameterID{"duckShape", 1}, "Duck Shape",
        juce::NormalisableRange<float>(0.5f, 8.0f), 2.0f, attr(plainStr)));

    return { params.begin(), params.end() };
}

void LiftXAudioProcessor::cacheParameterPointers()
{
    auto p = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };

    pLift = p("lift");
    pLiftMode = p("liftMode");
    pBars = p("bars");
    pAttack = p("attack");
    pRelease = p("release");
    pMaster = p("master");

    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
    {
        const juce::String n(i + 1);
        pOsc[(size_t)i] = { p("osc" + n + "On"), p("osc" + n + "Solo"), p("osc" + n + "Mute"),
                            p("osc" + n + "Wave"), p("osc" + n + "Pos"), p("osc" + n + "Level"),
                            p("osc" + n + "Coarse"), p("osc" + n + "Uni"), p("osc" + n + "Det"),
                            p("osc" + n + "Spread"), p("osc" + n + "KeyStart"), p("osc" + n + "KeyEnd") };
    }

    pNoiseSolo = p("noiseSolo");
    pNoiseMute = p("noiseMute");
    pNoiseType = p("noiseType");
    pNoiseLevel = p("noiseLevel");
    pNoisePitch = p("noisePitch");
    pNoiseRes = p("noiseRes");
    pNoiseRange = p("noiseRange");

    static const char* srcNames[4] = { "Osc1", "Osc2", "Osc3", "Noise" };
    for (int i = 0; i < RiserEngine::kNumFilters; ++i)
    {
        const juce::String n(i + 1);
        pFlt[(size_t)i].on = p("flt" + n + "On");
        pFlt[(size_t)i].type = p("flt" + n + "Type");
        pFlt[(size_t)i].cutoff = p("flt" + n + "Cutoff");
        pFlt[(size_t)i].res = p("flt" + n + "Res");
        pFlt[(size_t)i].env = p("flt" + n + "Env");
        for (int s = 0; s < RiserEngine::kNumSources; ++s)
            pFlt[(size_t)i].route[(size_t)s] = p("flt" + n + "Route" + srcNames[s]);
    }

    for (int i = 0; i < FxChain::kNumSlots; ++i)
        pFxType[(size_t)i] = p("fx" + juce::String(i + 1) + "Type");

    pSatAmt = p("satAmt");   pSatAlgo = p("satAlgo"); pSatDrive = p("satDrive");
    pSatPre = p("satPre");   pSatTrim = p("satTrim");
    pChoAmt = p("choAmt");   pChoRate = p("choRate"); pChoDepth = p("choDepth"); pChoWidth = p("choWidth");
    pDlyAmt = p("dlyAmt");   pDlyTime = p("dlyTime"); pDlyFb = p("dlyFb");
    pDlyDuck = p("dlyDuck"); pDlyDamp = p("dlyDamp");
    pRevAmt = p("revAmt");   pRevDecay = p("revDecay"); pRevShimmer = p("revShimmer");
    pRevDamp = p("revDamp"); pRevMod = p("revMod");
    pDuckAmt = p("duckAmt"); pDuckRate = p("duckRate"); pDuckShape = p("duckShape");
}

// ==========================================================
// prepareToPlay
// ==========================================================
void LiftXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mPreparedSampleRate = sampleRate;
    mPreparedBlockSize = juce::jmax(16, samplesPerBlock);

    mEngine.prepare(sampleRate);
    mFx.prepare(sampleRate);

    mScratchR.assign((size_t)mPreparedBlockSize, 0.0f);

    // キャプチャバッファ (最大30秒・ステレオ)
    const size_t capSize = (size_t)(sampleRate * kMaxCaptureSeconds);
    mCapL.assign(capSize, 0.0f);
    mCapR.assign(capSize, 0.0f);
    mCapWrite = 0;
    mCapturing = false;
    mWasActive = false;
    mTailRemain = -1;
    mCapLenPub.store(0);
    mCapVersion.fetch_add(1);
    mCapActive.store(false);

    mMasterSm.reset(sampleRate, 0.02);
    mMasterSm.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(pMaster != nullptr ? pMaster->load() : 0.0f));

    mPrepared = true;
}

void LiftXAudioProcessor::releaseResources()
{
    mPrepared = false;
}

bool LiftXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

// ==========================================================
// processBlock
// ==========================================================
void LiftXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // ---- DAWフェイルセーフ層 ----
    if (!mPrepared
        || std::abs(getSampleRate() - mPreparedSampleRate) > 0.5
        || numSamples > mPreparedBlockSize
        || numSamples <= 0)
    {
        buffer.clear();
        mEngine.hardReset();
        return;
    }

    buffer.clear();

    // ---- トランスポート情報 ----
    bool playing = false, hasPpq = false;
    double ppq = 0.0, bpm = 120.0, qnPerBar = 4.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
            if (auto ts = pos->getTimeSignature())
                qnPerBar = 4.0 * (double)ts->numerator / juce::jmax(1.0, (double)ts->denominator);
        }
    }

    // ---- MIDI ----
    bool noteOnThisBlock = false;
    const double qnPerSample = (bpm / 60.0) / mPreparedSampleRate;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            noteOnThisBlock = true;
            const double ppqAtEvent = hasPpq && playing
                ? ppq + (double)meta.samplePosition * qnPerSample : ppq;
            mEngine.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), ppqAtEvent, playing && hasPpq);

            // MIDI Learn 用 (GUIがStartKey/EndKey設定に使用)
            mLastNote.store(msg.getNoteNumber(), std::memory_order_relaxed);
            mNoteEvents.fetch_add(1, std::memory_order_relaxed);
        }
        else if (msg.isNoteOff())
        {
            mEngine.noteOff(msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            mEngine.allNotesOff();
        }
    }

    // ---- DAW同期 ----
    const int bars = barsFromChoice((int)pBars->load());
    mEngine.syncTransport(playing, hasPpq, ppq, bpm, qnPerBar, bars);

    // ---- エンジンレンダリング ----
    RiserEngine::Params ep;
    gatherEngineParams(ep);

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : mScratchR.data();
    if (buffer.getNumChannels() <= 1)
        juce::FloatVectorOperations::clear(mScratchR.data(), numSamples);

    mEngine.render(L, R, numSamples, ep, mCurves);

    if (buffer.getNumChannels() <= 1)
        juce::FloatVectorOperations::addWithMultiply(L, mScratchR.data(), 0.5f, numSamples);

    // ---- FXチェーン (カーブ変調をブロックレートで合成) ----
    FxChain::Params fp;
    gatherFxParams(fp, bpm, ppq, playing);
    mFx.process(buffer, fp);

    // ---- マスターゲイン + セーフティクリップ ----
    mMasterSm.setTargetValue(juce::Decibels::decibelsToGain(pMaster->load()));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer(ch);
        auto sm = mMasterSm;
        for (int i = 0; i < numSamples; ++i)
        {
            float v = d[i] * sm.getNextValue();
            if (v > 2.0f) v = 2.0f + std::tanh(v - 2.0f) * 0.1f;
            else if (v < -2.0f) v = -2.0f + std::tanh(v + 2.0f) * 0.1f;
            d[i] = v;
        }
    }
    mMasterSm.skip(numSamples);

    // ---- ライザー出力キャプチャ (プラグイン最終出力 / RT安全: memcpyのみ) ----
    {
        const bool act = mEngine.isNoteActive();

        if (noteOnThisBlock)
        {
            // 新しいライザー開始 → 録音をやり直す
            mCapWrite = 0;
            mCapturing = true;
            mTailRemain = -1;
            mCapActive.store(true, std::memory_order_relaxed);
        }

        if (mCapturing)
        {
            const int cap = (int)mCapL.size();
            const int nWrite = juce::jmin(numSamples, cap - mCapWrite);
            if (nWrite > 0)
            {
                const float* sl = buffer.getReadPointer(0);
                const float* sr2 = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : sl;
                std::memcpy(mCapL.data() + mCapWrite, sl, (size_t)nWrite * sizeof(float));
                std::memcpy(mCapR.data() + mCapWrite, sr2, (size_t)nWrite * sizeof(float));
                mCapWrite += nWrite;
            }
            mCapLenPub.store(mCapWrite, std::memory_order_relaxed);

            // リリース完了後はFXテールを1.5秒だけ録ってから確定
            if (!act)
            {
                if (mTailRemain < 0)
                    mTailRemain = (int)(mPreparedSampleRate * 1.5);
                mTailRemain -= numSamples;
            }
            else
            {
                mTailRemain = -1;
            }

            if ((!act && mTailRemain <= 0) || mCapWrite >= cap)
            {
                mCapturing = false;
                mCapActive.store(false, std::memory_order_relaxed);
                mCapVersion.fetch_add(1, std::memory_order_release);
            }
        }

        mWasActive = act;
    }
}

// ==========================================================
// パラメーター収集 (RT安全: atomic load のみ)
// ==========================================================
void LiftXAudioProcessor::gatherEngineParams(RiserEngine::Params& ep) const noexcept
{
    ep.lift = pLift->load();
    ep.liftAuto = pLiftMode->load() > 0.5f;
    ep.attackMs = pAttack->load();
    ep.releaseMs = pRelease->load();

    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
    {
        auto& o = ep.osc[(size_t)i];
        const auto& q = pOsc[(size_t)i];
        o.on = q.on->load() > 0.5f;
        o.solo = q.solo->load() > 0.5f;
        o.mute = q.mute->load() > 0.5f;
        o.waveMode = (int)q.wave->load();
        o.pos = q.pos->load();
        o.level = q.level->load();
        o.coarse = q.coarse->load();
        o.unison = (int)q.uni->load();
        o.detune = q.det->load();
        o.spread = q.spread->load();
        o.keyStart = (int)q.keyStart->load();
        o.keyEnd = (int)q.keyEnd->load();
    }

    ep.noiseSolo = pNoiseSolo->load() > 0.5f;
    ep.noiseMute = pNoiseMute->load() > 0.5f;
    ep.noiseType = (int)pNoiseType->load();
    ep.noiseLevel = pNoiseLevel->load();
    ep.noisePitch = pNoisePitch->load();
    ep.noiseRes = pNoiseRes->load();
    ep.noiseRangeOct = pNoiseRange->load();

    for (int i = 0; i < RiserEngine::kNumFilters; ++i)
    {
        auto& f = ep.flt[(size_t)i];
        const auto& q = pFlt[(size_t)i];
        f.on = q.on->load() > 0.5f;
        f.type = (int)q.type->load();
        f.cutoff = q.cutoff->load();
        f.res = q.res->load();
        f.env = q.env->load();
        for (int s = 0; s < RiserEngine::kNumSources; ++s)
            f.route[(size_t)s] = q.route[(size_t)s]->load() > 0.5f;
    }
}

void LiftXAudioProcessor::gatherFxParams(FxChain::Params& fp, double bpm, double ppq, bool playing) const noexcept
{
    fp.bpm = bpm;
    fp.ppq = ppq;
    fp.playing = playing;

    for (int s = 0; s < FxChain::kNumSlots; ++s)
        fp.type[(size_t)s] = (int)pFxType[(size_t)s]->load();

    // マルチENVカーブによるバイポーラ加算変調 (中央=ノブ値, ±レンジ半分)
    //  LIFT Auto時はProgress自体がLIFTになる (エンジンと同一規則)
    const float prog = mEngine.getProgressF();
    const bool liftAuto = pLiftMode->load() > 0.5f;
    const float lift = juce::jlimit(0.0f, 1.0f, liftAuto ? prog : pLift->load());
    auto bip = [this, prog, lift](int idx) noexcept
    {
        const float y = mCurves.read(idx).evaluate(prog);
        return (y - 0.5f) * 2.0f * lift;
    };
    auto c01 = [](float v) noexcept { return juce::jlimit(0.0f, 1.0f, v); };

    fp.satAmt = c01(pSatAmt->load() + bip(CurveStore::SatAmt) * 0.5f);
    fp.satAlgo = (int)pSatAlgo->load();
    fp.satDrive = juce::jlimit(1.0f, 12.0f, pSatDrive->load() + bip(CurveStore::SatDrive) * 5.5f);
    fp.satPreHz = pSatPre->load();
    fp.satTrimDb = pSatTrim->load();

    fp.choAmt = c01(pChoAmt->load() + bip(CurveStore::ChoAmt) * 0.5f);
    fp.choRate = pChoRate->load();
    fp.choDepth = c01(pChoDepth->load() + bip(CurveStore::ChoDepth) * 0.5f);
    fp.choWidth = pChoWidth->load();

    fp.dlyAmt = c01(pDlyAmt->load() + bip(CurveStore::DlyAmt) * 0.5f);
    fp.dlyFeedback = juce::jlimit(0.0f, 0.95f, pDlyFb->load() + bip(CurveStore::DlyFb) * 0.475f);
    fp.dlyDuck = pDlyDuck->load();
    fp.dlyDamp = pDlyDamp->load();
    // TIME: カーブで拍長を±2オクターブ変調 (上=長く / 下=短く=加速)
    fp.dlyBeats = FxChain::delayTimeToBeats((int)pDlyTime->load())
                * std::exp2(bip(CurveStore::DlyTime) * 2.0f);

    fp.revAmt = c01(pRevAmt->load() + bip(CurveStore::RevAmt) * 0.5f);
    fp.revDecay = pRevDecay->load();
    fp.revShimmer = c01(pRevShimmer->load() + bip(CurveStore::RevShimmer) * 0.5f);
    fp.revDamp = pRevDamp->load();
    fp.revMod = pRevMod->load();

    fp.duckAmt = c01(pDuckAmt->load() + bip(CurveStore::DuckAmt) * 0.5f);
    // RATE: ±2オクターブを音楽的に量子化 (×4..×1/4)
    fp.duckBeats = FxChain::duckRateToBeats((int)pDuckRate->load())
                 * std::exp2((float)juce::roundToInt(bip(CurveStore::DuckRate) * 2.0f));
    fp.duckShape = juce::jlimit(0.5f, 8.0f, pDuckShape->load() + bip(CurveStore::DuckShape) * 3.75f);
}

// ==========================================================
// カスタムWavetable (OSC毎 / メッセージスレッド専用)
// ==========================================================
bool LiftXAudioProcessor::loadCustomWavetable(int oscIdx, const juce::File& file)
{
    oscIdx = juce::jlimit(0, RiserEngine::kNumOscs - 1, oscIdx);
    if (!file.existsAsFile()) return false;

    std::unique_ptr<juce::AudioFormatReader> reader(mFormatManager.createReaderFor(file));
    if (reader == nullptr) return false;

    const int numSamples = (int)juce::jmin<juce::int64>(reader->lengthInSamples,
        (juce::int64)(MorphWavetable::kMaxCustomFrames * MorphWavetable::kTableSize));
    if (numSamples < 16) return false;

    juce::AudioBuffer<float> tmp((int)reader->numChannels, numSamples);
    reader->read(&tmp, 0, numSamples, 0, true, true);

    std::vector<float> mono((size_t)numSamples, 0.0f);
    const float chNorm = 1.0f / (float)juce::jmax(1u, (juce::uint32)reader->numChannels);
    for (int ch = 0; ch < (int)reader->numChannels; ++ch)
    {
        const float* src = tmp.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            mono[(size_t)i] += src[i] * chNorm;
    }

    if (!mWavetables[(size_t)oscIdx].loadCustomFromBuffer(mono.data(), numSamples))
        return false;

    apvts.state.setProperty("customWavetablePath" + juce::String(oscIdx + 1),
                            file.getFullPathName(), nullptr);
    return true;
}

void LiftXAudioProcessor::clearCustomWavetable(int oscIdx)
{
    oscIdx = juce::jlimit(0, RiserEngine::kNumOscs - 1, oscIdx);
    mWavetables[(size_t)oscIdx].clearCustom();
    apvts.state.removeProperty("customWavetablePath" + juce::String(oscIdx + 1), nullptr);
}

// ==========================================================
// ステート保存/復元 (APVTS + カーブ + WTパス)
// ==========================================================
void LiftXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    for (int i = state.getNumChildren() - 1; i >= 0; --i)
        if (state.getChild(i).hasType("CURVES"))
            state.removeChild(i, nullptr);
    state.appendChild(mCurves.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void LiftXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    mCurves.fromValueTree(state.getChildWithName("CURVES"));
    apvts.replaceState(state);

    // OSC毎のカスタムWavetable復元
    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
    {
        const auto path = getCustomWavetablePath(i);
        if (path.isNotEmpty())
        {
            const juce::File f(path);
            if (f.existsAsFile())
                loadCustomWavetable(i, f);
        }
    }
}

// ==========================================================
juce::AudioProcessorEditor* LiftXAudioProcessor::createEditor()
{
    return new LiftXAudioProcessorEditor(*this);
}

// ==========================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LiftXAudioProcessor();
}
