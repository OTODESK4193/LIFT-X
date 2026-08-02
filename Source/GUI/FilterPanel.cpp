// ==========================================
// File: FilterPanel.cpp
// ==========================================
#include "FilterPanel.h"

FilterPanel::FilterPanel(LiftXAudioProcessor& p)
    : proc(p)
{
    for (int i = 0; i < 4; ++i)
    {
        subTabs[(size_t)i] = std::make_unique<juce::TextButton>("FLT " + juce::String(i + 1));
        subTabs[(size_t)i]->onClick = [this, i] { setSub(i); };
        addAndMakeVisible(*subTabs[(size_t)i]);
    }

    addAndMakeVisible(editor);
    editor.setBipolar(true);
    editor.setProgressProvider([this] { return proc.getEnvPosition(); });
    editor.onChanged = [this](const CurveSnapshot& s)
    {
        proc.getCurves().publish(CurveStore::Filter1 + activeSub, s);
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
    hint.setText("Curve x ENV AMT sweeps CUTOFF across full range "
                 "(ZDF/TPT: stable even under fast sweeps)",
                 juce::dontSendNotification);
    addAndMakeVisible(hint);

    setSub(0);
    startTimerHz(30);
}

// マルチENV変化幅をCUTOFFノブへ動的表示および応答曲線更新
void FilterPanel::timerCallback()
{
    if (!isVisible()) return;

    const juce::String n(activeSub + 1);
    auto* prm = proc.apvts.getParameter("flt" + n + "Cutoff");
    const float envAmt = proc.apvts.getRawParameterValue("flt" + n + "Env")->load();
    const bool onState = proc.apvts.getRawParameterValue("flt" + n + "On")->load() > 0.5f;
    const int typeState = (int)proc.apvts.getRawParameterValue("flt" + n + "Type")->load();
    const float cutoffVal = proc.apvts.getRawParameterValue("flt" + n + "Cutoff")->load();
    const float resVal = proc.apvts.getRawParameterValue("flt" + n + "Res")->load();

    // ENV評価位置: Auto=Progress / Manual=LIFTノブ
    const float envPos = proc.getEnvPosition();
    const float bipVal = (proc.getCurves().read(CurveStore::Filter1 + activeSub).evaluate(envPos) - 0.5f) * 2.0f;
    const float modAmount = juce::jlimit(-1.0f, 1.0f, envAmt * bipVal);

    const float maxHz = 20000.0f;
    const float baseHz = juce::jlimit(20.0f, maxHz, cutoffVal);
    const float logCut = std::log2(baseHz);
    const float logTarget = modAmount >= 0.0f ? logCut + modAmount * (std::log2(maxHz) - logCut)
                                               : logCut + modAmount * (logCut - std::log2(20.0f));
    const float liveCutoffHz = juce::jlimit(20.0f, maxHz, std::exp2(logTarget));

    responseDisplay.setParams(typeState, cutoffVal, resVal, liveCutoffHz, onState);

    ModBand::update(cutoffKnob, prm,
                    proc.getCurves().read(CurveStore::Filter1 + activeSub), 1.0f, envPos,
                    [envAmt](float b, float bip) {
                        const float mAmt = juce::jlimit(-1.0f, 1.0f, envAmt * bip);
                        const float lCut = std::log2(juce::jlimit(20.0f, 20000.0f, b));
                        const float lTgt = mAmt >= 0.0f ? lCut + mAmt * (std::log2(20000.0f) - lCut)
                                                        : lCut + mAmt * (lCut - std::log2(20.0f));
                        return juce::jlimit(20.0f, 20000.0f, std::exp2(lTgt));
                    });
}

void FilterPanel::setSub(int idx)
{
    activeSub = juce::jlimit(0, 3, idx);
    const int curveIdx = CurveStore::Filter1 + activeSub;
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
    g.setColour(LiftColors::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 8.0f);
}

void FilterPanel::resized()
{
    auto r = getLocalBounds().reduced(12, 10);

    // サブタブ行 (FLT 1 ~ FLT 4)
    auto top = r.removeFromTop(30);
    for (int i = 0; i < 4; ++i)
    {
        subTabs[(size_t)i]->setBounds(top.removeFromLeft(96));
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

    // 左2/3: Env画面
    editor.setBounds(leftArea);

    // 右1/3:
    // 1. 最上部に ENABLE ボタン と TYPE コンボ
    auto enableTypeRow = rightArea.removeFromTop(26);
    onToggle->setBounds(enableTypeRow.removeFromLeft(80));
    enableTypeRow.removeFromLeft(8);
    typeLabel.setBounds(enableTypeRow.removeFromLeft(40));
    typeBox.setBounds(enableTypeRow);

    rightArea.removeFromTop(8);

    // 2. 中央に Filterリアルタイム応答カーブ表示 (高さ 135px を確保)
    responseDisplay.setBounds(rightArea.removeFromTop(135));

    rightArea.removeFromTop(8);

    // 3. 下部に 3個のノブ (CUTOFF / RES / ENV AMT)
    // ノブセルの中で上に名称ラベル(16px)、下にノブを配置
    int knobWidth = (rightArea.getWidth() - 12) / 3;

    auto layoutKnobCell = [](juce::Rectangle<int> area, juce::Label& l, ValueKnob& k)
    {
        l.setBounds(area.removeFromTop(16));
        area.removeFromTop(2);
        k.setBounds(area);
    };

    auto kCell1 = rightArea.removeFromLeft(knobWidth);
    rightArea.removeFromLeft(6);
    layoutKnobCell(kCell1, cutoffLabel, cutoffKnob);

    auto kCell2 = rightArea.removeFromLeft(knobWidth);
    rightArea.removeFromLeft(6);
    layoutKnobCell(kCell2, resLabel, resKnob);

    auto kCell3 = rightArea;
    layoutKnobCell(kCell3, envLabel, envKnob);
}
