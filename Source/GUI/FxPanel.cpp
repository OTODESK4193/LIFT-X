// ==========================================
// File: FxPanel.cpp
// ==========================================
#include "FxPanel.h"

const std::array<FxPanel::FxDef, 6>& FxPanel::defs()
{
    static const std::array<FxDef, 6> d = { {
        { "SAT",    2, { CurveStore::SatAmt,  CurveStore::SatDrive, 0 },
                       { "AMT", "DRIVE", "" } },
        { "CHORUS", 2, { CurveStore::ChoAmt,  CurveStore::ChoDepth, 0 },
                       { "AMT", "DEPTH", "" } },
        { "DELAY",  3, { CurveStore::DlyAmt,  CurveStore::DlyFb, CurveStore::DlyTime },
                       { "AMT", "FB", "TIME" } },
        { "REVERB", 2, { CurveStore::RevAmt,  CurveStore::RevShimmer, 0 },
                       { "AMT", "SHIMMER", "" } },
        { "DUCK",   3, { CurveStore::DuckAmt, CurveStore::DuckRate, CurveStore::DuckShape },
                       { "AMT", "RATE", "SHAPE" } },
        { "STUTTER",2, { CurveStore::StutAmt, CurveStore::StutRate, 0 },
                       { "AMT", "RATE", "" } },
    } };
    return d;
}

