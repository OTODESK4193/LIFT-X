// ==========================================
// File: MainPanel.cpp
// ==========================================
#include "MainPanel.h"

MainPanel::MainPanel(LiftXAudioProcessor& p)
    : proc(p), progressStrip(p), waveStrip(p), browser(p)
{
    // ---- グローバル ----
    setupKnob(liftCell, "LIFT", "lift", LiftColors::accentMaster);
    setupKnob(attackCell, "ATTACK", "attack", LiftColors::accentMaster);
    setupKnob(releaseCell, "RELEASE", "release", LiftColors::accentMaster);
    setupKnob(masterCell, "MASTER", "master", LiftColors::accentMaster);

    barsLabel.setText("BARS", juce::dontSendNotification);
    barsLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    barsLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
    barsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(barsLabel);
    setupCombo(barsBox, "bars", { "1", "2", "4", "8", "16" });

    addAndMakeVisible(progressStrip);
    addAndMakeVisible(waveStrip);

    // ---- オシレーター ----
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);

        oscOn[(size_t)i] = std::make_unique<GlowToggle>("OSC " + n, LiftColors::accentOsc);
        oscSolo[(size_t)i] = std::make_unique<GlowToggle>("S", LiftColors::peach);
        oscMute[(size_t)i] = std::make_unique<GlowToggle>("M", LiftColors::rose);
        for (auto* t : { oscOn[(size_t)i].get(), oscSolo[(size_t)i].get(), oscMute[(size_t)i].get() })
            addAndMakeVisible(*t);

        buttonAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "osc" + n + "On", *oscOn[(size_t)i]));
        buttonAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "osc" + n + "Solo", *oscSolo[(size_t)i]));
        buttonAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "osc" + n + "Mute", *oscMute[(size_t)i]));

        setupCombo(waveBox[(size_t)i], "osc" + n + "Wave",
                   { "Sine", "Triangle", "Square", "Saw", "FM", "Wavetable" });

        waveDisp[(size_t)i].setAccent(LiftColors::accentOsc);
        addAndMakeVisible(waveDisp[(size_t)i]);

        browseBtn[(size_t)i].setButtonText("BROWSE");
        rndBtn[(size_t)i].setButtonText("RND");
        addAndMakeVisible(browseBtn[(size_t)i]);
        addAndMakeVisible(rndBtn[(size_t)i]);
        browseBtn[(size_t)i].onClick = [this, i] { browser.openFor(i); };
        rndBtn[(size_t)i].onClick = [this, i] { browser.loadRandomFor(i); };

        setupKnob(oscPos[(size_t)i], "POS", "osc" + n + "Pos", LiftColors::accentOsc);
        setupKnob(oscLevel[(size_t)i], "LEVEL", "osc" + n + "Level", LiftColors::accentOsc);
        setupKnob(oscCoarse[(size_t)i], "COARSE", "osc" + n + "Coarse", LiftColors::accentOsc);
        setupKnob(oscUni[(size_t)i], "UNISON", "osc" + n + "Uni", LiftColors::accentOsc);
        setupKnob(oscDet[(size_t)i], "DETUNE", "osc" + n + "Det", LiftColors::accentOsc);
        setupKnob(oscSpread[(size_t)i], "SPREAD", "osc" + n + "Spread", LiftColors::accentOsc);

        addAndMakeVisible(keyStartBtn[(size_t)i]);
        addAndMakeVisible(keyEndBtn[(size_t)i]);
        keyStartBtn[(size_t)i].onClick = [this, i]
        { armLearn("osc" + juce::String(i + 1) + "KeyStart", keyStartBtn[(size_t)i]); };
        keyEndBtn[(size_t)i].onClick = [this, i]
        { armLearn("osc" + juce::String(i + 1) + "KeyEnd", keyEndBtn[(size_t)i]); };

        // 波形表示ソース
        waveDisp[(size_t)i].setSource([this, i](float* out, int nPts)
        {
            const int mode = (int)proc.apvts.getRawParameterValue(
                "osc" + juce::String(i + 1) + "Wave")->load();
            const float pos = proc.apvts.getRawParameterValue(
                "osc" + juce::String(i + 1) + "Pos")->load();
            const bool useCustom = (mode == RiserEngine::CustomWT);
            const float morph = useCustom ? pos : (float)mode * 0.25f;
            proc.getWavetable(i).getDisplayWave(morph, out, nPts, useCustom);
        });
    }

    // ---- ノイズ ----
    noiseTitle.setText("NOISE", juce::dontSendNotification);
    noiseTitle.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    noiseTitle.setColour(juce::Label::textColourId, LiftColors::lilac);
    addAndMakeVisible(noiseTitle);

    noiseSolo = std::make_unique<GlowToggle>("S", LiftColors::peach);
    noiseMute = std::make_unique<GlowToggle>("M", LiftColors::rose);
    addAndMakeVisible(*noiseSolo);
    addAndMakeVisible(*noiseMute);
    buttonAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "noiseSolo", *noiseSolo));
    buttonAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "noiseMute", *noiseMute));

    setupCombo(noiseTypeBox, "noiseType", { "White", "Pink", "Brown" });
    setupKnob(noiseLevel, "LEVEL", "noiseLevel", LiftColors::lilac);
    setupKnob(noisePitch, "PITCH", "noisePitch", LiftColors::lilac);
    setupKnob(noiseRes, "RES", "noiseRes", LiftColors::lilac);
    setupKnob(noiseRange, "RANGE", "noiseRange", LiftColors::lilac);

    // ---- ブラウザ (最前面オーバーレイ) ----
    addChildComponent(browser);
    browser.onLoaded = [this]
    {
        for (int i = 0; i < 3; ++i)
            refreshWaveDisplay(i);
    };

    // ModBand用パラメーターキャッシュ
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);
        prmOscLevel[(size_t)i] = proc.apvts.getParameter("osc" + n + "Level");
        prmOscDet[(size_t)i] = proc.apvts.getParameter("osc" + n + "Det");
        prmOscSpread[(size_t)i] = proc.apvts.getParameter("osc" + n + "Spread");
    }
    prmNoiseLevel = proc.apvts.getParameter("noiseLevel");
    prmNoiseRes = proc.apvts.getParameter("noiseRes");
    prmNoisePitch = proc.apvts.getParameter("noisePitch");

    refreshKeyButtons();
    for (int i = 0; i < 3; ++i)
        refreshWaveDisplay(i);

    startTimerHz(30);
}

