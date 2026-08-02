// ==========================================
// File: PluginProcessor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

#include <cmath>
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
    // PAN: -1=左端 / 0=中央 / +1=右端 を "L50 / C / R50" 表記にする
    juce::String panStr(float v, int)
    {
        const int p = (int)std::round(std::abs(v) * 100.0f);
        if (p < 1) return "C";
        return (v < 0.0f ? "L" : "R") + juce::String(p);
    }

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

    // Key/Scale変更を監視して Start/End キーを自動スナップする
    apvts.addParameterListener("scaleOn", this);
    apvts.addParameterListener("scaleKey", this);
    apvts.addParameterListener("scaleType", this);
    for (int i = 1; i <= RiserEngine::kNumOscs; ++i)
        apvts.addParameterListener("osc" + juce::String(i) + "Scale", this);

    rememberSnapState();   // 起動直後は現在値をそのまま採用 (勝手に動かさない)

    // Key/Scale変更の取り込みはメッセージスレッドのタイマーで行う。
    //  ホストによってはプロセッサーの構築がメッセージスレッド以外で走るため、
    //  startTimer は callAsync 経由で確実にメッセージスレッドから呼ぶ。
    juce::MessageManager::callAsync(
        [ref = juce::WeakReference<LiftXAudioProcessor>(this)]
        {
            if (ref != nullptr)
                ref->startTimer(40);   // 25Hz: 操作の追従には十分で負荷も無視できる
        });
}

LiftXAudioProcessor::~LiftXAudioProcessor()
{
    stopTimer();
    apvts.removeParameterListener("scaleOn", this);
    apvts.removeParameterListener("scaleKey", this);
    apvts.removeParameterListener("scaleType", this);
    for (int i = 1; i <= RiserEngine::kNumOscs; ++i)
        apvts.removeParameterListener("osc" + juce::String(i) + "Scale", this);
}

// ==========================================================
// Key/Scale変更時の Start/End キー自動スナップ
// ==========================================================
LiftXAudioProcessor::SnapState LiftXAudioProcessor::readSnapState() const
{
    SnapState s;
    s.on   = pScaleOn   != nullptr && pScaleOn->load() > 0.5f;
    s.key  = pScaleKey  != nullptr ? (int)pScaleKey->load() : 0;
    s.type = pScaleType != nullptr ? (int)pScaleType->load() : 0;
    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
        s.oscApply[(size_t)i] = pOsc[(size_t)i].scaleQ != nullptr
                             && pOsc[(size_t)i].scaleQ->load() > 0.5f;
    return s;
}

void LiftXAudioProcessor::rememberSnapState()
{
    mLastSnapState = readSnapState();
}

void LiftXAudioProcessor::parameterChanged(const juce::String&, float)
{
    // ホストオートメーション経由だとオーディオスレッドから呼ばれるため、
    // ここでは atomic フラグを立てるだけに留める (ロックもアロケーションも無し)。
    // 実際のスナップ処理は timerCallback() がメッセージスレッドで行う。
    mSnapDirty.store(true, std::memory_order_release);
}

void LiftXAudioProcessor::timerCallback()
{
    // メッセージスレッド。パラメーターの書き換えはここでのみ行う。
    if (!mSnapDirty.exchange(false, std::memory_order_acquire))
        return;

    const auto now = readSnapState();
    if (!(now != mLastSnapState))
        return;                    // 実質変化なし (ステート復元直後など)

    mLastSnapState = now;
    if (now.on)
        snapKeysToScale();
}

