// ==========================================
// File: MainPanel.cpp
// ==========================================
#include "MainPanel.h"

MainPanel::MainPanel(LiftXAudioProcessor& p)
    : proc(p), progressStrip(p), waveStrip(p), browser(p)
{
    // ---- グローバル ----
    setupKnob(liftCell, "LIFT", "lift", LiftColors::IdPink);
    setupKnob(attackCell, "ATTACK", "attack", LiftColors::IdPink);
    setupKnob(releaseCell, "RELEASE", "release", LiftColors::IdPink);

    barsLabel.setText("BARS", juce::dontSendNotification);
    barsLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    barsLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
    barsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(barsLabel);
    setupCombo(barsBox, "bars", LiftXAudioProcessor::getBarsNames());

    // ---- REVERSE: ENV評価位置を反転 (ライザー↔ダウナー) ----
    reverseButton = std::make_unique<GlowToggle>("REVERSE", LiftColors::peach);
    addAndMakeVisible(*reverseButton);
    buttonAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "reverse", *reverseButton));

    // ---- RANDOM: MAIN/OSC ENVを音楽的な範囲でランダマイズ ----
    auto afterRandomise = [this]
    {
        refreshKeyButtons();
        for (int i = 0; i < 3; ++i)
            refreshWaveDisplay(i);
        // カーブが入れ替わるので、他タブのエディタも読み直させる
        if (auto* top = getParentComponent())
            top->postCommandMessage(0);
    };

    randomButton.setButtonText("RANDOM");
    randomButton.setTooltip("Randomise MAIN + OSC ENV within musically safe ranges");
    addAndMakeVisible(randomButton);
    randomButton.onClick = [this, afterRandomise]
    {
        proc.randomizeMainAndOsc(lockOscBtn->getToggleState(),
                                 lockCurveBtn->getToggleState());
        afterRandomise();
    };

    // MUTATE: 気に入った音を壊さずに近傍を探索する
    mutateButton.setButtonText("MUTATE");
    mutateButton.setTooltip("Nudge the current sound slightly (hold Shift for a bigger jump)");
    addAndMakeVisible(mutateButton);
    mutateButton.onClick = [this, afterRandomise]
    {
        const bool big = juce::ModifierKeys::getCurrentModifiers().isShiftDown();
        proc.mutateMainAndOsc(big ? 0.35f : 0.12f);
        afterRandomise();
    };

    // ---- BARS ロック ----
    //  曲の尺に合わせて BARS を決めたあと、音色だけをプリセットで探したい、
    //  という使い方のためのもの。ON のあいだは RANDOM でもプリセット読み込みでも
    //  小節数が変わらない。状態はグローバル設定へ永続化する。
    lockBarsBtn.setClickingTogglesState(true);
    lockBarsBtn.setTooltip("Lock BARS - keeps the bar length when loading presets and on RANDOM");
    lockBarsBtn.setColour(juce::TextButton::buttonOnColourId,
                          LiftColors::peach.withAlpha(0.45f));
    {
        const bool locked = LiftXAudioProcessor::getGlobalSettings()
                                .getBoolValue("lockBars", false);
        lockBarsBtn.setToggleState(locked, juce::dontSendNotification);
        proc.setBarsLocked(locked);
    }
    lockBarsBtn.onClick = [this]
    {
        const bool locked = lockBarsBtn.getToggleState();
        proc.setBarsLocked(locked);
        auto& gs = LiftXAudioProcessor::getGlobalSettings();
        gs.setValue("lockBars", locked);
        gs.saveIfNeeded();
    };
    addAndMakeVisible(lockBarsBtn);

    // ロックトグル (RANDOM の対象から外す)
    lockOscBtn   = std::make_unique<GlowToggle>("LOCK OSC",  LiftColors::peach);
    lockCurveBtn = std::make_unique<GlowToggle>("LOCK ENV",  LiftColors::peach);
    lockOscBtn->setTooltip("Keep oscillator settings when pressing RANDOM");
    lockCurveBtn->setTooltip("Keep envelope curves when pressing RANDOM");
    addAndMakeVisible(*lockOscBtn);
    addAndMakeVisible(*lockCurveBtn);

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

        setupKnob(oscPos[(size_t)i], "POS", "osc" + n + "Pos", LiftColors::IdMint);
        setupKnob(oscLevel[(size_t)i], "LEVEL", "osc" + n + "Level", LiftColors::IdMint);
        setupKnob(oscCoarse[(size_t)i], "COARSE", "osc" + n + "Coarse", LiftColors::IdMint);
        setupKnob(oscFine[(size_t)i], "FINE", "osc" + n + "Fine", LiftColors::IdMint);
        setupKnob(oscUni[(size_t)i], "UNISON", "osc" + n + "Uni", LiftColors::IdMint);
        setupKnob(oscDet[(size_t)i], "DETUNE", "osc" + n + "Det", LiftColors::IdMint);
        setupKnob(oscSpread[(size_t)i], "SPREAD", "osc" + n + "Spread", LiftColors::IdMint);
        setupKnob(oscPan[(size_t)i], "PAN", "osc" + n + "Pan", LiftColors::IdMint);

        pitchRail[(size_t)i] = std::make_unique<PitchRail>(proc, i);
        addAndMakeVisible(*pitchRail[(size_t)i]);

        addAndMakeVisible(keyStartBtn[(size_t)i]);
        addAndMakeVisible(keyEndBtn[(size_t)i]);

        // SWAP: START と END を入れ替える。
        //  REVERSE はカーブ全体 (フィルター/FX含む) が逆再生になるのに対し、
        //  こちらはピッチの向きだけを反転させたい場合に使う。
        keySwapBtn[(size_t)i].setButtonText("SWAP");
        keySwapBtn[(size_t)i].setTooltip("Swap START and END keys (pitch direction only)");
        addAndMakeVisible(keySwapBtn[(size_t)i]);
        keySwapBtn[(size_t)i].onClick = [this, i]
        {
            const juce::String nn(i + 1);
            auto* ps = proc.apvts.getParameter("osc" + nn + "KeyStart");
            auto* pe = proc.apvts.getParameter("osc" + nn + "KeyEnd");
            if (ps == nullptr || pe == nullptr) return;
            const float a = ps->getValue(), b = pe->getValue();
            ps->setValueNotifyingHost(b);
            pe->setValueNotifyingHost(a);
            refreshKeyButtons();
        };
        keyStartBtn[(size_t)i].onClick = [this, i]
        { armLearn("osc" + juce::String(i + 1) + "KeyStart", keyStartBtn[(size_t)i]); };
        keyEndBtn[(size_t)i].onClick = [this, i]
        { armLearn("osc" + juce::String(i + 1) + "KeyEnd", keyEndBtn[(size_t)i]); };

        // 波形表示ソース (リアルタイム実効Position対応)
        waveDisp[(size_t)i].setSource([this, i](float* out, int nPts)
        {
            const int mode = (int)proc.apvts.getRawParameterValue(
                "osc" + juce::String(i + 1) + "Wave")->load();
            const bool useCustom = (mode == RiserEngine::CustomWT);
            const float morph = useCustom ? getEffectivePos(i) : (float)mode * 0.25f;
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
    setupKnob(noiseLevel, "LEVEL", "noiseLevel", LiftColors::IdLilac);
    setupKnob(noisePitch, "PITCH", "noisePitch", LiftColors::IdLilac);
    setupKnob(noiseRes, "RES", "noiseRes", LiftColors::IdLilac);
    setupKnob(noiseRange, "RANGE", "noiseRange", LiftColors::IdLilac);
    setupKnob(noisePan, "PAN", "noisePan", LiftColors::IdLilac);

    // ---- マスターエリア (ノイズ列の下): OUT + Limiter CEILING ----
    masterTitle.setText("MASTER", juce::dontSendNotification);
    masterTitle.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    masterTitle.setColour(juce::Label::textColourId, LiftColors::accentMaster);
    addAndMakeVisible(masterTitle);
    setupKnob(masterCell, "OUT", "master", LiftColors::IdPink);
    setupKnob(ceilCell, "CEILING", "limCeiling", LiftColors::IdPink);

    panicBtn.setTooltip("Stop all sound, reset oscillators, and clear all FX buffers immediately");
    panicBtn.onClick = [this] { proc.triggerPanic(); };
    addAndMakeVisible(panicBtn);

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
        prmOscPos[(size_t)i]   = proc.apvts.getParameter("osc" + n + "Pos");
        prmOscLevel[(size_t)i] = proc.apvts.getParameter("osc" + n + "Level");
        prmOscDet[(size_t)i]   = proc.apvts.getParameter("osc" + n + "Det");
        prmOscSpread[(size_t)i] = proc.apvts.getParameter("osc" + n + "Spread");
        prmOscPan[(size_t)i]   = proc.apvts.getParameter("osc" + n + "Pan");
    }
    prmNoiseLevel = proc.apvts.getParameter("noiseLevel");
    prmNoiseRes = proc.apvts.getParameter("noiseRes");
    prmNoisePitch = proc.apvts.getParameter("noisePitch");
    prmNoisePan = proc.apvts.getParameter("noisePan");

    refreshKeyButtons();
    for (int i = 0; i < 3; ++i)
        refreshWaveDisplay(i);

    startTimerHz(30);
}