// ==========================================================
void MainPanel::timerCallback()
{
    // ---- LIFT Auto: ノブをProgressへ追従 ----
    const float prog = proc.getUiProgress();
    const bool autoMode = proc.apvts.getRawParameterValue("liftMode")->load() > 0.5f;
    if (autoMode)
    {
        if (liftCell.knob.isEnabled())
            liftCell.knob.setEnabled(false);
        liftCell.knob.setValue(prog, juce::dontSendNotification);
    }
    else if (!liftCell.knob.isEnabled())
    {
        liftCell.knob.setEnabled(true);
        liftCell.knob.setValue(proc.apvts.getRawParameterValue("lift")->load(),
                               juce::dontSendNotification);
    }

    // ---- マルチENV変化幅のノブ表示 ----
    const float lift = juce::jlimit(0.0f, 1.0f,
        autoMode ? prog : proc.apvts.getRawParameterValue("lift")->load());
    updateModBands(lift, prog);

    // ---- MIDIラーン ----
    const int events = proc.getNoteEventCount();
    if (armedParamId.isNotEmpty() && events != lastNoteEvents)
    {
        const int note = proc.getLastNote();
        if (note >= 0)
        {
            if (auto* prm = proc.apvts.getParameter(armedParamId))
                prm->setValueNotifyingHost(prm->convertTo0to1((float)note));
        }
        armedParamId.clear();
        armedButton = nullptr;
        refreshKeyButtons();
    }
    lastNoteEvents = events;

    // ---- 波形表示の更新検知 ----
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);
        const int mode = (int)proc.apvts.getRawParameterValue("osc" + n + "Wave")->load();
        const float pos = proc.apvts.getRawParameterValue("osc" + n + "Pos")->load();
        const auto path = proc.getCustomWavetablePath(i);

        if (mode != lastWaveMode[(size_t)i]
            || std::abs(pos - lastPos[(size_t)i]) > 0.002f
            || path != lastWtPath[(size_t)i])
        {
            refreshWaveDisplay(i);
        }
    }

    // キー表示は毎回更新 (オートメーション等の外部変更対応, 低コスト)
    if (armedParamId.isEmpty())
        refreshKeyButtons();
}