void LiftXAudioProcessor::snapKeysToScale()
{
    const auto st = readSnapState();
    if (!st.on) return;

    const int key = juce::jlimit(0, 11, st.key);
    const int type = juce::jlimit(0, ScaleQuantizer::numScales() - 1, st.type);

    auto snapOne = [this, key, type](const juce::String& id)
    {
        auto* prm = apvts.getParameter(id);
        if (prm == nullptr) return;

        const auto& range = prm->getNormalisableRange();
        const float cur = range.convertFrom0to1(prm->getValue());
        float snapped = ScaleQuantizer::quantize(cur, key, type);

        // 0..127 の範囲外へ出た場合はオクターブ単位で内側へ戻す
        while (snapped < range.start) snapped += 12.0f;
        while (snapped > range.end)   snapped -= 12.0f;
        snapped = juce::jlimit(range.start, range.end, std::round(snapped));

        if (std::abs(snapped - cur) < 0.5f) return;   // 既に構成音
        prm->setValueNotifyingHost(range.convertTo0to1(snapped));
    };

    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
    {
        if (!st.oscApply[(size_t)i]) continue;   // 適用外のOSCは触らない
        const juce::String n(i + 1);
        snapOne("osc" + n + "KeyStart");
        snapOne("osc" + n + "KeyEnd");
    }
}

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
    // LIFT = ENV評価位置。デフォルトは AUTO + 0% (Manual時にEndKey側で鳴る誤解を防ぐ)
    add(std::make_unique<FloatP>(juce::ParameterID{"lift", 1}, "LIFT",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<BoolP>(juce::ParameterID{"liftMode", 1}, "LIFT Auto", true));
    // REVERSE: ENV評価位置を反転 (ライザー↔ダウナー)
    add(std::make_unique<BoolP>(juce::ParameterID{"reverse", 1}, "Reverse", false));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"bars", 1}, "Bars",
        getBarsNames(), 7)); // デフォルト "4"
    add(std::make_unique<FloatP>(juce::ParameterID{"attack", 1}, "Attack",
        logRange(0.1f, 500.0f), 3.0f, attr(msStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"release", 1}, "Release",
        logRange(5.0f, 4000.0f), 200.0f, attr(msStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"master", 1}, "Master",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, attr(dbStr)));

    // ---- Pitch ENV スケール量子化 (CONFIGタブ) ----
    //  Off : カーブ通りの滑らかなピッチ変化 (従来動作)
    //  On  : Key + Scale の構成音のみを通る階段状のピッチ変化
    add(std::make_unique<BoolP>(juce::ParameterID{"scaleOn", 1}, "Scale Quantize", false));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"scaleKey", 1}, "Scale Key",
        juce::StringArray{ "C", "C#", "D", "D#", "E", "F",
                           "F#", "G", "G#", "A", "A#", "B" }, 0));
    {
        juce::StringArray scaleNames;
        for (const auto& s : ScaleQuantizer::getScales())
            scaleNames.add(s.name);
        add(std::make_unique<ChoiceP>(juce::ParameterID{"scaleType", 1}, "Scale Type",
            scaleNames, 2)); // デフォルト = Natural Minor
    }

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
        // FINE: ±100セント。レイヤー間のわずかなズレで厚みを出す
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Fine", 1}, "Osc" + n + " Fine",
            juce::NormalisableRange<float>(-100.0f, 100.0f), 0.0f, attr(ctStr)));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "Uni", 1}, "Osc" + n + " Unison",
            1, RiserEngine::kMaxUnison, 1));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Det", 1}, "Osc" + n + " Detune",
            juce::NormalisableRange<float>(0.0f, 100.0f), 12.0f, attr(ctStr)));
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Spread", 1}, "Osc" + n + " Spread",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f, attr(pctStr)));
        // PAN: 基準定位。PAN ENVカーブはここを中心に±で振れる
        add(std::make_unique<FloatP>(juce::ParameterID{"osc" + n + "Pan", 1}, "Osc" + n + " Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f, attr(panStr)));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "KeyStart", 1}, "Osc" + n + " Start Key",
            0, 127, 36, noteAttr));
        add(std::make_unique<IntP>(juce::ParameterID{"osc" + n + "KeyEnd", 1}, "Osc" + n + " End Key",
            0, 127, 84, noteAttr));
        // このOSCにスケール量子化を適用するか (マスターscaleOnとのAND)
        add(std::make_unique<BoolP>(juce::ParameterID{"osc" + n + "Scale", 1},
            "Osc" + n + " Scale Quantize", true));
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
    add(std::make_unique<FloatP>(juce::ParameterID{"noisePan", 1}, "Noise Pan",
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f, attr(panStr)));

    // ---- フィルター 1-4 (ZDF/TPT + ソース別ルーティング) ----
    static const char* srcNames[4] = { "Osc1", "Osc2", "Osc3", "Noise" };
    for (int i = 1; i <= RiserEngine::kNumFilters; ++i)
    {
        const juce::String n(i);
        add(std::make_unique<BoolP>(juce::ParameterID{"flt" + n + "On", 1}, "Filter" + n + " On", i == 1));
        add(std::make_unique<ChoiceP>(juce::ParameterID{"flt" + n + "Type", 1}, "Filter" + n + " Type",
            juce::StringArray{"LowPass", "HighPass", "BandPass", "Notch",
                              "Vowel", "Comb"}, 0));
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

    // ---- マスターリミッター ----
    add(std::make_unique<BoolP>(juce::ParameterID{"limOn", 1}, "Limiter On", true));
    add(std::make_unique<FloatP>(juce::ParameterID{"limCeiling", 1}, "Limiter Ceiling",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f), -0.3f, attr(dbStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"limRelease", 1}, "Limiter Release",
        logRange(20.0f, 1000.0f), 120.0f, attr(msStr)));

    // ---- FXのソース別ルーティング (エフェクト種別 × OSC1-3/Noise) ----
    //  OFFにしたソースは、そのエフェクトを完全にバイパスして素通しする。
    //  デフォルトは全ON = 従来と完全に同じ挙動。
    //  ※ 既存セッションのオートメーション割り当てに影響しないよう、
    //     必ずパラメーターリストの末尾に追加すること。
    {
        static const char* fxPrefix[5] = { "sat", "cho", "dly", "rev", "duck" };
        static const char* fxLabel[5]  = { "Sat", "Chorus", "Delay", "Reverb", "Duck" };
        for (int f = 0; f < 5; ++f)
            for (int s = 0; s < RiserEngine::kNumSources; ++s)
                add(std::make_unique<BoolP>(
                    juce::ParameterID{ juce::String(fxPrefix[f]) + "Route" + srcNames[s], 1 },
                    juce::String(fxLabel[f]) + " " + srcNames[s], true));
    }

    // ---- KEY FOLLOW (v0.5) ----
    //  MIDIノートは本来トリガー専用で、ピッチは Start/End キーだけで決まる。
    //  Follow を有効にすると、弾いたノートに合わせて音域ごと平行移動する。
    //   Fixed       : 従来どおりノートを無視
    //   Follow Start: 弾いた音が開始音になる (End も同じ量だけ動く)
    //   Follow End  : 弾いた音が着地音になる (ライザーが着地する音を鍵盤で指定)
    //  ※ Start/End パラメーター自体は書き換えない (オートメーションと衝突させない)
    //  基準は常に OSC1 の Start/End キー。つまり「OSC1のSTARTと同じ音を弾けば
    //  移調ゼロ」になるため、別途の基準ノート設定は不要。
    add(std::make_unique<ChoiceP>(juce::ParameterID{"keyFollow", 1}, "Key Follow",
        juce::StringArray{ "Fixed", "Follow Start", "Follow End" }, 0));

    // ---- VELOCITY モジュレーション (v0.5) ----
    //  ベロシティは従来レベルにしか効いていなかった。弾き方で表情を変えられるよう
    //  代表的な3先へのルーティング量を用意する (0 = 従来と完全に同じ)。
    add(std::make_unique<FloatP>(juce::ParameterID{"velToCutoff", 1}, "Vel > Cutoff",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"velToNoise", 1}, "Vel > Noise Lv",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<FloatP>(juce::ParameterID{"velToDrive", 1}, "Vel > Sat Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));

    // ---- Stutter (FXスロット6番目) ----
    add(std::make_unique<FloatP>(juce::ParameterID{"stutAmt", 1}, "Stutter Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));
    add(std::make_unique<ChoiceP>(juce::ParameterID{"stutRate", 1}, "Stutter Rate",
        FxChain::getStutterRateNames(), 11));   // 既定 1/16
    for (int s2 = 0; s2 < RiserEngine::kNumSources; ++s2)
        add(std::make_unique<BoolP>(juce::ParameterID{ juce::String("stutRoute") + srcNames[s2], 1 },
                                    juce::String("Stutter ") + srcNames[s2], true));

    // ---- HUMANIZE: ENV評価位置へゆっくりしたランダムな揺れを加える ----
    //  完全に機械的なライザーに有機的な「息づかい」を足す。0% で従来と同一。
    add(std::make_unique<FloatP>(juce::ParameterID{"humanize", 1}, "Humanize",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, attr(pctStr)));

    return { params.begin(), params.end() };
}

void LiftXAudioProcessor::cacheParameterPointers()
{
    auto p = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };

    pLift = p("lift");
    pLiftMode = p("liftMode");
    pReverse = p("reverse");
    pBars = p("bars");
    pAttack = p("attack");
    pRelease = p("release");
    pMaster = p("master");
    pScaleOn = p("scaleOn");
    pScaleKey = p("scaleKey");
    pScaleType = p("scaleType");

    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
    {
        const juce::String n(i + 1);
        pOsc[(size_t)i] = { p("osc" + n + "On"), p("osc" + n + "Solo"), p("osc" + n + "Mute"),
                            p("osc" + n + "Wave"), p("osc" + n + "Pos"), p("osc" + n + "Level"),
                            p("osc" + n + "Coarse"), p("osc" + n + "Fine"),
                            p("osc" + n + "Uni"), p("osc" + n + "Det"),
                            p("osc" + n + "Spread"), p("osc" + n + "Pan"),
                            p("osc" + n + "KeyStart"), p("osc" + n + "KeyEnd"),
                            p("osc" + n + "Scale") };
    }

    pNoiseSolo = p("noiseSolo");
    pNoiseMute = p("noiseMute");
    pNoiseType = p("noiseType");
    pNoiseLevel = p("noiseLevel");
    pNoisePitch = p("noisePitch");
    pNoiseRes = p("noiseRes");
    pNoiseRange = p("noiseRange");
    pNoisePan   = p("noisePan");

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

    {
        static const char* fxPrefix[5] = { "sat", "cho", "dly", "rev", "duck" };
        for (int f = 0; f < 5; ++f)
            for (int s = 0; s < RiserEngine::kNumSources; ++s)
                pFxRoute[(size_t)f][(size_t)s] = p(juce::String(fxPrefix[f]) + "Route" + srcNames[s]);
    }

    pSatAmt = p("satAmt");   pSatAlgo = p("satAlgo"); pSatDrive = p("satDrive");
    pSatPre = p("satPre");   pSatTrim = p("satTrim");
    pChoAmt = p("choAmt");   pChoRate = p("choRate"); pChoDepth = p("choDepth"); pChoWidth = p("choWidth");
    pDlyAmt = p("dlyAmt");   pDlyTime = p("dlyTime"); pDlyFb = p("dlyFb");
    pDlyDuck = p("dlyDuck"); pDlyDamp = p("dlyDamp");
    pRevAmt = p("revAmt");   pRevDecay = p("revDecay"); pRevShimmer = p("revShimmer");
    pRevDamp = p("revDamp"); pRevMod = p("revMod");
    pDuckAmt = p("duckAmt"); pDuckRate = p("duckRate"); pDuckShape = p("duckShape");
    pLimOn = p("limOn"); pLimCeiling = p("limCeiling"); pLimRelease = p("limRelease");

    pStutAmt  = p("stutAmt");
    pStutRate = p("stutRate");
    for (int s2 = 0; s2 < RiserEngine::kNumSources; ++s2)
        pFxRoute[5][(size_t)s2] = p(juce::String("stutRoute") + srcNames[s2]);
    pHumanize = p("humanize");

    pKeyFollow   = p("keyFollow");
    pVelToCutoff = p("velToCutoff");
    pVelToNoise  = p("velToNoise");
    pVelToDrive  = p("velToDrive");
}

float LiftXAudioProcessor::getEnvPosition() const noexcept
{
    const bool autoMode = pLiftMode->load() > 0.5f;
    const float pos = juce::jlimit(0.0f, 1.0f,
        autoMode ? mEngine.uiProgress.load(std::memory_order_relaxed) : pLift->load());
    // REVERSE時はカーブを逆から読むため、プレイヘッド/ノブ帯の表示位置も反転させる
    return (pReverse != nullptr && pReverse->load() > 0.5f) ? (1.0f - pos) : pos;
}

// ==========================================================
// prepareToPlay
// ==========================================================
void LiftXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mPreparedSampleRate = sampleRate;
    // 一部のホスト (オフラインバウンス/フリーズ/一部ライブ環境) は prepareToPlay で
    // 通知したサイズより大きなブロックを渡してくることがある。
    // 余裕を持って確保し、ブロックサイズ超過で無音になる事故を防ぐ。
    mPreparedBlockSize = juce::jmax(16, samplesPerBlock);
    mMaxBlockSize = juce::jmax(mPreparedBlockSize * 2, 8192);

    // LIFT(MANUAL)の平滑時定数をブロック長へ追従させるため、ブロック長も渡す
    mEngine.prepare(sampleRate, mPreparedBlockSize);
    mFx.prepare(sampleRate);
    mLimiter.prepare(sampleRate);

    for (int s = 0; s < RiserEngine::kNumSources; ++s)
    {
        mBusL[(size_t)s].assign((size_t)mMaxBlockSize, 0.0f);
        mBusR[(size_t)s].assign((size_t)mMaxBlockSize, 0.0f);
    }

    // キャプチャバッファ (最大60秒・ステレオ / サンプル数上限でメモリを抑制)
    const size_t capSize = (size_t)juce::jlimit<juce::int64>(
        16384, (juce::int64)kMaxCaptureSamples,
        (juce::int64)(sampleRate * kMaxCaptureSeconds));
    mCapL.assign(capSize, 0.0f);
    mCapR.assign(capSize, 0.0f);
    mCapWrite = 0;
    mCapBodyEnd = 0;
    mCapturing = false;
    mCapInTail = false;
    mCapSilentRun = 0;
    mCapTailWritten = 0;
    mCapSilenceHold = juce::jmax(64, (int)(sampleRate * kSilenceHoldSec));
    mCapTailCap     = juce::jmax(1024, (int)(sampleRate * kMaxTailSeconds));
    mCapLenPub.store(0);
    mCapBodyEndPub.store(0);
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
    //  SR不一致 / 未prepare / 事前確保を超える異常ブロックのみ停止する。
    //  (通常のブロックサイズ変動は mMaxBlockSize の余裕で吸収する)
    if (!mPrepared
        || std::abs(getSampleRate() - mPreparedSampleRate) > 0.5
        || numSamples > mMaxBlockSize
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
    // getTailLengthSeconds() はプレイヘッドを参照できないため、ここで保持しておく
    mLastBpm.store(bpm, std::memory_order_relaxed);

    // ---- DAW同期 (レンダリング前に progInc / totalQn を確定させる) ----
    const double bars = barsFromChoice((int)pBars->load());
    mEngine.syncTransport(playing, hasPpq, ppq, bpm, qnPerBar, bars);

    // ---- エンジンパラメーター収集 ----
    RiserEngine::Params ep;
    gatherEngineParams(ep);

    // ---- ソース別バス ----
    //  FXのソース別ルーティングのため、OSC1/2/3/Noise を分離したまま
    //  FXチェーンへ渡し、最後にまとめて出力バッファへ合算する。
    float* busL[RiserEngine::kNumSources];
    float* busR[RiserEngine::kNumSources];
    for (int s = 0; s < RiserEngine::kNumSources; ++s)
    {
        busL[s] = mBusL[(size_t)s].data();
        busR[s] = mBusR[(size_t)s].data();
        juce::FloatVectorOperations::clear(busL[s], numSamples);
        juce::FloatVectorOperations::clear(busR[s], numSamples);
    }

    // ---- MIDI + レンダリング (サンプルアキュレート) ----
    //  MIDIイベントの位置でブロックを分割してレンダリングする。
    //  従来はブロック先頭でまとめてノートオンを適用してからブロック全体を
    //  描画していたため、ライザーの立ち上がりが最大1ブロック
    //  (512サンプル @48kHz = 10.7ms) 早くなることがあった。
    //  Ducking は PPQ 同期でグリッドに正確に張り付くので、この差は
    //  「ライザーだけが前へずれる」形で効いていた。
    bool noteOnThisBlock = false;
    int  noteOnSample = 0;              // ブロック内でのノートオン位置 (キャプチャ用)
    const double qnPerSample = (bpm / 60.0) / mPreparedSampleRate;
    int rendered = 0;

    auto renderUpTo = [&](int endSample)
    {
        const int n = endSample - rendered;
        if (n <= 0) return;

        float* segL[RiserEngine::kNumSources];
        float* segR[RiserEngine::kNumSources];
        for (int s = 0; s < RiserEngine::kNumSources; ++s)
        {
            segL[s] = busL[s] + rendered;
            segR[s] = busR[s] + rendered;
        }
        mEngine.render(segL, segR, n, ep, mCurves);
        rendered = endSample;
    };

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const bool isNoteOn  = msg.isNoteOn();
        const bool isNoteOff = msg.isNoteOff();
        const bool isAllOff  = msg.isAllNotesOff() || msg.isAllSoundOff();
        if (!isNoteOn && !isNoteOff && !isAllOff)
            continue;

        // イベント位置まで先に描いてから、その瞬間にイベントを適用する
        const int evPos = juce::jlimit(0, numSamples, meta.samplePosition);
        renderUpTo(evPos);

        if (isNoteOn)
        {
            noteOnThisBlock = true;
            noteOnSample = evPos;
            const double ppqAtEvent = hasPpq && playing
                ? ppq + (double)evPos * qnPerSample : ppq;
            mEngine.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), ppqAtEvent, playing && hasPpq);

            // MIDI Learn 用 (GUIがStartKey/EndKey設定に使用)
            mLastNote.store(msg.getNoteNumber(), std::memory_order_relaxed);
            mNoteEvents.fetch_add(1, std::memory_order_relaxed);
        }
        else if (isNoteOff)
        {
            mEngine.noteOff(msg.getNoteNumber());
        }
        else
        {
            mEngine.allNotesOff();
        }
    }
    renderUpTo(numSamples);

    // ---- FXチェーン (カーブ変調をブロックレートで合成) ----
    FxChain::Params fp;
    gatherFxParams(fp, bpm, ppq, playing);
    mFx.process(busL, busR, numSamples, fp);

    // ---- バス合算 → 出力バッファ ----
    //  モノラル出力時は (L+R)/2 で正しくダウンミックスする。
    //  (FX自体はステレオのまま処理してから畳むため、コーラス/リバーブの
    //   ステレオ感がモノ和として正しく残る)
    {
        float* outL = buffer.getWritePointer(0);
        const bool stereo = buffer.getNumChannels() > 1;
        float* outR = stereo ? buffer.getWritePointer(1) : nullptr;

        if (stereo)
        {
            for (int s = 0; s < RiserEngine::kNumSources; ++s)
            {
                juce::FloatVectorOperations::add(outL, busL[s], numSamples);
                juce::FloatVectorOperations::add(outR, busR[s], numSamples);
            }
        }
        else
        {
            for (int s = 0; s < RiserEngine::kNumSources; ++s)
            {
                juce::FloatVectorOperations::addWithMultiply(outL, busL[s], 0.5f, numSamples);
                juce::FloatVectorOperations::addWithMultiply(outL, busR[s], 0.5f, numSamples);
            }
        }
    }

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

    // ---- マスターリミッター (最終段) ----
    if (pLimOn->load() > 0.5f)
    {
        mLimiter.setRelease(pLimRelease->load());
        float* limL = buffer.getWritePointer(0);
        float* limR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : limL;
        mLimiter.process(limL, limR, numSamples,
                         juce::Decibels::decibelsToGain(pLimCeiling->load()));
    }

    // ---- アウトプットメーター用ピーク (GUIヘッダー表示) ----
    for (int ch = 0; ch < 2; ++ch)
    {
        const int src = juce::jmin(ch, buffer.getNumChannels() - 1);
        mOutPeak[(size_t)ch].store(buffer.getMagnitude(src, 0, numSamples),
                                   std::memory_order_relaxed);
    }

    // ---- ライザー出力キャプチャ (プラグイン最終出力 / RT安全: memcpy と算術のみ) ----
    //
    //  録音は2段階:
    //   [本編] ノートオン → Progress が 1.0 に到達 (=指定小節分) するか、
    //          リリースが完了するまで。Progress は PPQ 同期なのでテンポ変化に追従する。
    //   [テール] 本編終了後、出力が実際に鳴り止むまで録り続ける。
    //          -90dBFS を 100ms 連続で下回ったら確定し、末尾の無音を切り詰める。
    //          → Shimmer Reverb の長い減衰や FB 0.95 の Delay でも切れ際まで入る。
    {
        const bool act = mEngine.isNoteActive();

        if (noteOnThisBlock)
        {
            // 新しいライザー開始 → 録音をやり直す
            mCapWrite = 0;
            mCapBodyEnd = 0;
            mCapturing = true;
            mCapInTail = false;
            mCapSilentRun = 0;
            mCapTailWritten = 0;
            mCapBodyEndPub.store(0, std::memory_order_relaxed);
            mCapActive.store(true, std::memory_order_relaxed);
        }

        // 本編の想定長は毎ブロック再計算する (ノートオン時のBPMで固定すると
        // テンポオートメーションでズレるため)。Progress 到達判定の保険として使う。
        {
            const double riserSec = (bars * qnPerBar) * 60.0 / juce::jmax(20.0, bpm);
            mCapRiserLen = juce::jlimit(256, (int)mCapL.size(),
                                        (int)(riserSec * mPreparedSampleRate));
        }

        if (mCapturing)
        {
            // ノートオンがブロック途中だった場合、その手前(=前のライザーのテール)は
            // 録らずに切り落とす。こうしないと録音の頭に無関係な残響が混ざる。
            const int srcStart = noteOnThisBlock ? noteOnSample : 0;
            const int nAvail   = numSamples - srcStart;

            const int cap = (int)mCapL.size();
            const int nWrite = juce::jmin(nAvail, cap - mCapWrite);

            const float* sl  = buffer.getReadPointer(0) + srcStart;
            const float* sr2 = (buffer.getNumChannels() > 1
                                ? buffer.getReadPointer(1) : buffer.getReadPointer(0)) + srcStart;

            if (nWrite > 0)
            {
                std::memcpy(mCapL.data() + mCapWrite, sl,  (size_t)nWrite * sizeof(float));
                std::memcpy(mCapR.data() + mCapWrite, sr2, (size_t)nWrite * sizeof(float));
                mCapWrite += nWrite;
            }
            mCapLenPub.store(mCapWrite, std::memory_order_relaxed);

            // ---- 本編 → テールへの遷移 ----
            //  Progress 到達 (指定小節分を録り切った) か、リリース完了で本編終了。
            if (!mCapInTail)
            {
                const bool bodyDone = !act
                                   || mEngine.getProgressF() >= 0.99999f
                                   || mCapWrite >= mCapRiserLen;
                if (bodyDone)
                {
                    mCapInTail = true;
                    mCapBodyEnd = mCapWrite;
                    mCapSilentRun = 0;
                    mCapTailWritten = 0;
                    mCapBodyEndPub.store(mCapBodyEnd, std::memory_order_relaxed);
                }
            }

            // ---- テール: 実際に鳴り止むまで録る ----
            if (mCapInTail)
            {
                // このブロックのピークを見て無音判定 (書けなかった分は無音扱い)
                float pk = 0.0f;
                for (int i = 0; i < nWrite; ++i)
                    pk = juce::jmax(pk, std::abs(sl[i]), std::abs(sr2[i]));

                if (pk < kSilenceThresh) mCapSilentRun += juce::jmax(nWrite, nAvail);
                else                     mCapSilentRun = 0;

                mCapTailWritten += nAvail;

                const bool silent    = mCapSilentRun >= mCapSilenceHold;
                const bool tailMaxed = mCapTailWritten >= mCapTailCap;
                const bool bufFull   = mCapWrite >= cap;

                if (silent || tailMaxed || bufFull)
                {
                    // 末尾の無音を切り詰める (WAVを無駄に長くしない)。
                    // 10ms だけ余韻を残してから確定する。
                    if (silent)
                    {
                        const int keep = (int)(mPreparedSampleRate * 0.01);
                        mCapWrite = juce::jlimit(juce::jmax(256, mCapBodyEnd), mCapWrite,
                                                 mCapWrite - mCapSilentRun + keep);
                    }
                    mCapLenPub.store(mCapWrite, std::memory_order_relaxed);
                    mCapturing = false;
                    mCapInTail = false;
                    mCapActive.store(false, std::memory_order_relaxed);
                    mCapVersion.fetch_add(1, std::memory_order_release);
                }
            }
        }
    }
}