// ==========================================================
void MainPanel::timerCallback()
{
    // ---- LIFT Auto: ノブをENV評価位置へ追従 ----
    //  REVERSE時は評価位置が 1→0 と進むため、ノブもプレイヘッドと同じ向きに動く
    //  (PROGRESSバーは時間軸なので常に 0→100% のまま)
    const bool autoMode = proc.apvts.getRawParameterValue("liftMode")->load() > 0.5f;
    if (autoMode)
    {
        if (liftCell.knob.isEnabled())
            liftCell.knob.setEnabled(false);
        liftCell.knob.setValue(proc.getEnvPosition(), juce::dontSendNotification);
    }
    else if (!liftCell.knob.isEnabled())
    {
        liftCell.knob.setEnabled(true);
        liftCell.knob.setValue(proc.apvts.getRawParameterValue("lift")->load(),
                               juce::dontSendNotification);
    }

    // ---- マルチENV変化幅のノブ表示 (評価位置: Auto=Progress / Manual=LIFTノブ) ----
    updateModBands(1.0f, proc.getEnvPosition());

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

    // ---- 波形表示の更新検知 (リアルタイム実効Position追従) ----
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);
        const int mode = (int)proc.apvts.getRawParameterValue("osc" + n + "Wave")->load();
        const float effPos = getEffectivePos(i);
        const auto path = proc.getCustomWavetablePath(i);

        if (mode != lastWaveMode[(size_t)i]
            || std::abs(effPos - lastPos[(size_t)i]) > 0.002f
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

    // フルレンジ加算 (中央=ノブ値 / 上端=MAX / 下端=MIN) — DSPと同一スケール
    for (int i = 0; i < 3; ++i)
    {
        ModBand::update(oscPos[(size_t)i].knob, prmOscPos[(size_t)i],
                        curves.read(CurveStore::posCurve(i)), lift, prog,
                        [](float b, float bip) { return juce::jlimit(0.0f, 1.0f, b + bip * 1.0f); });
        ModBand::update(oscLevel[(size_t)i].knob, prmOscLevel[(size_t)i],
                        curves.read(CurveStore::oscCurve(i, 1)), lift, prog,
                        [](float b, float bip) { return b + bip * 1.0f; });
        ModBand::update(oscDet[(size_t)i].knob, prmOscDet[(size_t)i],
                        curves.read(CurveStore::oscCurve(i, 2)), lift, prog,
                        [](float b, float bip) { return b + bip * 100.0f; });
        ModBand::update(oscSpread[(size_t)i].knob, prmOscSpread[(size_t)i],
                        curves.read(CurveStore::oscCurve(i, 3)), lift, prog,
                        [](float b, float bip) { return b + bip * 1.0f; });
        ModBand::update(oscPan[(size_t)i].knob, prmOscPan[(size_t)i],
                        curves.read(CurveStore::panCurve(i)), lift, prog,
                        [](float b, float bip) { return b + bip * 1.0f; });
    }

    ModBand::update(noiseLevel.knob, prmNoiseLevel,
                    curves.read(CurveStore::NoiseLevel), lift, prog,
                    [](float b, float bip) { return b + bip * 1.0f; });
    ModBand::update(noiseRes.knob, prmNoiseRes,
                    curves.read(CurveStore::NoiseRes), lift, prog,
                    [](float b, float bip) { return b + bip * 11.5f; });
    ModBand::update(noisePan.knob, prmNoisePan,
                    curves.read(CurveStore::panCurve(3)), lift, prog,
                    [](float b, float bip) { return b + bip * 1.0f; });

    // NOISE PITCH は DSP 側で 20Hz..sr*0.45 にクランプされるため、表示も合わせる
    const float rangeOct = proc.apvts.getRawParameterValue("noiseRange")->load();
    const float nyq = (float)(proc.getPreparedSampleRate() * 0.45);
    ModBand::update(noisePitch.knob, prmNoisePitch,
                    curves.read(CurveStore::NoisePitch), lift, prog,
                    [rangeOct, nyq](float b, float bip)
                    { return juce::jlimit(20.0f, nyq, b * std::exp2(bip * rangeOct)); });
}