void MainPanel::updateModBands(float lift, float prog)
{
    const auto& curves = proc.getCurves();

    for (int i = 0; i < 3; ++i)
    {
        ModBand::update(oscLevel[(size_t)i].knob, prmOscLevel[(size_t)i],
                        curves.read(CurveStore::oscCurve(i, 1)), lift, prog,
                        [](float b, float bip) { return b + bip * 0.5f; });
        ModBand::update(oscDet[(size_t)i].knob, prmOscDet[(size_t)i],
                        curves.read(CurveStore::oscCurve(i, 2)), lift, prog,
                        [](float b, float bip) { return b + bip * 50.0f; });
        ModBand::update(oscSpread[(size_t)i].knob, prmOscSpread[(size_t)i],
                        curves.read(CurveStore::oscCurve(i, 3)), lift, prog,
                        [](float b, float bip) { return b + bip * 0.5f; });
    }

    ModBand::update(noiseLevel.knob, prmNoiseLevel,
                    curves.read(CurveStore::NoiseLevel), lift, prog,
                    [](float b, float bip) { return b + bip * 0.5f; });
    ModBand::update(noiseRes.knob, prmNoiseRes,
                    curves.read(CurveStore::NoiseRes), lift, prog,
                    [](float b, float bip) { return b + bip * 5.75f; });

    const float rangeOct = proc.apvts.getRawParameterValue("noiseRange")->load();
    ModBand::update(noisePitch.knob, prmNoisePitch,
                    curves.read(CurveStore::NoisePitch), lift, prog,
                    [rangeOct](float b, float bip) { return b * std::exp2(bip * rangeOct); });
}

void MainPanel::refreshWaveDisplay(int osc)
{
    const juce::String n(osc + 1);
    lastWaveMode[(size_t)osc] = (int)proc.apvts.getRawParameterValue("osc" + n + "Wave")->load();
    lastPos[(size_t)osc] = proc.apvts.getRawParameterValue("osc" + n + "Pos")->load();
    lastWtPath[(size_t)osc] = proc.getCustomWavetablePath(osc);
    waveDisp[(size_t)osc].refresh();
}

void MainPanel::refreshKeyButtons()
{
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);
        const int ks = (int)proc.apvts.getRawParameterValue("osc" + n + "KeyStart")->load();
        const int ke = (int)proc.apvts.getRawParameterValue("osc" + n + "KeyEnd")->load();

        if (armedButton != &keyStartBtn[(size_t)i])
            keyStartBtn[(size_t)i].setButtonText(
                "ST " + juce::MidiMessage::getMidiNoteName(ks, true, true, 3));
        if (armedButton != &keyEndBtn[(size_t)i])
            keyEndBtn[(size_t)i].setButtonText(
                "END " + juce::MidiMessage::getMidiNoteName(ke, true, true, 3));
    }
}

void MainPanel::armLearn(const juce::String& paramId, juce::TextButton& btn)
{
    if (armedParamId == paramId)
    {
        // 再クリックで解除
        armedParamId.clear();
        armedButton = nullptr;
        refreshKeyButtons();
        return;
    }
    armedParamId = paramId;
    armedButton = &btn;
    lastNoteEvents = proc.getNoteEventCount();
    refreshKeyButtons();
    btn.setButtonText("PRESS KEY..");
}

// ==========================================================
void MainPanel::setupKnob(KnobCell& c, const juce::String& text, const juce::String& paramId,
                          juce::Colour accent)
{
    c.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    c.knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 15);
    c.knob.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(c.knob);

    c.label.setText(text, juce::dontSendNotification);
    c.label.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    c.label.setColour(juce::Label::textColourId, LiftColors::textDim);
    c.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(c.label);

    sliderAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramId, c.knob));
}

void MainPanel::setupCombo(juce::ComboBox& box, const juce::String& paramId,
                           const juce::StringArray& items)
{
    box.addItemList(items, 1);
    addAndMakeVisible(box);
    comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, paramId, box));
}

void MainPanel::layoutKnobGrid(juce::Rectangle<int> area, KnobCell** cells, int count, int cols)
{
    const int rows = (count + cols - 1) / cols;
    const int cw = area.getWidth() / cols;
    const int ch = area.getHeight() / juce::jmax(1, rows);

    for (int i = 0; i < count; ++i)
    {
        const int cx = area.getX() + (i % cols) * cw;
        const int cy = area.getY() + (i / cols) * ch;
        juce::Rectangle<int> cell(cx, cy, cw, ch);

        cells[i]->label.setBounds(cell.removeFromTop(16));
        cells[i]->knob.setBounds(cell.reduced(2));
    }
}