// ==========================================================
// テール長の申告 (ホストのフリーズ/バウンスで残響が切られないように)
//  Reverb の DECAY と Delay の FB / TIME から RT60 を見積もる。
//  スロットに入っていないFXは無視する。
// ==========================================================
double LiftXAudioProcessor::getTailLengthSeconds() const
{
    double tail = 0.5;   // リリース最大4秒 + 余裕は下で加算する

    bool hasReverb = false, hasDelay = false;
    for (int s = 0; s < FxChain::kNumSlots; ++s)
    {
        if (pFxType[(size_t)s] == nullptr) continue;
        const int t = (int)pFxType[(size_t)s]->load();
        if (t == FxChain::Reverb) hasReverb = true;
        if (t == FxChain::Delay)  hasDelay = true;
    }

    // ln(0.001) = -60dB
    constexpr double kLn60 = -6.907755;

    if (hasReverb && pRevDecay != nullptr)
    {
        // ShimmerReverb: feedback = min(0.98, 0.5 + decay*0.48)
        // 遅延長の平均は約62ms (31〜101ms のプライム分布)
        const double fb = juce::jmin(0.98, 0.5 + (double)pRevDecay->load() * 0.48);
        const double rt = 0.062 * (kLn60 / std::log(juce::jmax(1.0e-4, fb)));
        tail = juce::jmax(tail, rt * 1.2);   // Shimmer のループ分の余裕
    }

    if (hasDelay && pDlyFb != nullptr && pDlyTime != nullptr)
    {
        const double bpm  = juce::jlimit(20.0, 999.0, mLastBpm.load(std::memory_order_relaxed));
        const double beat = (double)FxChain::delayTimeToBeats((int)pDlyTime->load());
        const double dSec = juce::jmax(0.01, beat * 60.0 / bpm);
        const double fb   = juce::jlimit(0.0f, 0.95f, pDlyFb->load());
        if (fb > 0.01)
            tail = juce::jmax(tail, dSec * (kLn60 / std::log(juce::jmax(1.0e-4, (double)fb))));
        else
            tail = juce::jmax(tail, dSec * 2.0);
    }

    // アンプのリリース分を加算
    if (pRelease != nullptr)
        tail += (double)pRelease->load() * 0.001 * 3.0;

    return juce::jlimit(1.0, 30.0, tail);
}