FxPanel::FxPanel(LiftXAudioProcessor& p)
    : proc(p)
{
    // ---- 5スロット (適用順序) ----
    chainLabel.setText("CHAIN:", juce::dontSendNotification);
    chainLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    chainLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
    addAndMakeVisible(chainLabel);

    for (int s = 0; s < FxChain::kNumSlots; ++s)
    {
        slotType[(size_t)s].addItemList(FxChain::getTypeNames(), 1);

        // 他のスロットですでに使用されているFXタイプは非活性化 (Granular準拠・重複禁止)
        slotType[(size_t)s].setItemEnabledPredicate([this, s](int itemId) -> bool
        {
            const int fxType = itemId - 1; // 1-based (addItemList starts at 1) to 0-based FxType
            if (fxType <= 0) return true;  // None (0) は常にどのスロットでも選択可能

            // 自分のスロットで現在選択中のタイプなら選択可能
            auto* myParam = proc.apvts.getRawParameterValue("fx" + juce::String(s + 1) + "Type");
            const int myType = (myParam != nullptr) ? (int)myParam->load() : 0;
            if (fxType == myType) return true;

            // 他のスロットで既に選択されているFXタイプは非活性化 (重複禁止)
            for (int other = 0; other < FxChain::kNumSlots; ++other)
            {
                if (other == s) continue;
                auto* p = proc.apvts.getRawParameterValue("fx" + juce::String(other + 1) + "Type");
                if (p != nullptr && (int)p->load() == fxType)
                    return false; // 他のスロットで使用中のため非活性化
            }
            return true;
        });

        addAndMakeVisible(slotType[(size_t)s]);
        comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.apvts, "fx" + juce::String(s + 1) + "Type", slotType[(size_t)s]));
    }

    // ---- FXサブタブ / カーブサブタブ ----
    for (int i = 0; i < 6; ++i)
    {
        fxTabs[(size_t)i] = std::make_unique<juce::TextButton>(defs()[(size_t)i].name);
        fxTabs[(size_t)i]->onClick = [this, i] { setFx(i); };
        addAndMakeVisible(*fxTabs[(size_t)i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        curveTabs[(size_t)i] = std::make_unique<juce::TextButton>("");
        curveTabs[(size_t)i]->onClick = [this, i] { setCurve(i); };
        addAndMakeVisible(*curveTabs[(size_t)i]);
    }

    addAndMakeVisible(editor);
    editor.setBipolar(true);
    editor.setProgressProvider([this] { return proc.getEnvPosition(); });
    editor.onChanged = [this](const CurveSnapshot& s)
    {
        const auto& d = defs()[(size_t)activeFx];
        proc.getCurves().publish(d.curveIdx[juce::jlimit(0, d.numCurves - 1, activeCurve)], s);
    };

    hint.setFont(juce::Font(juce::FontOptions(12.0f)));
    hint.setColour(juce::Label::textColourId, LiftColors::textDim);
    hint.setJustificationType(juce::Justification::centredLeft);
    hint.setText("Center = knob value / Top = max / Bottom = min   "
                 "TIME & RATE: +-2 octaves (lower = faster)   "
                 "ROUTE: sources switched off bypass this effect entirely",
                 juce::dontSendNotification);
    addAndMakeVisible(hint);

    // ---- 詳細コントロール ----
    mkKnob(satAmt, "AMT", "satAmt", LiftColors::IdRose);
    mkCombo(satAlgoBox, satAlgoLabel, "ALGO", "satAlgo", FxChain::getSatAlgoNames());
    mkKnob(satDrive, "DRIVE", "satDrive", LiftColors::IdRose);
    mkKnob(satPre, "PRE HPF", "satPre", LiftColors::IdRose);
    mkKnob(satTrim, "TRIM", "satTrim", LiftColors::IdRose);

    mkKnob(choAmt, "AMT", "choAmt", LiftColors::IdMint);
    mkKnob(choRate, "RATE", "choRate", LiftColors::IdMint);
    mkKnob(choDepth, "DEPTH", "choDepth", LiftColors::IdMint);
    mkKnob(choWidth, "WIDTH", "choWidth", LiftColors::IdMint);

    mkKnob(dlyAmt, "AMT", "dlyAmt", LiftColors::IdBabyBlue);
    mkCombo(dlyTimeBox, dlyTimeLabel, "TIME", "dlyTime", FxChain::getDelayTimeNames());
    mkKnob(dlyFb, "FB", "dlyFb", LiftColors::IdBabyBlue);
    mkKnob(dlyDuck, "DUCK", "dlyDuck", LiftColors::IdBabyBlue);
    mkKnob(dlyDamp, "DAMP", "dlyDamp", LiftColors::IdBabyBlue);

    mkKnob(revAmt, "AMT", "revAmt", LiftColors::IdLavender);
    mkKnob(revDecay, "DECAY", "revDecay", LiftColors::IdLavender);
    mkKnob(revShimmer, "SHIMMER", "revShimmer", LiftColors::IdLavender);
    mkKnob(revDamp, "DAMP", "revDamp", LiftColors::IdLavender);
    mkKnob(revMod, "MOD", "revMod", LiftColors::IdLavender);

    mkKnob(duckAmt, "AMT", "duckAmt", LiftColors::IdPeach);
    mkCombo(duckRateBox, duckRateLabel, "RATE", "duckRate", FxChain::getDuckRateNames());
    mkKnob(duckShape, "SHAPE", "duckShape", LiftColors::IdPeach);

    mkKnob(stutAmt, "AMT", "stutAmt", LiftColors::IdSage);
    mkCombo(stutRateBox, stutRateLabel, "RATE", "stutRate", FxChain::getStutterRateNames());

    // TIME / RATE のカーブ変調による実効値表示
    for (auto* l : { &dlyTimeLive, &duckRateLive, &stutRateLive })
    {
        l->setFont(LiftFonts::mono(11.0f, true));
        l->setColour(juce::Label::textColourId, LiftColors::accentMaster);
        l->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*l);
    }

    // ---- ソース別ルーティング (FILTERタブと同じ操作感) ----
    //  OFFにしたソースは、選択中のエフェクトを完全にバイパスして素通しする。
    {
        static const char* srcBtnNames[RiserEngine::kNumSources] =
            { "OSC 1", "OSC 2", "OSC 3", "NOISE" };
        for (int s = 0; s < RiserEngine::kNumSources; ++s)
        {
            routeToggles[(size_t)s] = std::make_unique<GlowToggle>(
                srcBtnNames[s], s == 3 ? LiftColors::lilac : LiftColors::accentOsc);
            addAndMakeVisible(*routeToggles[(size_t)s]);
        }
        routeLabel.setText("ROUTE:", juce::dontSendNotification);
        routeLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        routeLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
        addAndMakeVisible(routeLabel);
    }

    setFx(0);
    startTimerHz(30);
}