// ==========================================================
void MainPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour(LiftColors::panel);
    g.fillRoundedRectangle(r.removeFromTop(150.0f).reduced(2.0f), 8.0f);
    g.fillRoundedRectangle(r.reduced(2.0f).withTrimmedTop(4.0f), 8.0f);

    const auto cols = getLocalBounds().withTrimmedTop(158);
    g.setColour(LiftColors::panelLine);
    for (int i = 1; i < 4; ++i)
    {
        const int x = cols.getX() + cols.getWidth() * i / 4;
        g.drawVerticalLine(x, (float)cols.getY() + 12.0f, (float)cols.getBottom() - 12.0f);
    }
}

void MainPanel::resized()
{
    auto r = getLocalBounds();

    // ---- 上段: グローバル ----
    auto top = r.removeFromTop(150).reduced(10, 6);

    {
        auto liftArea = top.removeFromLeft(140);
        liftCell.label.setBounds(liftArea.removeFromTop(18));
        liftCell.knob.setBounds(liftArea.reduced(2));
    }
    {
        auto barsArea = top.removeFromLeft(84).reduced(4, 0);
        barsLabel.setBounds(barsArea.removeFromTop(18));
        barsBox.setBounds(barsArea.removeFromTop(26).reduced(2, 0));
    }

    KnobCell* globals[3] = { &attackCell, &releaseCell, &masterCell };
    for (auto* c : globals)
    {
        auto cell = top.removeFromLeft(86);
        c->label.setBounds(cell.removeFromTop(18));
        c->knob.setBounds(cell.reduced(6));
    }

    auto right = top.reduced(8, 0);
    right.removeFromTop(14);
    progressStrip.setBounds(right.removeFromTop(32));
    right.removeFromTop(6);
    waveStrip.setBounds(right); // Progress下: ライザー波形 + WAVドラッグ

    // ---- 下段: OSC1-3 + NOISE の4列 ----
    r.removeFromTop(8);
    const auto columnsArea = r;
    const int colW = r.getWidth() / 4;

    for (int i = 0; i < 3; ++i)
    {
        auto col = r.removeFromLeft(colW).reduced(9, 8);

        // ヘッダー: OSCn (幅短縮) + S + M
        auto head = col.removeFromTop(24);
        oscOn[(size_t)i]->setBounds(head.removeFromLeft(head.getWidth() - 76));
        head.removeFromLeft(4);
        oscSolo[(size_t)i]->setBounds(head.removeFromLeft(34));
        head.removeFromLeft(4);
        oscMute[(size_t)i]->setBounds(head);

        col.removeFromTop(4);
        waveBox[(size_t)i].setBounds(col.removeFromTop(22));
        col.removeFromTop(4);
        waveDisp[(size_t)i].setBounds(col.removeFromTop(40));
        col.removeFromTop(4);

        auto browseRow = col.removeFromTop(20);
        browseBtn[(size_t)i].setBounds(browseRow.removeFromLeft(browseRow.getWidth() * 2 / 3));
        browseRow.removeFromLeft(4);
        rndBtn[(size_t)i].setBounds(browseRow);

        // キー設定 (最下段)
        auto keyRow = col.removeFromBottom(24);
        keyStartBtn[(size_t)i].setBounds(keyRow.removeFromLeft(keyRow.getWidth() / 2 - 2));
        keyRow.removeFromLeft(4);
        keyEndBtn[(size_t)i].setBounds(keyRow);
        col.removeFromBottom(4);

        col.removeFromTop(4);
        KnobCell* cells[6] = { &oscPos[(size_t)i], &oscLevel[(size_t)i],
                               &oscCoarse[(size_t)i], &oscUni[(size_t)i],
                               &oscDet[(size_t)i], &oscSpread[(size_t)i] };
        layoutKnobGrid(col, cells, 6, 2);
    }

    // ノイズ列
    {
        auto col = r.reduced(9, 8);
        auto head = col.removeFromTop(24);
        noiseTitle.setBounds(head.removeFromLeft(head.getWidth() - 76));
        head.removeFromLeft(4);
        noiseSolo->setBounds(head.removeFromLeft(34));
        head.removeFromLeft(4);
        noiseMute->setBounds(head);

        col.removeFromTop(4);
        noiseTypeBox.setBounds(col.removeFromTop(22));
        col.removeFromTop(8);

        KnobCell* cells[4] = { &noiseLevel, &noisePitch, &noiseRes, &noiseRange };
        layoutKnobGrid(col.removeFromTop(col.getHeight() * 3 / 5), cells, 4, 2);
    }

    // ブラウザ: 下段全体を覆うオーバーレイ
    browser.setBounds(columnsArea.reduced(4));
}