// ==========================================================
// パラメーター収集 (RT安全: atomic load のみ)
// ==========================================================
void LiftXAudioProcessor::gatherEngineParams(RiserEngine::Params& ep) const noexcept
{
    ep.lift = pLift->load();
    ep.liftAuto = pLiftMode->load() > 0.5f;
    ep.reverse = pReverse->load() > 0.5f;
    ep.attackMs = pAttack->load();
    ep.releaseMs = pRelease->load();
    ep.scaleOn = pScaleOn->load() > 0.5f;
    ep.scaleKey = (int)pScaleKey->load();
    ep.scaleType = (int)pScaleType->load();
    ep.keyFollowMode = pKeyFollow != nullptr ? (int)pKeyFollow->load() : 0;
    ep.humanize = pHumanize != nullptr ? pHumanize->load() : 0.0f;
    ep.velToCutoff = pVelToCutoff != nullptr ? pVelToCutoff->load() : 0.0f;
    ep.velToNoise  = pVelToNoise  != nullptr ? pVelToNoise->load()  : 0.0f;

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
        o.fine = q.fine != nullptr ? q.fine->load() : 0.0f;
        o.unison = (int)q.uni->load();
        o.detune = q.det->load();
        o.spread = q.spread->load();
        o.pan = q.pan != nullptr ? q.pan->load() : 0.0f;
        o.keyStart = (int)q.keyStart->load();
        o.keyEnd = (int)q.keyEnd->load();
        o.scaleQ = q.scaleQ->load() > 0.5f;
    }

    ep.noiseSolo = pNoiseSolo->load() > 0.5f;
    ep.noiseMute = pNoiseMute->load() > 0.5f;
    ep.noiseType = (int)pNoiseType->load();
    ep.noiseLevel = pNoiseLevel->load();
    ep.noisePitch = pNoisePitch->load();
    ep.noiseRes = pNoiseRes->load();
    ep.noiseRangeOct = pNoiseRange->load();
    ep.noisePan = pNoisePan != nullptr ? pNoisePan->load() : 0.0f;

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

    for (int f = 0; f < FxChain::kNumFxKinds; ++f)
        for (int s = 0; s < RiserEngine::kNumSources; ++s)
            if (pFxRoute[(size_t)f][(size_t)s] != nullptr)
                fp.route[(size_t)f][(size_t)s] = pFxRoute[(size_t)f][(size_t)s]->load() > 0.5f;

    // マルチENVカーブによるバイポーラ加算変調 (中央=ノブ値, ±レンジ半分)
    //  評価位置はエンジンが実際に使った値をそのまま貰う。
    //  (Auto/Manual の切り替え・REVERSE・平滑はすべてエンジン側で適用済み)
    //  こうすることでオシレーターとFXが必ず同じカーブ位置を読む。
    const float evalPos = juce::jlimit(0.0f, 1.0f, mEngine.getEvalPos());

    // FX側もセグメント探索ヒントを共有する (エンジンとは別配列)
    auto bip = [this, evalPos](int idx) noexcept
    {
        const float y = mCurves.read(idx).evaluate(evalPos, &mFxCurveHint[(size_t)idx]);
        return (y - 0.5f) * 2.0f;
    };
    auto c01 = [](float v) noexcept { return juce::jlimit(0.0f, 1.0f, v); };

    // フルレンジ加算: 中央=ノブ値 / 上端=MAX方向 / 下端=MIN方向 (クランプ付き)
    fp.satAmt = c01(pSatAmt->load() + bip(CurveStore::SatAmt) * 1.0f);
    fp.satAlgo = (int)pSatAlgo->load();
    {
        // VELOCITY -> SAT DRIVE: 弱く弾くほど歪みが浅くなる
        const float vd = pVelToDrive != nullptr ? pVelToDrive->load() : 0.0f;
        const float velMul = (vd > 0.0f)
            ? 1.0f - vd * (1.0f - mEngine.getVelocityNorm()) : 1.0f;
        fp.satDrive = juce::jlimit(1.0f, 12.0f,
            1.0f + (pSatDrive->load() + bip(CurveStore::SatDrive) * 11.0f - 1.0f) * velMul);
    }
    fp.satPreHz = pSatPre->load();
    fp.satTrimDb = pSatTrim->load();

    fp.choAmt = c01(pChoAmt->load() + bip(CurveStore::ChoAmt) * 1.0f);
    fp.choRate = pChoRate->load();
    fp.choDepth = c01(pChoDepth->load() + bip(CurveStore::ChoDepth) * 1.0f);
    fp.choWidth = pChoWidth->load();

    fp.dlyAmt = c01(pDlyAmt->load() + bip(CurveStore::DlyAmt) * 1.0f);
    fp.dlyFeedback = juce::jlimit(0.0f, 0.95f, pDlyFb->load() + bip(CurveStore::DlyFb) * 0.95f);
    fp.dlyDuck = pDlyDuck->load();
    fp.dlyDamp = pDlyDamp->load();
    // TIME: カーブで拍長を±2オクターブ変調 (上=長く / 下=短く=加速)
    fp.dlyBeats = FxChain::delayTimeToBeats((int)pDlyTime->load())
                * std::exp2(bip(CurveStore::DlyTime) * 2.0f);

    fp.revAmt = c01(pRevAmt->load() + bip(CurveStore::RevAmt) * 1.0f);
    fp.revDecay = pRevDecay->load();
    fp.revShimmer = c01(pRevShimmer->load() + bip(CurveStore::RevShimmer) * 1.0f);
    fp.revDamp = pRevDamp->load();
    fp.revMod = pRevMod->load();

    fp.duckAmt = c01(pDuckAmt->load() + bip(CurveStore::DuckAmt) * 1.0f);
    // RATE: ±2オクターブを音楽的に量子化 (×4..×1/4)
    fp.duckBeats = FxChain::duckRateToBeats((int)pDuckRate->load())
                 * std::exp2((float)juce::roundToInt(bip(CurveStore::DuckRate) * 2.0f));
    fp.duckShape = juce::jlimit(0.5f, 8.0f, pDuckShape->load() + bip(CurveStore::DuckShape) * 7.5f);

    // ---- Stutter ----
    fp.stutAmt = c01((pStutAmt != nullptr ? pStutAmt->load() : 0.0f)
                     + bip(CurveStore::StutAmt) * 1.0f);
    //  グレイン長: Duck と同じく ±2オクターブを整数段へ量子化
    //  (連続変化させると拍から外れて気持ち悪くなるため)
    fp.stutBeats = FxChain::stutterRateToBeats(pStutRate != nullptr ? (int)pStutRate->load() : 11)
                 * std::exp2((float)juce::roundToInt(bip(CurveStore::StutRate) * 2.0f));
}