// マルチENV変化幅を表示中FXのノブへ動的表示 (バイポーラ加算式と同一規則)
void FxPanel::timerCallback()
{
    if (!isVisible()) return;

    // ENV評価位置: Auto=Progress / Manual=LIFTノブ
    const float envPos = proc.getEnvPosition();

    const auto& curves = proc.getCurves();
    auto upd = [&](Cell& c, const char* paramId, int curveIdx, float halfRange)
    {
        ModBand::update(c.knob, proc.apvts.getParameter(paramId),
                        curves.read(curveIdx), 1.0f, envPos,
                        [halfRange](float b, float bip) { return b + bip * halfRange; });
    };

    // フルレンジ加算 (DSPと同一スケール)
    switch (activeFx)
    {
    case 0:
        upd(satAmt, "satAmt", CurveStore::SatAmt, 1.0f);
        upd(satDrive, "satDrive", CurveStore::SatDrive, 11.0f);
        break;
    case 1:
        upd(choAmt, "choAmt", CurveStore::ChoAmt, 1.0f);
        upd(choDepth, "choDepth", CurveStore::ChoDepth, 1.0f);
        break;
    case 2:
        upd(dlyAmt, "dlyAmt", CurveStore::DlyAmt, 1.0f);
        upd(dlyFb, "dlyFb", CurveStore::DlyFb, 0.95f);
        updateDelayTimeLive(envPos);
        break;
    case 3:
        upd(revAmt, "revAmt", CurveStore::RevAmt, 1.0f);
        upd(revShimmer, "revShimmer", CurveStore::RevShimmer, 1.0f);
        break;
    case 4:
        upd(duckAmt, "duckAmt", CurveStore::DuckAmt, 1.0f);
        upd(duckShape, "duckShape", CurveStore::DuckShape, 7.5f);
        updateDuckRateLive(envPos);
        break;
    default:
        upd(stutAmt, "stutAmt", CurveStore::StutAmt, 1.0f);
        updateStutterRateLive(envPos);
        break;
    }
}

// ==========================================================
// TIME / RATE の「カーブ適用後の実効値」表示
//  どちらもコンボボックスなのでノブの変調帯が使えず、カーブを描いても
//  どれだけ動いているかが画面上で全く分からなかった。
// ==========================================================
void FxPanel::updateDelayTimeLive(float envPos)
{
    const float bip = (proc.getCurves().read(CurveStore::DlyTime).evaluate(envPos) - 0.5f) * 2.0f;
    const float mult = std::exp2(bip * 2.0f);   // DSPと同一: ±2オクターブ
    const float beats = FxChain::delayTimeToBeats(
        (int)proc.apvts.getRawParameterValue("dlyTime")->load()) * mult;

    const double bpm = juce::jlimit(20.0, 999.0, proc.getLastBpm());
    const int ms = (int)std::round(beats * 60000.0 / bpm);

    juce::String t;
    if (std::abs(mult - 1.0f) < 0.005f) t = juce::String(ms) + "ms";
    else t = juce::String::formatted("x%.2f  %dms", mult, ms);

    if (dlyTimeLive.getText() != t)
        dlyTimeLive.setText(t, juce::dontSendNotification);
}

