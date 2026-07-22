// ==========================================
// File: PluginProcessor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::NormalisableRange<float> logRange(float lo, float hi)
    {
        juce::NormalisableRange<float> r(lo, hi);
        r.setSkewForCentre(std::sqrt(lo * hi));
        return r;
    }
}

// ==========================================================
// コンストラクタ
// ==========================================================
LiftXAudioProcessor::LiftXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    mFormatManager.registerBasicFormats();
    mEngine.setWavetable(&mWavetable);
    cacheParameterPointers();
}

LiftXAudioProcessor::~LiftXAudioProcessor() = default;

// ==========================================================
// パラメーターレイアウト
//  カーブ(マルチENV)はここに置かない: CurveStoreで管理し、
//  ホストオートメーションから完全に隔離する (巻き戻り現象対策)。
// ==========================================================
juce::AudioProcessorValueTreeState::ParameterLayout LiftXAudioProcessor::createParameterLayout()
{
    using FloatP = juce::AudioParameterFloat;
    using ChoiceP = juce::AudioParameterChoice;
    using BoolP = juce::AudioParameterBool;
    using IntP = juce::AudioParameterInt;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto add = [&params](auto p) { params.push_back(std::move(p)); };

    // ---- グローバル ----
    add(std::make_unique<FloatP>(juce::ParameterID{"lift", 1}, "LIFT",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"bars", 1}, "Bars",
        juce::StringArray{"1", "2", "4", "8", "16"}, 2));
    add(std::make_unique<FloatP>(juce::ParameterID{"attack", 1}, "Attack",
        logRange(0.1f, 500.0f), 3.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"release", 1}, "Release",
        logRange(5.0f, 4000.0f), 200.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"master", 1}, "Master",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    // ---- オシレーター 1-3 ----
    for (int i = 1; i <= RiserEngine::kNumOscs; ++i)
    {
        const juce::String n(i);
        add(std::make_unique<BoolP>(juce::ParameterID{"osc" + n + "On", 1}, "Osc" + n + " On", i == 1));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Wave", 1}, "Osc" + n + " Wave",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Level", 1}, "Osc" + n + " Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "Coarse", 1}, "Osc" + n + " Coarse",
            -24, 24, 0));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "Uni", 1}, "Osc" + n + " Unison",
            1, RiserEngine::kMaxUnison, 1));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Det", 1}, "Osc" + n + " Detune",
            juce::NormalisableRange<float>(0.0f, 100.0f), 12.0f));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Spread", 1}, "Osc" + n + " Spread",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Range", 1}, "Osc" + n + " Pitch Range",
            juce::NormalisableRange<float>(0.0f, 48.0f, 1.0f), 24.0f));
    }

    // ---- ノイズ ----
    add(std::make_unique<ChoiceP>(juce::ParameterID{"noiseType", 1}, "Noise Type",
        juce::StringArray{"White", "Pink", "Brown"}, 1));
    add(std::make_unique<FloatP>(juce::ParameterID{"noiseLevel", 1}, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"noisePitch", 1}, "Noise Pitch",
        logRange(20.0f, 20000.0f), 500.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"noiseRes", 1}, "Noise Res",
        juce::NormalisableRange<float>(0.5f, 12.0f), 2.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"noiseRange", 1}, "Noise Range (Oct)",
        juce::NormalisableRange<float>(0.0f, 10.0f), 5.0f));

    // ---- フィルター 1-4 (ZDF/TPT) ----
    for (int i = 1; i <= RiserEngine::kNumFilters; ++i)
    {
        const juce::String n(i);
        add(std::make_unique<BoolP>(juce::ParameterID{"flt" + n + "On", 1}, "Filter" + n + " On", i == 1));
        add(std::make_unique<ChoiceP>(juce::ParameterID{"flt" + n + "Type", 1}, "Filter" + n + " Type",
            juce::StringArray{"LowPass", "HighPass", "BandPass", "Notch"}, 0));
        add(std::make_unique<FloatP>(juce::ParameterID{"flt" + n + "Cutoff", 1}, "Filter" + n + " Cutoff",
            logRange(20.0f, 20000.0f), 1000.0f));
        add(std::make_unique<FloatP>(juce::ParameterID{"flt" + n + "Res", 1}, "Filter" + n + " Res",
            juce::NormalisableRange<float>(0.5f, 12.0f), 0.9f));
        add(std::make_unique<FloatP>(juce::ParameterID{"flt" + n + "Env", 1}, "Filter" + n + " Env",
            juce::NormalisableRange<float>(-1.0f, 1.0f), i == 1 ? 0.5f : 0.0f));
    }

    // ---- FXスロット 1-6 ----
    for (int i = 1; i <= FxChain::kNumSlots; ++i)
    {
        const juce::String n(i);
        add(std::make_unique<ChoiceP>(juce::ParameterID{"fx" + n + "Type", 1}, "FX" + n + " Type",
            FxChain::getTypeNames(), 0));
        add(std::make_unique<FloatP>(juce::ParameterID{"fx" + n + "Amt", 1}, "FX" + n + " Amount",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        add(std::make_unique<FloatP>(juce::ParameterID{"fx" + n + "Env", 1}, "FX" + n + " EnvDepth",
            juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    }

    // ---- FX詳細 ----
    add(std::make_unique<ChoiceP>(juce::ParameterID{"satAlgo", 1}, "Sat Algo", FxChain::getSatAlgoNames(), 0));
    add(std::make_unique<FloatP>(juce::ParameterID{"satDrive", 1}, "Sat Drive",
        juce::NormalisableRange<float>(1.0f, 12.0f), 2.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"satPre", 1}, "Sat PreHPF",
        logRange(20.0f, 2000.0f), 20.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"satTrim", 1}, "Sat Trim",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    add(std::make_unique<FloatP>(juce::ParameterID{"choRate", 1}, "Chorus Rate",
        logRange(0.05f, 8.0f), 0.8f));
    add(std::make_unique<FloatP>(juce::ParameterID{"choDepth", 1}, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    add(std::make_unique<FloatP>(juce::ParameterID{"choWidth", 1}, "Chorus Width",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    add(std::make_unique<ChoiceP>(juce::ParameterID{"dlyTime", 1}, "Delay Time", FxChain::getDelayTimeNames(), 5));
    add(std::make_unique<FloatP>(juce::ParameterID{"dlyFb", 1}, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f), 0.45f));
    add(std::make_unique<FloatP>(juce::ParameterID{"dlyDuck", 1}, "Delay Duck",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    add(std::make_unique<FloatP>(juce::ParameterID{"dlyDamp", 1}, "Delay Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    add(std::make_unique<FloatP>(juce::ParameterID{"frzSize", 1}, "Freeze Size",
        logRange(20.0f, 1000.0f), 100.0f));
    add(std::make_unique<FloatP>(juce::ParameterID{"frzFb", 1}, "Freeze Feedback",
        juce::NormalisableRange<float>(0.0f, 0.99f), 0.9f));
    add(std::make_unique<FloatP>(juce::ParameterID{"frzDamp", 1}, "Freeze Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));

    add(std::make_unique<FloatP>(juce::ParameterID{"revDecay", 1}, "Reverb Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    add(std::make_unique<FloatP>(juce::ParameterID{"revShimmer", 1}, "Reverb Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));
    add(std::make_unique<FloatP>(juce::ParameterID{"revDamp", 1}, "Reverb Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    add(std::make_unique<FloatP>(juce::ParameterID{"revMod", 1}, "Reverb Mod",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));

    add(std::make_unique<ChoiceP>(juce::ParameterID{"duckRate", 1}, "Duck Rate", FxChain::getDuckRateNames(), 2));
    add(std::make_unique<FloatP>(juce::ParameterID{"duckShape", 1}, "Duck Shape",
        juce::NormalisableRange<float>(0.5f, 8.0f), 2.0f));

    return { params.begin(), params.end() };
}

void LiftXAudioProcessor::cacheParameterPointers()
{
    auto p = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };

    pLift = p("lift");
    pBars = p("bars");
    pAttack = p("attack");
    pRelease = p("release");
    pMaster = p("master");

    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
    {
        const juce::String n(i + 1);
        pOsc[(size_t)i] = { p("osc" + n + "On"), p("osc" + n + "Wave"), p("osc" + n + "Level"),
                            p("osc" + n + "Coarse"), p("osc" + n + "Uni"), p("osc" + n + "Det"),
                            p("osc" + n + "Spread"), p("osc" + n + "Range") };
    }

    pNoiseType = p("noiseType");
    pNoiseLevel = p("noiseLevel");
    pNoisePitch = p("noisePitch");
    pNoiseRes = p("noiseRes");
    pNoiseRange = p("noiseRange");

    for (int i = 0; i < RiserEngine::kNumFilters; ++i)
    {
        const juce::String n(i + 1);
        pFlt[(size_t)i] = { p("flt" + n + "On"), p("flt" + n + "Type"), p("flt" + n + "Cutoff"),
                            p("flt" + n + "Res"), p("flt" + n + "Env") };
    }

    for (int i = 0; i < FxChain::kNumSlots; ++i)
    {
        const juce::String n(i + 1);
        pFxSlot[(size_t)i] = { p("fx" + n + "Type"), p("fx" + n + "Amt"), p("fx" + n + "Env") };
    }

    pSatAlgo = p("satAlgo");   pSatDrive = p("satDrive"); pSatPre = p("satPre");   pSatTrim = p("satTrim");
    pChoRate = p("choRate");   pChoDepth = p("choDepth"); pChoWidth = p("choWidth");
    pDlyTime = p("dlyTime");   pDlyFb = p("dlyFb");       pDlyDuck = p("dlyDuck"); pDlyDamp = p("dlyDamp");
    pFrzSize = p("frzSize");   pFrzFb = p("frzFb");       pFrzDamp = p("frzDamp");
    pRevDecay = p("revDecay"); pRevShimmer = p("revShimmer");
    pRevDamp = p("revDamp");   pRevMod = p("revMod");
    pDuckRate = p("duckRate"); pDuckShape = p("duckShape");
}

// ==========================================================
// prepareToPlay: 全ての事前アロケーションはここで行う
// ==========================================================
void LiftXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mPreparedSampleRate = sampleRate;
    mPreparedBlockSize = juce::jmax(16, samplesPerBlock);

    mEngine.prepare(sampleRate);
    mFx.prepare(sampleRate);

    mScratchR.assign((size_t)mPreparedBlockSize, 0.0f);

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
    //  Ableton Live等はSR変更時に prepareToPlay より先に processBlock を
    //  呼ぶことがある。不一致を検知したら即ゼロクリア+リセットして返す。
    if (!mPrepared
        || std::abs(getSampleRate() - mPreparedSampleRate) > 0.5
        || numSamples > mPreparedBlockSize
        || numSamples <= 0)
    {
        buffer.clear();
        mEngine.hardReset();
        return;
    }

    buffer.clear(); // シンセなので常に無音から開始

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

    // ---- MIDI (ノートオン=ライザートリガー) ----
    const double qnPerSample = (bpm / 60.0) / mPreparedSampleRate;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            const double ppqAtEvent = hasPpq && playing
                ? ppq + (double)meta.samplePosition * qnPerSample : ppq;
            mEngine.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), ppqAtEvent, playing && hasPpq);
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

    // ---- DAW同期 (選択案A: PPQ基準の絶対時間でProgressを進める) ----
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

    // ---- FXチェーン ----
    FxChain::Params fp;
    gatherFxParams(fp, bpm, ppq, playing);
    mFx.process(buffer, fp);

    // ---- マスターゲイン (平滑) + セーフティクリップ ----
    mMasterSm.setTargetValue(juce::Decibels::decibelsToGain(pMaster->load()));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer(ch);
        auto sm = mMasterSm; // チャンネル毎に同じ軌跡を辿るためコピー
        for (int i = 0; i < numSamples; ++i)
        {
            float v = d[i] * sm.getNextValue();
            // 最終安全弁 (±2.0でソフト飽和)
            if (v > 2.0f) v = 2.0f + std::tanh(v - 2.0f) * 0.1f;
            else if (v < -2.0f) v = -2.0f + std::tanh(v + 2.0f) * 0.1f;
            d[i] = v;
        }
    }
    mMasterSm.skip(numSamples); // 本体を前進 (コピーで消費した分)
}

// ==========================================================
// パラメーター収集 (RT安全: atomic load のみ)
// ==========================================================
void LiftXAudioProcessor::gatherEngineParams(RiserEngine::Params& ep) const noexcept
{
    ep.lift = pLift->load();
    ep.attackMs = pAttack->load();
    ep.releaseMs = pRelease->load();

    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
    {
        auto& o = ep.osc[(size_t)i];
        const auto& q = pOsc[(size_t)i];
        o.on = q.on->load() > 0.5f;
        o.wave = q.wave->load();
        o.level = q.level->load();
        o.coarse = q.coarse->load();
        o.unison = (int)q.uni->load();
        o.detune = q.det->load();
        o.spread = q.spread->load();
        o.range = q.range->load();
    }

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
    }
}

void LiftXAudioProcessor::gatherFxParams(FxChain::Params& fp, double bpm, double ppq, bool playing) const noexcept
{
    fp.bpm = bpm;
    fp.ppq = ppq;
    fp.playing = playing;

    // FXカーブ (マルチENV) の値をスロットWet量へ合成
    const float fxEnv = mEngine.getFxEnvValue();
    for (int s = 0; s < FxChain::kNumSlots; ++s)
    {
        const auto& q = pFxSlot[(size_t)s];
        fp.type[(size_t)s] = (int)q.type->load();
        fp.amount[(size_t)s] = juce::jlimit(0.0f, 1.0f, q.amt->load() + q.env->load() * fxEnv);
    }

    fp.satAlgo = (int)pSatAlgo->load();
    fp.satDrive = pSatDrive->load();
    fp.satPreHz = pSatPre->load();
    fp.satTrimDb = pSatTrim->load();

    fp.choRate = pChoRate->load();
    fp.choDepth = pChoDepth->load();
    fp.choWidth = pChoWidth->load();

    fp.dlyTime = (int)pDlyTime->load();
    fp.dlyFeedback = pDlyFb->load();
    fp.dlyDuck = pDlyDuck->load();
    fp.dlyDamp = pDlyDamp->load();

    fp.frzSize = pFrzSize->load();
    fp.frzFeedback = pFrzFb->load();
    fp.frzDamp = pFrzDamp->load();

    fp.revDecay = pRevDecay->load();
    fp.revShimmer = pRevShimmer->load();
    fp.revDamp = pRevDamp->load();
    fp.revMod = pRevMod->load();

    fp.duckRate = (int)pDuckRate->load();
    fp.duckShape = pDuckShape->load();
}

// ==========================================================
// カスタムWavetable (メッセージスレッド専用 / SPECTRA8方式)
// ==========================================================
bool LiftXAudioProcessor::loadCustomWavetable(const juce::File& file)
{
    if (!file.existsAsFile()) return false;

    std::unique_ptr<juce::AudioFormatReader> reader(mFormatManager.createReaderFor(file));
    if (reader == nullptr) return false;

    const int numSamples = (int)juce::jmin<juce::int64>(reader->lengthInSamples,
        (juce::int64)(MorphWavetable::kMaxCustomFrames * MorphWavetable::kTableSize));
    if (numSamples < 16) return false;

    juce::AudioBuffer<float> tmp((int)reader->numChannels, numSamples);
    reader->read(&tmp, 0, numSamples, 0, true, true);

    // モノラル化
    std::vector<float> mono((size_t)numSamples, 0.0f);
    const float chNorm = 1.0f / (float)juce::jmax(1u, (juce::uint32)reader->numChannels);
    for (int ch = 0; ch < (int)reader->numChannels; ++ch)
    {
        const float* src = tmp.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            mono[(size_t)i] += src[i] * chNorm;
    }

    if (!mWavetable.loadCustomFromBuffer(mono.data(), numSamples))
        return false;

    apvts.state.setProperty("customWavetablePath", file.getFullPathName(), nullptr);
    return true;
}

void LiftXAudioProcessor::clearCustomWavetable()
{
    mWavetable.clearCustom();
    apvts.state.removeProperty("customWavetablePath", nullptr);
}

// ==========================================================
// ステート保存/復元 (APVTS + カーブ)
// ==========================================================
void LiftXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // 既存のCURVESノードを除去してから最新を追加
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

    // カーブ復元
    mCurves.fromValueTree(state.getChildWithName("CURVES"));

    apvts.replaceState(state);

    // カスタムWavetable復元
    const auto wtPath = getCustomWavetablePath();
    if (wtPath.isNotEmpty())
    {
        const juce::File f(wtPath);
        if (f.existsAsFile())
            loadCustomWavetable(f);
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
