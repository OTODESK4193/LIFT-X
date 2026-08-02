// ==========================================
// File: FilterPanel.cpp
// ==========================================
#include "FilterPanel.h"

FilterPanel::FilterPanel(LiftXAudioProcessor& p)
    : proc(p)
{
    // サブタブ (FLT 1 ~ FLT 4)
    static const char* fltNames[4] = { "FLT 1", "FLT 2", "FLT 3", "FLT 4" };
    for (int i = 0; i < 4; ++i)
    {
        subTabs[(size_t)i] = std::make_unique<juce::TextButton>(fltNames[i]);
        subTabs[(size_t)i]->onClick = [this, i] { setSub(i); };
        addAndMakeVisible(*subTabs[(size_t)i]);
    }

    // ENV ターゲット切り替え (ENV: CUTOFF / ENV: RES)
    static const char* envTargetNames[2] = { "ENV: CUTOFF", "ENV: RES" };
    for (int i = 0; i < 2; ++i)
    {
        envTargetTabs[(size_t)i] = std::make_unique<juce::TextButton>(envTargetNames[i]);
        envTargetTabs[(size_t)i]->onClick = [this, i]
        {
            activeEnvTarget = i;
            setSub(activeSub);
        };
        addAndMakeVisible(*envTargetTabs[(size_t)i]);
    }

    addAndMakeVisible(editor);
    editor.setBipolar(true);
    editor.setProgressProvider([this] { return proc.getEnvPosition(); });
    editor.onChanged = [this](const CurveSnapshot& s)
    {
        const int curveIdx = (activeEnvTarget == 0) ? (CurveStore::Filter1 + activeSub)
                                                    : (CurveStore::Filter1Res + activeSub);
        proc.getCurves().publish(curveIdx, s);
    };

    addAndMakeVisible(responseDisplay);

    onToggle = std::make_unique<GlowToggle>("ENABLE", LiftColors::accentFilter);
    addAndMakeVisible(*onToggle);

    typeBox.addItemList({ "LowPass", "HighPass", "BandPass", "Notch" }, 1);
    addAndMakeVisible(typeBox);
    typeLabel.setText("TYPE", juce::dontSendNotification);
    typeLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    typeLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
    typeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(typeLabel);

    auto setupKnob = [this](ValueKnob& k, juce::Label& l, const juce::String& text)
    {
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 14);
        k.getProperties().set("accentId", (int)LiftColors::IdBabyBlue); // テーマ連動
        addAndMakeVisible(k);

        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, LiftColors::textDim);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };
    setupKnob(cutoffKnob, cutoffLabel, "CUTOFF");
    setupKnob(resKnob, resLabel, "RES");
    setupKnob(envKnob, envLabel, "ENV AMT");

    // ソース別ルーティングボタン (点灯=通す / 消灯=バイパス)
    static const char* srcBtnNames[4] = { "OSC 1", "OSC 2", "OSC 3", "NOISE" };
    for (int s = 0; s < 4; ++s)
    {
        routeToggles[(size_t)s] = std::make_unique<GlowToggle>(srcBtnNames[s],
            s < 3 ? LiftColors::accentOsc : LiftColors::lilac);
        addAndMakeVisible(*routeToggles[(size_t)s]);
    }
    routeLabel.setText("ROUTE:", juce::dontSendNotification);
    routeLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    routeLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
    addAndMakeVisible(routeLabel);

    hint.setFont(juce::Font(juce::FontOptions(12.0f)));
    hint.setColour(juce::Label::textColourId, LiftColors::textDim);
    hint.setJustificationType(juce::Justification::centredLeft);
    hint.setText("Curve x ENV AMT sweeps CUTOFF & RES across full range "
                 "(ZDF/TPT: stable even under fast sweeps)",
                 juce::dontSendNotification);
    addAndMakeVisible(hint);

    setSub(0);
    startTimerHz(30);
}

