// ==========================================
// File: OscEnvPanel.cpp
// ==========================================
#include "OscEnvPanel.h"

namespace
{
    const char* kSrcNames[4] = { "OSC 1", "OSC 2", "OSC 3", "NOISE" };
    const char* kOscTgtNames[5]   = { "PITCH", "LEVEL", "DETUNE", "SPREAD", "PAN" };
    const char* kNoiseTgtNames[4] = { "PITCH", "LEVEL", "RES", "PAN" };
}

OscEnvPanel::OscEnvPanel(LiftXAudioProcessor& p)
    : proc(p)
{
    for (int i = 0; i < 4; ++i)
    {
        srcTabs[(size_t)i] = std::make_unique<juce::TextButton>(kSrcNames[i]);
        srcTabs[(size_t)i]->onClick = [this, i] { setSource(i); };
        addAndMakeVisible(*srcTabs[(size_t)i]);
    }
    for (int i = 0; i < kNumTargets; ++i)
    {
        tgtTabs[(size_t)i] = std::make_unique<juce::TextButton>(kOscTgtNames[i]);
        tgtTabs[(size_t)i]->onClick = [this, i] { setTarget(i); };
        addAndMakeVisible(*tgtTabs[(size_t)i]);
    }

    addAndMakeVisible(editor);
    editor.setProgressProvider([this] { return proc.getEnvPosition(); });
    editor.onChanged = [this](const CurveSnapshot& s)
    {
        proc.getCurves().publish(curveIndex(), s);
    };

    // 「Paste to All」: 現在のターゲット (PITCH/LEVEL/...) を OSC1-3 全部へ複製する。
    //  和音ライザーを作るとき、OSC1 で描いた形を 2/3 へ手作業でコピーする必要が
    //  なくなる。ノイズは音の性質が違うため対象外。
    editor.onPasteToAll = [this](const CurveSnapshot& s)
    {
        if (activeSource >= 3) return;      // ノイズ選択中は何もしない
        for (int o = 0; o < RiserEngine::kNumOscs; ++o)
            proc.getCurves().publish(CurveStore::oscCurve(o, activeTarget), s);
    };

    hint.setFont(juce::Font(juce::FontOptions(12.0f)));
    hint.setColour(juce::Label::textColourId, LiftColors::textDim);
    hint.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hint);

    setSource(0);
}

int OscEnvPanel::curveIndex() const
{
    // PAN は OSC/ノイズ共通で専用インデックス
    if (activeTarget == kPanTarget)
        return CurveStore::panCurve(activeSource);
    if (activeSource < 3)
        return CurveStore::oscCurve(activeSource, activeTarget);
    return CurveStore::noiseCurve(juce::jmin(activeTarget, 2));
}

void OscEnvPanel::setSource(int idx)
{
    activeSource = juce::jlimit(0, 3, idx);

    // ターゲットタブの名称/数をソースに合わせて更新
    //  ノイズは DETUNE/SPREAD が無いので 3番目のタブを隠し、PAN だけ残す
    const bool isNoise = (activeSource == 3);
    for (int i = 0; i < kNumTargets; ++i)
    {
        const bool visible = isNoise ? (i < 3 || i == kPanTarget) : true;
        tgtTabs[(size_t)i]->setVisible(visible);
        if (isNoise)
            tgtTabs[(size_t)i]->setButtonText(i < 3 ? kNoiseTgtNames[i]
                                            : (i == kPanTarget ? "PAN" : ""));
        else
            tgtTabs[(size_t)i]->setButtonText(kOscTgtNames[i]);
    }
    if (isNoise && activeTarget == 3)
        activeTarget = 0;

    setTarget(activeTarget);
}

void OscEnvPanel::setTarget(int idx)
{
    const bool isNoise = (activeSource == 3);
    activeTarget = juce::jlimit(0, kNumTargets - 1, idx);
    if (isNoise && activeTarget == 3) activeTarget = 0;   // ノイズに DETUNE/SPREAD は無い

    const int ci = curveIndex();
    editor.setSnapshot(proc.getCurves().get(ci));
    editor.setAccent(LiftColors::curveAccent(ci));
    editor.setTitle(CurveStore::name(ci));

    // OSCピッチのみユニポーラ (下=StartKey / 上=EndKey)、他はバイポーラ (中央=ノブ値)
    editor.setBipolar(!(activeSource < 3 && activeTarget == 0));

    for (int i = 0; i < 4; ++i)
        styleTabButton(*srcTabs[(size_t)i], i == activeSource,
                       i < 3 ? LiftColors::accentPitch : LiftColors::lilac);
    for (int i = 0; i < kNumTargets; ++i)
        styleTabButton(*tgtTabs[(size_t)i], i == activeTarget, LiftColors::curveAccent(ci));

    updateHint();
}

void OscEnvPanel::updateHint()
{
    const bool isNoise = (activeSource == 3);
    juce::String t;
    if (!isNoise && activeTarget == 0)
        t = "Bottom = StartKey / Top = EndKey (set in MAIN) - MIDI note is trigger only";
    else if (isNoise && activeTarget == 0)
        t = "Center = PITCH knob / +-RANGE oct";
    else if (activeTarget == kPanTarget)
        t = "Center = centre / Top = hard right / Bottom = hard left (equal power)";
    else
        t = "Center = knob value / Top = max / Bottom = min (clamped)";

    t += "   Double-click: add/remove point   Drag diamond: tension";
    hint.setText(t, juce::dontSendNotification);
}

void OscEnvPanel::styleTabButton(juce::TextButton& b, bool active, juce::Colour accent)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? accent.withAlpha(0.22f) : LiftColors::knobTrack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? LiftColors::text : LiftColors::textDim);
    b.repaint();
}

void OscEnvPanel::paint(juce::Graphics& g)
{
    LiftColors::paintPanel(g, getLocalBounds().toFloat().reduced(2.0f));
}

void OscEnvPanel::resized()
{
    auto r = getLocalBounds().reduced(12, 10);

    // ソースタブ行
    auto top = r.removeFromTop(30);
    for (int i = 0; i < 4; ++i)
    {
        srcTabs[(size_t)i]->setBounds(top.removeFromLeft(96));
        top.removeFromLeft(6);
    }

    r.removeFromTop(6);

    // ターゲットタブ行
    auto tgtRow = r.removeFromTop(26);
    for (int i = 0; i < kNumTargets; ++i)
    {
        tgtTabs[(size_t)i]->setBounds(tgtRow.removeFromLeft(84));
        tgtRow.removeFromLeft(6);
    }

    r.removeFromTop(8);
    hint.setBounds(r.removeFromBottom(20));
    r.removeFromBottom(4);
    editor.setBounds(r);
}