// ==========================================================
// RANDOM (メッセージスレッド専用)
//  MAINタブ + OSC ENVタブのみを対象に、音楽的に成立する範囲でランダマイズする。
//  MASTER / FX / FILTER / CONFIG(スケール設定・テーマ・リミッター) は変更しない。
//
//  「破綻しない」ための制約:
//   - 必ずOSC1が有効かつ十分なレベル → 無音にならない
//   - Start/EndKey は全OSCで共通の音域を使い、COARSEは和声的な度数から選ぶ
//     → OSC間が不協和にならない
//   - ライザー/ダウナーの向きは全OSCで揃える
//   - UNISONとDETUNE/SPREADを連動 → ユニゾン1本で過大デチューンにならない
//   - Pitchカーブは単調 → 上がったり下がったりする不自然な動きを避ける
//   - Scaleクオンタイズが有効ならキーをスケール構成音へスナップ
// ==========================================================
void LiftXAudioProcessor::randomizeMainAndOsc(bool lockOsc, bool lockCurves)
{
    juce::Random rng((juce::int64)juce::Time::getHighResolutionTicks());

    auto setP = [this](const juce::String& id, float v)
    {
        if (auto* prm = apvts.getParameter(id))
        {
            const auto& r = prm->getNormalisableRange();
            prm->setValueNotifyingHost(r.convertTo0to1(juce::jlimit(r.start, r.end, v)));
        }
    };
    auto rf     = [&rng](float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto ri     = [&rng](int lo, int hi)     { return lo + rng.nextInt(juce::jmax(1, hi - lo + 1)); };
    auto chance = [&rng](float p)            { return rng.nextFloat() < p; };
    // 対数レンジ用 (Hz系を聴感的に均一なランダムにする)
    auto rlog   = [&rf](float lo, float hi)  { return std::exp(rf(std::log(lo), std::log(hi))); };

    // ---- グローバル ----
    //  Bars: 音楽的な長さへバイアス (1/2, 1, 2, 4, 8)。1/32などの極端値は選ばない
    static const int kBarChoices[] = { 4, 5, 6, 7, 7, 8 };
    setP("bars", (float)kBarChoices[ri(0, 5)]);
    setP("attack",  rlog(0.5f, 30.0f));
    setP("release", rlog(120.0f, 900.0f));

    // ライザー / ダウナー (ダウナーは25%)
    const bool downer = chance(0.25f);

    // 全OSC共通の音域 (2〜5オクターブ)。ここを共有することで音程関係が保たれる
    const int span   = ri(24, 60);
    const int lowKey = ri(28, 52);
    const int highKey = juce::jlimit(24, 108, lowKey + span);

    // ---- OSC1-3 ----
    //  OSC1は必ず有効。2,3は確率的に追加する
    const int numActive = chance(0.45f) ? 1 : (chance(0.6f) ? 2 : 3);

    // COARSE候補: ユニゾン/オクターブ/完全5度/長短3度など和声的な度数のみ
    static const int kChordSteps[] = { 0, 0, 0, 12, -12, 7, -5, 3, 4, 5, 12, 7 };

    for (int i = 1; i <= RiserEngine::kNumOscs && !lockOsc; ++i)
    {
        const juce::String n(i);
        const bool on = (i <= numActive);

        setP("osc" + n + "On", on ? 1.0f : 0.0f);
        setP("osc" + n + "Solo", 0.0f);      // SOLO/MUTEは常にオフ (無音事故の防止)
        setP("osc" + n + "Mute", 0.0f);
        if (!on) continue;

        // 波形: Wavetable(5)はカスタム未ロード時にビルトインへ落ちるため除外
        setP("osc" + n + "Wave", (float)ri(0, 4));
        setP("osc" + n + "Pos", rf(0.0f, 1.0f));
        setP("osc" + n + "Level", i == 1 ? rf(0.70f, 1.0f) : rf(0.30f, 0.75f));
        setP("osc" + n + "Coarse", i == 1 ? 0.0f : (float)kChordSteps[ri(0, 11)]);

        const int uni = chance(0.55f) ? ri(3, 7) : 1;
        setP("osc" + n + "Uni", (float)uni);
        // ユニゾン本数と連動: 1本のときに大きなデチューンを掛けない
        setP("osc" + n + "Det",    uni > 1 ? rf(8.0f, 55.0f) : rf(0.0f, 12.0f));
        setP("osc" + n + "Spread", uni > 1 ? rf(0.55f, 1.0f) : rf(0.0f, 0.4f));

        setP("osc" + n + "KeyStart", (float)(downer ? highKey : lowKey));
        setP("osc" + n + "KeyEnd",   (float)(downer ? lowKey  : highKey));
    }

    // ---- ノイズ ----
    const bool useNoise = chance(0.6f);
    if (!lockOsc)
    {
    setP("noiseSolo", 0.0f);
    setP("noiseMute", 0.0f);
    setP("noiseType",  (float)ri(0, 2));
    setP("noiseLevel", useNoise ? rf(0.15f, 0.70f) : 0.0f);
    setP("noisePitch", rlog(200.0f, 6000.0f));
    setP("noiseRes",   rf(0.8f, 5.0f));
    setP("noiseRange", rf(2.0f, 7.0f));
    }

    // ---- OSC ENV カーブ (0-11 = OSC1-3 の Pitch/Level/Detune/Spread, 12-14 = Noise) ----
    //  FILTER Cutoff(15-18) / FILTER Res(19-22) / FX(23-34) のカーブは対象外
    auto makeCurve = [](float y0, float y1, float tension)
    {
        auto s = CurveSnapshot::makeDefault(y0, y1);
        s.pts[0].curve = juce::jlimit(-1.0f, 1.0f, tension);
        return s;
    };

    for (int o = 0; o < RiserEngine::kNumOscs && !lockCurves; ++o)
    {
        // PITCH: 必ず 0→1 の単調上昇 (Start→End)。テンションで加速/減速だけ変える
        mCurves.publish(CurveStore::oscCurve(o, 0), makeCurve(0.0f, 1.0f, rf(-0.5f, 0.7f)));

        // LEVEL: 半分は変化なし(中央フラット)、半分は控えめなフェードイン
        mCurves.publish(CurveStore::oscCurve(o, 1),
            chance(0.5f) ? makeCurve(0.5f, 0.5f, 0.0f)
                         : makeCurve(rf(0.20f, 0.45f), rf(0.55f, 0.85f), rf(-0.3f, 0.5f)));

        // DETUNE / SPREAD: 変化なし、または広がっていく方向へ軽く
        mCurves.publish(CurveStore::oscCurve(o, 2),
            chance(0.6f) ? makeCurve(0.5f, 0.5f, 0.0f)
                         : makeCurve(rf(0.35f, 0.5f), rf(0.55f, 0.8f), rf(-0.3f, 0.3f)));
        mCurves.publish(CurveStore::oscCurve(o, 3),
            chance(0.6f) ? makeCurve(0.5f, 0.5f, 0.0f)
                         : makeCurve(rf(0.4f, 0.5f), rf(0.55f, 0.75f), 0.0f));
    }

    // ノイズ: PITCH は上昇、LEVEL はフェードイン、RES はほぼ据え置き
    if (!lockCurves)
    {
    mCurves.publish(CurveStore::NoisePitch, makeCurve(rf(0.30f, 0.45f), rf(0.85f, 1.0f), rf(-0.3f, 0.6f)));
    mCurves.publish(CurveStore::NoiseLevel,
        useNoise ? makeCurve(rf(0.20f, 0.40f), rf(0.80f, 1.0f), rf(-0.2f, 0.6f))
                 : makeCurve(0.5f, 0.5f, 0.0f));
    mCurves.publish(CurveStore::NoiseRes,
        chance(0.7f) ? makeCurve(0.5f, 0.5f, 0.0f)
                     : makeCurve(rf(0.4f, 0.5f), rf(0.55f, 0.75f), 0.0f));
    }

    // Scaleクオンタイズが有効なら Start/End キーを構成音へ寄せる
    snapKeysToScale();
    rememberSnapState();

    mCurrentPresetName = "Random";
    mCurrentFactoryIndex = -1;
    mCurrentUserFile = juce::File();
}

// ==========================================================
// MUTATE (メッセージスレッド専用)
//  RANDOM が「全部作り直す」のに対し、こちらは現在の音を起点に
//  近傍だけを探索する。気に入った音を少しずつ育てられる。
//
//  ・連続値パラメーターは正規化値へ ±amount の揺らぎを加える
//  ・On/Off や Choice 系は触らない (構成が変わると別の音になってしまう)
//  ・カーブは制御点の y とテンションだけを軽く揺らす (x は動かさない)
//  ・MASTER / FX / FILTER / CONFIG は RANDOM と同じく対象外
// ==========================================================
void LiftXAudioProcessor::mutateMainAndOsc(float amount)
{
    const float amt = juce::jlimit(0.01f, 0.5f, amount);
    juce::Random rng((juce::int64)juce::Time::getHighResolutionTicks());

    auto jitter = [&](const juce::String& id, float scale = 1.0f)
    {
        auto* prm = apvts.getParameter(id);
        if (prm == nullptr) return;
        const float cur = prm->getValue();                       // 0..1 正規化値
        const float d = (rng.nextFloat() * 2.0f - 1.0f) * amt * scale;
        prm->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, cur + d));
    };

    for (int i = 1; i <= RiserEngine::kNumOscs; ++i)
    {
        const juce::String n(i);
        jitter("osc" + n + "Level");
        jitter("osc" + n + "Det");
        jitter("osc" + n + "Spread");
        jitter("osc" + n + "Pos");
        jitter("osc" + n + "Pan", 0.6f);
        jitter("osc" + n + "Fine", 0.5f);
        // キーは音域が飛ばないよう控えめに
        jitter("osc" + n + "KeyStart", 0.25f);
        jitter("osc" + n + "KeyEnd", 0.25f);
    }

    jitter("noiseLevel");
    jitter("noisePitch", 0.7f);
    jitter("noiseRes");
    jitter("noiseRange", 0.7f);
    jitter("noisePan", 0.6f);
    jitter("attack", 0.5f);
    jitter("release", 0.5f);

    // ---- カーブ: 形の「気配」を残したまま少し崩す ----
    static const int kMutCurves[] = {
        CurveStore::Osc1Pitch, CurveStore::Osc1Level, CurveStore::Osc1Detune, CurveStore::Osc1Spread,
        CurveStore::Osc2Pitch, CurveStore::Osc2Level,
        CurveStore::Osc3Pitch, CurveStore::Osc3Level,
        CurveStore::NoisePitch, CurveStore::NoiseLevel, CurveStore::NoiseRes };

    for (int idx : kMutCurves)
    {
        auto c = mCurves.get(idx);
        for (int i = 0; i < c.numPoints; ++i)
        {
            c.pts[(size_t)i].y = juce::jlimit(0.0f, 1.0f,
                c.pts[(size_t)i].y + (rng.nextFloat() * 2.0f - 1.0f) * amt * 0.35f);
            c.pts[(size_t)i].curve = juce::jlimit(-1.0f, 1.0f,
                c.pts[(size_t)i].curve + (rng.nextFloat() * 2.0f - 1.0f) * amt * 0.8f);
        }
        mCurves.publish(idx, c);
    }

    snapKeysToScale();
    rememberSnapState();
    mCurrentPresetName = mCurrentPresetName.endsWith("*") ? mCurrentPresetName
                                                          : mCurrentPresetName + "*";
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
juce::ValueTree LiftXAudioProcessor::buildStateTree()
{
    auto state = apvts.copyState();

    for (int i = state.getNumChildren() - 1; i >= 0; --i)
        if (state.getChild(i).hasType("CURVES"))
            state.removeChild(i, nullptr);
    state.appendChild(mCurves.toValueTree(), nullptr);

    // プリセット名も保存する (DAW再起動後にヘッダー表示が "Init" に戻る問題の修正)
    state.setProperty("presetName", mCurrentPresetName, nullptr);
    state.setProperty("stateVersion", 2, nullptr);
    return state;
}