float MainPanel::getEffectivePos(int osc) const
{
    const juce::String n(osc + 1);
    const float basePos = proc.apvts.getRawParameterValue("osc" + n + "Pos")->load();
    const float envPos = proc.getEnvPosition();
    const float posBip = (proc.getCurves().read(CurveStore::posCurve(osc)).evaluate(envPos) - 0.5f) * 2.0f;
    return juce::jlimit(0.0f, 1.0f, basePos + posBip * 1.0f);
}

void MainPanel::refreshWaveDisplay(int osc)
{
    const juce::String n(osc + 1);
    lastWaveMode[(size_t)osc] = (int)proc.apvts.getRawParameterValue("osc" + n + "Wave")->load();
    lastPos[(size_t)osc] = getEffectivePos(osc);
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
                          int accentId)
{
    c.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    c.knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 14);
    c.knob.setColour(juce::Slider::rotarySliderFillColourId, LiftColors::accentById(accentId));
    c.knob.getProperties().set("accentId", accentId); // テーマ変更へライブ連動
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
        if (cells[i] == nullptr) continue;   // 空きセル (奇数個のグリッド用)

        const int cx = area.getX() + (i % cols) * cw;
        const int cy = area.getY() + (i / cols) * ch;
        juce::Rectangle<int> cell(cx, cy, cw, ch);

        cells[i]->label.setBounds(cell.removeFromTop(14));
        cells[i]->knob.setBounds(cell.reduced(2));
    }
}