void FxPanel::updateStutterRateLive(float envPos)
{
    const float bip = (proc.getCurves().read(CurveStore::StutRate).evaluate(envPos) - 0.5f) * 2.0f;
    const float mult = std::exp2((float)juce::roundToInt(bip * 2.0f));
    const int baseIdx = (int)proc.apvts.getRawParameterValue("stutRate")->load();
    const float beats = FxChain::stutterRateToBeats(baseIdx) * mult;

    const auto names = FxChain::getStutterRateNames();
    int best = 0; float bestErr = 1.0e9f;
    for (int i = 0; i < names.size(); ++i)
    {
        const float e = std::abs(FxChain::stutterRateToBeats(i) - beats);
        if (e < bestErr) { bestErr = e; best = i; }
    }
    juce::String t = (bestErr < 1.0e-3f) ? names[best] : juce::String(beats, 3) + " beat";
    if (best != baseIdx || bestErr >= 1.0e-3f)
        t = names[baseIdx] + "  >  " + t;

    if (stutRateLive.getText() != t)
        stutRateLive.setText(t, juce::dontSendNotification);
}

void FxPanel::updateDuckRateLive(float envPos)
{
    const float bip = (proc.getCurves().read(CurveStore::DuckRate).evaluate(envPos) - 0.5f) * 2.0f;
    // DSPと同一: ±2オクターブを整数段へ量子化 (x4 .. x1/4)
    const float mult = std::exp2((float)juce::roundToInt(bip * 2.0f));
    const int baseIdx = (int)proc.apvts.getRawParameterValue("duckRate")->load();
    const float beats = FxChain::duckRateToBeats(baseIdx) * mult;

    // 実効拍数に最も近い表の名前を出す (音楽的に読みやすい)
    const auto names = FxChain::getDuckRateNames();
    int best = 0; float bestErr = 1.0e9f;
    for (int i = 0; i < names.size(); ++i)
    {
        const float e = std::abs(FxChain::duckRateToBeats(i) - beats);
        if (e < bestErr) { bestErr = e; best = i; }
    }

    juce::String t = (bestErr < 1.0e-3f) ? names[best]
                                         : juce::String(beats, 3) + " beat";
    if (best != baseIdx || bestErr >= 1.0e-3f)
        t = names[baseIdx] + "  >  " + t;

    if (duckRateLive.getText() != t)
        duckRateLive.setText(t, juce::dontSendNotification);
}

// ==========================================================
std::vector<juce::Component*> FxPanel::componentsFor(int fx)
{
    switch (fx)
    {
    case 0: return { &satAmt.knob, &satAmt.label, &satAlgoBox, &satAlgoLabel,
                     &satDrive.knob, &satDrive.label, &satPre.knob, &satPre.label,
                     &satTrim.knob, &satTrim.label };
    case 1: return { &choAmt.knob, &choAmt.label, &choRate.knob, &choRate.label,
                     &choDepth.knob, &choDepth.label, &choWidth.knob, &choWidth.label };
    case 2: return { &dlyAmt.knob, &dlyAmt.label, &dlyTimeBox, &dlyTimeLabel, &dlyTimeLive,
                     &dlyFb.knob, &dlyFb.label, &dlyDuck.knob, &dlyDuck.label,
                     &dlyDamp.knob, &dlyDamp.label };
    case 3: return { &revAmt.knob, &revAmt.label, &revDecay.knob, &revDecay.label,
                     &revShimmer.knob, &revShimmer.label, &revDamp.knob, &revDamp.label,
                     &revMod.knob, &revMod.label };
    case 4: return { &duckAmt.knob, &duckAmt.label, &duckRateBox, &duckRateLabel, &duckRateLive,
                     &duckShape.knob, &duckShape.label };
    default: return { &stutAmt.knob, &stutAmt.label, &stutRateBox, &stutRateLabel, &stutRateLive };
    }
}