void LiftXAudioProcessor::applyStateTree(juce::ValueTree state)
{
    if (!state.isValid() || !state.hasType(apvts.state.getType()))
        return;

    // カーブ: CURVESが無い/旧バージョンの場合はデフォルトへ戻す。
    //  (前のプリセットのカーブが残ったまま新しいプリセットが鳴る事故を防ぐ)
    const auto curveTree = state.getChildWithName("CURVES");
    if (!curveTree.isValid() || (int)curveTree.getProperty("version", 1) < 2)
        mCurves.resetToDefaults();
    else
        mCurves.fromValueTree(curveTree);

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
        else
        {
            mWavetables[(size_t)i].clearCustom();
        }
    }

    // 復元したKey/Scaleを「スナップ済み」として記録し、
    // ステート適用が自動スナップを誘発してStart/Endを書き換えるのを防ぐ。
    rememberSnapState();
}

void LiftXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = buildStateTree().createXml())
        copyXmlToBinary(*xml, destData);
}

void LiftXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
        return;

    const auto tree = juce::ValueTree::fromXml(*xml);
    applyStateTree(tree);

    // プリセット名の復元 (旧セッションには存在しないため空なら "Init" 扱い)
    const auto nm = tree.getProperty("presetName", juce::String()).toString();
    mCurrentPresetName = nm.isNotEmpty() ? nm : juce::String("Init");
    rememberSnapState();
}