// マルチENV変化幅をCUTOFF/RESノブへ動的表示および応答曲線更新
void FilterPanel::timerCallback()
{
    if (!isVisible()) return;

    const juce::String n(activeSub + 1);
    auto* prmCut = proc.apvts.getParameter("flt" + n + "Cutoff");
    auto* prmRes = proc.apvts.getParameter("flt" + n + "Res");
    const float envAmt = proc.apvts.getRawParameterValue("flt" + n + "Env")->load();
    const bool onState = proc.apvts.getRawParameterValue("flt" + n + "On")->load() > 0.5f;
    const int typeState = (int)proc.apvts.getRawParameterValue("flt" + n + "Type")->load();
    const float cutoffVal = proc.apvts.getRawParameterValue("flt" + n + "Cutoff")->load();
    const float resVal = proc.apvts.getRawParameterValue("flt" + n + "Res")->load();

    // ENV評価位置: Auto=Progress / Manual=LIFTノブ
    const float envPos = proc.getEnvPosition();
    const float cutBipVal = (proc.getCurves().read(CurveStore::Filter1 + activeSub).evaluate(envPos) - 0.5f) * 2.0f;
    const float cutModAmount = juce::jlimit(-1.0f, 1.0f, envAmt * cutBipVal);

    // 上限は DSP と同じ sr*0.45 を使う。20000固定にしていたため、
    // 44.1kHz では実際(19845Hz)より広く、48kHz以上では狭く表示されていた。
    const float maxHz = (float)(proc.getPreparedSampleRate() * 0.45);
    const float baseHz = juce::jlimit(20.0f, maxHz, cutoffVal);
    const float logCut = std::log2(baseHz);
    const float logTarget = cutModAmount >= 0.0f ? logCut + cutModAmount * (std::log2(maxHz) - logCut)
                                               : logCut + cutModAmount * (logCut - std::log2(20.0f));
    const float liveCutoffHz = juce::jlimit(20.0f, maxHz, std::exp2(logTarget));

    // Resonance 独立変調 (Filter1Res..4Res) のリアルタイム値計算
    const float resBipVal = (proc.getCurves().read(CurveStore::Filter1Res + activeSub).evaluate(envPos) - 0.5f) * 2.0f;
    const float resModAmount = juce::jlimit(-1.0f, 1.0f, envAmt * resBipVal);
    const float baseRes = juce::jlimit(0.5f, 12.0f, resVal);
    const float liveResVal = juce::jlimit(0.5f, 12.0f,
        resModAmount >= 0.0f ? baseRes + resModAmount * (12.0f - baseRes)
                             : baseRes + resModAmount * (baseRes - 0.5f));

    responseDisplay.setParams(typeState, cutoffVal, liveResVal, liveCutoffHz, onState);

    // CUTOFF ノブの ModBand 更新 (DSP と同じ maxHz を使う)
    ModBand::update(cutoffKnob, prmCut,
                    proc.getCurves().read(CurveStore::Filter1 + activeSub), 1.0f, envPos,
                    [envAmt, maxHz](float b, float bip) {
                        const float mAmt = juce::jlimit(-1.0f, 1.0f, envAmt * bip);
                        const float lCut = std::log2(juce::jlimit(20.0f, maxHz, b));
                        const float lTgt = mAmt >= 0.0f ? lCut + mAmt * (std::log2(maxHz) - lCut)
                                                        : lCut + mAmt * (lCut - std::log2(20.0f));
                        return juce::jlimit(20.0f, maxHz, std::exp2(lTgt));
                    });

    // RES ノブの ModBand 更新 (独立した Filter1Res..4Res カーブ使用)
    ModBand::update(resKnob, prmRes,
                    proc.getCurves().read(CurveStore::Filter1Res + activeSub), 1.0f, envPos,
                    [envAmt](float b, float bip) {
                        const float mAmt = juce::jlimit(-1.0f, 1.0f, envAmt * bip);
                        const float baseRes = juce::jlimit(0.5f, 12.0f, b);
                        return juce::jlimit(0.5f, 12.0f, mAmt >= 0.0f ? baseRes + mAmt * (12.0f - baseRes)
                                                                      : baseRes + mAmt * (baseRes - 0.5f));
                    });
}

void FilterPanel::setSub(int idx)
{
    activeSub = juce::jlimit(0, 3, idx);
    const int curveIdx = (activeEnvTarget == 0) ? (CurveStore::Filter1 + activeSub)
                                                : (CurveStore::Filter1Res + activeSub);
    const juce::String n(activeSub + 1);

    editor.setSnapshot(proc.getCurves().get(curveIdx));
    editor.setAccent(LiftColors::curveAccent(curveIdx));
    editor.setTitle(CurveStore::name(curveIdx));

    // アタッチメント再接続 (必ず reset → 再生成の順)
    onAtt.reset();
    typeAtt.reset();
    cutoffAtt.reset();
    resAtt.reset();
    envAtt.reset();

    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "flt" + n + "On", *onToggle);
    typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, "flt" + n + "Type", typeBox);
    cutoffAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "flt" + n + "Cutoff", cutoffKnob);
    resAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "flt" + n + "Res", resKnob);
    envAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "flt" + n + "Env", envKnob);

    static const char* srcIds[4] = { "Osc1", "Osc2", "Osc3", "Noise" };
    for (int s = 0; s < 4; ++s)
    {
        routeAtts[(size_t)s].reset();
        routeAtts[(size_t)s] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "flt" + n + "Route" + srcIds[s], *routeToggles[(size_t)s]);
    }

    for (int i = 0; i < 4; ++i)
        styleTabButton(*subTabs[(size_t)i], i == activeSub);

    for (int i = 0; i < 2; ++i)
        styleTabButton(*envTargetTabs[(size_t)i], i == activeEnvTarget);

    // サブタブ切替直後に前のフィルターの帯が1フレーム残るのを防ぐ
    timerCallback();
}