// 選択中エフェクトのルーティングパラメーターへアタッチし直す
void FxPanel::rebuildRouteAttachments()
{
    static const char* fxPrefix[6] = { "sat", "cho", "dly", "rev", "duck", "stut" };
    static const char* srcIds[RiserEngine::kNumSources] = { "Osc1", "Osc2", "Osc3", "Noise" };

    for (int s = 0; s < RiserEngine::kNumSources; ++s)
    {
        routeAtts[(size_t)s].reset();   // 先に解除してから張り替える
        routeAtts[(size_t)s] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts,
            juce::String(fxPrefix[juce::jlimit(0, 5, activeFx)]) + "Route" + srcIds[s],
            *routeToggles[(size_t)s]);
    }
}

void FxPanel::setFx(int idx)
{
    activeFx = juce::jlimit(0, 5, idx);

    for (int f = 0; f < 6; ++f)
        for (auto* c : componentsFor(f))
            c->setVisible(f == activeFx);

    rebuildRouteAttachments();

    const auto& d = defs()[(size_t)activeFx];
    for (int i = 0; i < 3; ++i)
    {
        curveTabs[(size_t)i]->setVisible(i < d.numCurves);
        if (i < d.numCurves)
            curveTabs[(size_t)i]->setButtonText(juce::String("ENV: ") + d.curveNames[i]);
    }

    activeCurve = 0;
    setCurve(0);
    resized();
}

void FxPanel::setCurve(int idx)
{
    const auto& d = defs()[(size_t)activeFx];
    activeCurve = juce::jlimit(0, d.numCurves - 1, idx);
    const int ci = d.curveIdx[activeCurve];

    editor.setSnapshot(proc.getCurves().get(ci));
    editor.setAccent(LiftColors::curveAccent(ci));
    editor.setTitle(CurveStore::name(ci));

    for (int i = 0; i < 6; ++i)
        styleTabButton(*fxTabs[(size_t)i], i == activeFx, LiftColors::accentFx);
    for (int i = 0; i < 3; ++i)
        styleTabButton(*curveTabs[(size_t)i], i == activeCurve, LiftColors::curveAccent(ci));
}

void FxPanel::styleTabButton(juce::TextButton& b, bool active, juce::Colour accent)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? accent.withAlpha(0.22f) : LiftColors::knobTrack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? LiftColors::text : LiftColors::textDim);
    b.repaint();
}

// ==========================================================
void FxPanel::mkKnob(Cell& c, const juce::String& text, const juce::String& paramId, int accentId)
{
    c.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    c.knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 15);
    c.knob.setColour(juce::Slider::rotarySliderFillColourId, LiftColors::accentById(accentId));
    c.knob.getProperties().set("accentId", accentId); // テーマ連動
    addAndMakeVisible(c.knob);

    c.label.setText(text, juce::dontSendNotification);
    c.label.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    c.label.setColour(juce::Label::textColourId, LiftColors::textDim);
    c.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(c.label);

    sliderAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramId, c.knob));
}

void FxPanel::mkCombo(juce::ComboBox& box, juce::Label& label, const juce::String& text,
                      const juce::String& paramId, const juce::StringArray& items)
{
    box.addItemList(items, 1);
    addAndMakeVisible(box);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, LiftColors::textDim);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, paramId, box));
}

void FxPanel::layoutCell(juce::Rectangle<int> area, Cell& c)
{
    c.label.setBounds(area.removeFromTop(16));
    c.knob.setBounds(area.reduced(3));
}

void FxPanel::layoutDetailGrid(juce::Rectangle<int> area, std::vector<Cell*> cells,
                               juce::ComboBox* combo, juce::Label* comboLabel,
                               juce::Label* liveLabel)
{
    if (combo != nullptr && comboLabel != nullptr)
    {
        auto row = area.removeFromTop(44);
        comboLabel->setBounds(row.removeFromLeft(52));
        combo->setBounds(row.removeFromTop(24));

        // コンボの下に「カーブ適用後の実効値」を small text で出す
        if (liveLabel != nullptr)
            liveLabel->setBounds(row.removeFromTop(16));

        area.removeFromTop(4);
    }

    const int cols = 2;
    const int rows = ((int)cells.size() + cols - 1) / cols;
    const int cw = area.getWidth() / cols;
    const int ch = juce::jmin(104, area.getHeight() / juce::jmax(1, rows));

    for (int i = 0; i < (int)cells.size(); ++i)
    {
        juce::Rectangle<int> cell(area.getX() + (i % cols) * cw,
                                  area.getY() + (i / cols) * ch, cw, ch);
        layoutCell(cell, *cells[(size_t)i]);
    }
}