// ==========================================================
// プリセット (メッセージスレッド専用)
// ==========================================================
juce::File LiftXAudioProcessor::getUserPresetDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("LIFT-X").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

void LiftXAudioProcessor::saveUserPreset(const juce::String& name, const juce::String& subCategory)
{
    auto dir = getUserPresetDir();
    if (subCategory.isNotEmpty())
    {
        dir = dir.getChildFile(juce::File::createLegalFileName(subCategory));
        dir.createDirectory();
    }
    const auto file = dir.getChildFile(juce::File::createLegalFileName(name) + ".xml");

    if (auto xml = buildStateTree().createXml())
        xml->writeTo(file);
    mCurrentPresetName = name;
    mCurrentFactoryIndex = -1;
    mCurrentUserFile = file;
}

bool LiftXAudioProcessor::loadUserPreset(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto xml = juce::parseXML(file);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
        return false;
    applyStateTree(juce::ValueTree::fromXml(*xml));
    mCurrentPresetName = file.getFileNameWithoutExtension();
    mCurrentFactoryIndex = -1;
    mCurrentUserFile = file;
    rememberSnapState();
    return true;
}

void LiftXAudioProcessor::initPreset()
{
    // 全パラメーターをデフォルトへ + カーブ初期化 + カスタムWT解除
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
            rp->setValueNotifyingHost(rp->getDefaultValue());

    mCurves.resetToDefaults();
    for (int i = 0; i < RiserEngine::kNumOscs; ++i)
        clearCustomWavetable(i);
    mCurrentPresetName = "Init";
    mCurrentFactoryIndex = -1;
    mCurrentUserFile = juce::File();
    rememberSnapState();   // Init自体は自動スナップの対象外
}