void FilterPanel::styleTabButton(juce::TextButton& b, bool active)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? LiftColors::accentFilter.withAlpha(0.22f) : LiftColors::knobTrack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? LiftColors::text : LiftColors::textDim);
    b.repaint();
}

void FilterPanel::paint(juce::Graphics& g)
{
    LiftColors::paintPanel(g, getLocalBounds().toFloat().reduced(2.0f));
}

void FilterPanel::resized()
{
    auto r = getLocalBounds().reduced(12, 10);

    // サブタブ行 (FLT 1 ~ FLT 4) と 右側に ENV: CUTOFF / ENV: RES 切り替えタブ
    auto top = r.removeFromTop(30);
    for (int i = 0; i < 4; ++i)
    {
        subTabs[(size_t)i]->setBounds(top.removeFromLeft(90));
        top.removeFromLeft(6);
    }

    r.removeFromTop(4);

    // ヒント行
    hint.setBounds(r.removeFromBottom(18));
    r.removeFromBottom(4);

    // ルーティング行
    auto routeRow = r.removeFromTop(24);
    routeLabel.setBounds(routeRow.removeFromLeft(64));
    for (int s = 0; s < 4; ++s)
    {
        routeToggles[(size_t)s]->setBounds(routeRow.removeFromLeft(86));
        routeRow.removeFromLeft(6);
    }

    r.removeFromTop(6);

    // メイン領域分割: 左2/3をEnv画面(editor)、右1/3をフィルターコントロール(responseDisplay + ノブ群)
    int totalWidth = r.getWidth();
    int rightWidth = totalWidth / 3;  // 右1/3
    int leftWidth = totalWidth - rightWidth - 8; // 左2/3

    auto leftArea = r.removeFromLeft(leftWidth);
    r.removeFromLeft(8);
    auto rightArea = r;

    // 左2/3: 上部に ENV: CUTOFF / ENV: RES タブ行、その下に Env画面 (editor)
    auto envTabRow = leftArea.removeFromTop(26);
    envTargetTabs[0]->setBounds(envTabRow.removeFromLeft(110));
    envTabRow.removeFromLeft(6);
    envTargetTabs[1]->setBounds(envTabRow.removeFromLeft(100));
    leftArea.removeFromTop(4);

    editor.setBounds(leftArea);

    // 右1/3:
    // 1. 最上部に ENABLE ボタン と TYPE コンボ
    auto enableTypeRow = rightArea.removeFromTop(26);
    onToggle->setBounds(enableTypeRow.removeFromLeft(80));
    enableTypeRow.removeFromLeft(8);
    typeLabel.setBounds(enableTypeRow.removeFromLeft(40));
    typeBox.setBounds(enableTypeRow);

    rightArea.removeFromTop(6);

    // 2. 下部に 3個のノブ (高さ85pxに制限し、画像2(FX画面)と同じコンパクトで美しい配置にする)
    auto knobRowArea = rightArea.removeFromBottom(85);

    int knobWidth = (knobRowArea.getWidth() - 12) / 3;

    auto layoutKnobCell = [](juce::Rectangle<int> area, juce::Label& l, ValueKnob& k)
    {
        l.setBounds(area.removeFromTop(15));
        k.setBounds(area.reduced(2, 0));
    };

    auto kCell1 = knobRowArea.removeFromLeft(knobWidth);
    knobRowArea.removeFromLeft(6);
    layoutKnobCell(kCell1, cutoffLabel, cutoffKnob);

    auto kCell2 = knobRowArea.removeFromLeft(knobWidth);
    knobRowArea.removeFromLeft(6);
    layoutKnobCell(kCell2, resLabel, resKnob);

    auto kCell3 = knobRowArea;
    layoutKnobCell(kCell3, envLabel, envKnob);

    // 3. 余った中央上部全体を FilterResponseDisplay に割り当て
    rightArea.removeFromBottom(6);
    responseDisplay.setBounds(rightArea);
}