// ==========================================================
void MainPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    LiftColors::paintPanel(g, r.removeFromTop(132.0f).reduced(2.0f));
    LiftColors::paintPanel(g, r.reduced(2.0f).withTrimmedTop(4.0f));

    const auto cols = getLocalBounds().withTrimmedTop(140);
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
    auto top = r.removeFromTop(132).reduced(10, 6);

    {
        auto liftArea = top.removeFromLeft(140);
        liftCell.label.setBounds(liftArea.removeFromTop(18));
        liftCell.knob.setBounds(liftArea.reduced(2));
    }
    {
        // ---- BARS / RANDOM 系: 2列 x 3行のグリッド ----
        //   [BARS combo] [MUTATE  ]
        //   [REVERSE   ] [LOCK OSC]
        //   [RANDOM    ] [LOCK ENV]
        //  両列とも BARS コンボと同じ幅に揃える。GlowToggle は左端に LED を
        //  描くぶんテキスト領域が狭くなるが、この幅なら "LOCK OSC" も収まる。
        constexpr int kColW = 112;   // = BARS コンボの幅
        constexpr int kGap  = 8;

        auto barsArea = top.removeFromLeft(kColW * 2 + kGap + 8).reduced(4, 0);
        auto colA = barsArea.removeFromLeft(kColW);
        barsArea.removeFromLeft(kGap);
        auto colB = barsArea.removeFromLeft(kColW);

        // 1行目だけ左列にラベルが乗るため、右列も同じ高さぶん下げて行を揃える
        barsLabel.setBounds(colA.removeFromTop(16));
        colB.removeFromTop(16);

        {
            auto barsRow = colA.removeFromTop(26);
            lockBarsBtn.setBounds(barsRow.removeFromRight(barsRow.getWidth() / 3));
            barsRow.removeFromRight(4);
            barsBox.setBounds(barsRow);
        }
        mutateButton.setBounds(colB.removeFromTop(26));

        colA.removeFromTop(6);
        colB.removeFromTop(6);
        reverseButton->setBounds(colA.removeFromTop(24));
        lockOscBtn->setBounds(colB.removeFromTop(24));

        colA.removeFromTop(6);
        colB.removeFromTop(6);
        randomButton.setBounds(colA.removeFromTop(24));
        lockCurveBtn->setBounds(colB.removeFromTop(24));
    }

    KnobCell* globals[2] = { &attackCell, &releaseCell };
    for (auto* c : globals)
    {
        auto cell = top.removeFromLeft(90);
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
        waveDisp[(size_t)i].setBounds(col.removeFromTop(34));
        col.removeFromTop(4);

        auto browseRow = col.removeFromTop(20);
        browseBtn[(size_t)i].setBounds(browseRow.removeFromLeft(browseRow.getWidth() * 2 / 3));
        browseRow.removeFromLeft(4);
        rndBtn[(size_t)i].setBounds(browseRow);

        // キー設定 (最下段) + その上に Pitch ENV ライブバー
        auto keyRow = col.removeFromBottom(24);
        keySwapBtn[(size_t)i].setBounds(keyRow.removeFromRight(46));
        keyRow.removeFromRight(4);
        keyStartBtn[(size_t)i].setBounds(keyRow.removeFromLeft(keyRow.getWidth() / 2 - 2));
        keyRow.removeFromLeft(4);
        keyEndBtn[(size_t)i].setBounds(keyRow);
        col.removeFromBottom(4);

        pitchRail[(size_t)i]->setBounds(col.removeFromBottom(18));
        col.removeFromBottom(4);

        col.removeFromTop(4);
        KnobCell* cells[9] = { &oscPos[(size_t)i],    &oscLevel[(size_t)i],  &oscCoarse[(size_t)i],
                               &oscFine[(size_t)i],   &oscUni[(size_t)i],    &oscDet[(size_t)i],
                               &oscSpread[(size_t)i], &oscPan[(size_t)i],    nullptr };
        layoutKnobGrid(col, cells, 9, 3);
    }

    // ノイズ列 + マスターエリア
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

        KnobCell* cells[6] = { &noiseLevel, &noisePitch, &noiseRes,
                               &noiseRange, &noisePan,   nullptr };
        layoutKnobGrid(col.removeFromTop(180), cells, 6, 3);

        // MASTER: OUT + CEILING
        col.removeFromTop(8);
        masterTitle.setBounds(col.removeFromTop(18));
        KnobCell* mcells[2] = { &masterCell, &ceilCell };
        layoutKnobGrid(col.removeFromTop(100), mcells, 2, 2);

        col.removeFromTop(6);
        panicBtn.setBounds(col.removeFromTop(24).reduced(6, 0));
    }

    // ブラウザ: 下段全体を覆うオーバーレイ
    browser.setBounds(columnsArea.reduced(4));
}