void LiftXAudioProcessor::loadFactoryPreset(int index)
{
    if (index < 0 || index >= FactoryPresets::count())
        return;
    FactoryPresets::apply(*this, index);
    mCurrentPresetName = FactoryPresets::nameOf(index);
    mCurrentFactoryIndex = index;
    mCurrentUserFile = juce::File();
    // プリセットが持つStart/Endキーを尊重する (読み込み直後に勝手にスナップしない)
    rememberSnapState();
}

void LiftXAudioProcessor::stepPreset(int delta)
{
    if (delta == 0) return;

    struct Entry
    {
        bool factory;
        int idx;
        juce::File file;
        juce::String name;
    };
    std::vector<Entry> list;

    const int nFactory = FactoryPresets::count();
    for (int i = 0; i < nFactory; ++i)
        list.push_back({ true, i, {}, FactoryPresets::nameOf(i) });

    auto files = getUserPresetDir().findChildFiles(juce::File::findFiles, true, "*.xml");
    std::sort(files.begin(), files.end(),
              [](const juce::File& a, const juce::File& b)
              { return a.getFullPathName().compareIgnoreCase(b.getFullPathName()) < 0; });
    for (const auto& f : files)
        list.push_back({ false, -1, f, f.getFileNameWithoutExtension() });

    if (list.empty()) return;
    const int n = (int)list.size();

    // 現在位置の特定:
    //  1) 直前に読み込んだ実体 (Factoryインデックス / Userファイル) を優先。
    //     → Factory と User で同名プリセットがあってもナビゲーションが破綻しない。
    //  2) 見つからなければ名前一致でフォールバック (ステート復元直後など)。
    int cur = -1;
    if (mCurrentFactoryIndex >= 0 && mCurrentFactoryIndex < nFactory)
    {
        cur = mCurrentFactoryIndex;
    }
    else if (mCurrentUserFile != juce::File())
    {
        for (int i = nFactory; i < n; ++i)
            if (list[(size_t)i].file == mCurrentUserFile) { cur = i; break; }
    }
    if (cur < 0)
    {
        for (int i = 0; i < n; ++i)
            if (list[(size_t)i].name == mCurrentPresetName) { cur = i; break; }
    }

    const int next = (cur < 0) ? (delta > 0 ? 0 : n - 1)
                               : ((cur + delta) % n + n) % n;

    const auto& e = list[(size_t)next];
    if (e.factory) loadFactoryPreset(e.idx);
    else           loadUserPreset(e.file);
}

// ==========================================================
juce::AudioProcessorEditor* LiftXAudioProcessor::createEditor()
{
    // カラーテーマをエディタ構築前に適用 (グローバル設定から復元)
    LiftColors::setTheme(getGlobalSettings().getIntValue("colorTheme", 0));
    return new LiftXAudioProcessorEditor(*this);
}

// ==========================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LiftXAudioProcessor();
}