// ==========================================================
void FxPanel::paint(juce::Graphics& g)
{
    LiftColors::paintPanel(g, getLocalBounds().toFloat().reduced(2.0f));

    // 詳細エリアの枠
    if (!detailArea.isEmpty())
    {
        g.setColour(LiftColors::panelLine);
        g.drawRoundedRectangle(detailArea.toFloat(), 6.0f, 1.0f);
    }
}

void FxPanel::resized()
{
    auto r = getLocalBounds().reduced(12, 10);

    // ---- スロット行 ----
    auto chainRow = r.removeFromTop(24);
    chainLabel.setBounds(chainRow.removeFromLeft(60));
    const int slotW = chainRow.getWidth() / FxChain::kNumSlots;
    for (int s = 0; s < FxChain::kNumSlots; ++s)
        slotType[(size_t)s].setBounds(chainRow.removeFromLeft(slotW).reduced(3, 0));

    r.removeFromTop(8);

    // ---- FXサブタブ行 ----
    auto tabRow = r.removeFromTop(28);
    for (int i = 0; i < 6; ++i)
    {
        fxTabs[(size_t)i]->setBounds(tabRow.removeFromLeft(92));
        tabRow.removeFromLeft(6);
    }

    r.removeFromTop(8);

    // ---- ソース別ルーティング行 (選択中のエフェクトに対して働く) ----
    {
        auto routeRow = r.removeFromTop(24);
        routeLabel.setBounds(routeRow.removeFromLeft(64));
        for (int s = 0; s < RiserEngine::kNumSources; ++s)
        {
            routeToggles[(size_t)s]->setBounds(routeRow.removeFromLeft(96));
            routeRow.removeFromLeft(6);
        }
    }

    r.removeFromTop(8);
    hint.setBounds(r.removeFromBottom(20));
    r.removeFromBottom(4);

    // ---- 左: 詳細 / 右: カーブ ----
    detailArea = r.removeFromLeft(300);
    auto inner = detailArea.reduced(10, 8);

    switch (activeFx)
    {
    case 0: layoutDetailGrid(inner, { &satAmt, &satDrive, &satPre, &satTrim },
                             &satAlgoBox, &satAlgoLabel); break;
    case 1: layoutDetailGrid(inner, { &choAmt, &choRate, &choDepth, &choWidth },
                             nullptr, nullptr); break;
    case 2: layoutDetailGrid(inner, { &dlyAmt, &dlyFb, &dlyDuck, &dlyDamp },
                             &dlyTimeBox, &dlyTimeLabel, &dlyTimeLive); break;
    case 3: layoutDetailGrid(inner, { &revAmt, &revDecay, &revShimmer, &revDamp, &revMod },
                             nullptr, nullptr); break;
    case 4: layoutDetailGrid(inner, { &duckAmt, &duckShape },
                             &duckRateBox, &duckRateLabel, &duckRateLive); break;
    default: layoutDetailGrid(inner, { &stutAmt },
                              &stutRateBox, &stutRateLabel, &stutRateLive); break;
    }

    r.removeFromLeft(8);

    // カーブサブタブ + エディタ
    auto curveTabRow = r.removeFromTop(24);
    for (int i = 0; i < 3; ++i)
    {
        curveTabs[(size_t)i]->setBounds(curveTabRow.removeFromLeft(110));
        curveTabRow.removeFromLeft(6);
    }
    r.removeFromTop(4);
    editor.setBounds(r);
}
