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
    editor.setProgressProvider([this] { return proc.getUiProgress(); });
    editor.onChanged = [this](const CurveSnapshot& s)
    {
        proc.getCurves().publish(CurveStore::Filter1 + activeSub, s);
    };

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
        k.setColour(juce::Slider::rotarySliderFillColourId, LiftColors::accentFilter);
        addAndMakeVisible(k);

        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
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
    hint.setText(juce::CharPointer_UTF8(
        "\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x96\xc3\x97""ENV AMT \xe3\x81\xa7\xe3\x82\xab\xe3\x83\x83\xe3\x83\x88\xe3\x82\xaa\xe3\x83\x95\xe3\x82\x92"
        "\xc2\xb1""5\xe3\x82\xaa\xe3\x82\xaf\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x96\xe5\xa4\x89\xe8\xaa\xbf (ZDF/TPT: \xe6\x80\xa5\xe6\xbf\x80\xe3\x81\xaa"
        "\xe3\x82\xb9\xe3\x82\xa4\xe3\x83\xbc\xe3\x83\x97\xe3\x81\xa7\xe3\x82\x82\xe7\xa0\xb4\xe7\xb6\xbb\xe3\x81\x97\xe3\x81\xbe\xe3\x81\x9b\xe3\x82\x93)"),
        juce::dontSendNotification);
    addAndMakeVisible(hint);

    setSub(0);
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

    // サブタブ行
    auto top = r.removeFromTop(30);
    for (int i = 0; i < 4; ++i)
    {
        subTabs[(size_t)i]->setBounds(top.removeFromLeft(96));
        top.removeFromLeft(6);
    }

    // 右側: 選択中フィルターのコントロール
    auto rightCol = r.removeFromRight(120).reduced(4);
    onToggle->setBounds(rightCol.removeFromTop(26));
    rightCol.removeFromTop(6);
    typeLabel.setBounds(rightCol.removeFromTop(14));
    typeBox.setBounds(rightCol.removeFromTop(24));
    rightCol.removeFromTop(6);

    auto knobCell = [&rightCol](juce::Label& l, ValueKnob& k)
    {
        l.setBounds(rightCol.removeFromTop(14));
        k.setBounds(rightCol.removeFromTop(96).reduced(6));
        rightCol.removeFromTop(2);
    };
    knobCell(cutoffLabel, cutoffKnob);
    knobCell(resLabel, resKnob);
    knobCell(envLabel, envKnob);

    r.removeFromTop(6);

    // ルーティング行 (点灯=このフィルターを通る)
    auto routeRow = r.removeFromTop(24);
    routeLabel.setBounds(routeRow.removeFromLeft(64));
    for (int s = 0; s < 4; ++s)
    {
        routeToggles[(size_t)s]->setBounds(routeRow.removeFromLeft(86));
        routeRow.removeFromLeft(6);
    }

    r.removeFromTop(6);
    hint.setBounds(r.removeFromBottom(18));
    r.removeFromBottom(4);
    editor.setBounds(r);
}
