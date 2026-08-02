// ==========================================
// File: OscEnvPanel.cpp
// ==========================================
#include "OscEnvPanel.h"

namespace
{
    const char* kSrcNames[4] = { "OSC 1", "OSC 2", "OSC 3", "NOISE" };
    const char* kOscTgtNames[4] = { "PITCH", "LEVEL", "DETUNE", "SPREAD" };
    const char* kNoiseTgtNames[3] = { "PITCH", "LEVEL", "RES" };
}

OscEnvPanel::OscEnvPanel(LiftXAudioProcessor& p)
    : proc(p)
{
    for (int i = 0; i < 4; ++i)
    {
        srcTabs[(size_t)i] = std::make_unique<juce::TextButton>(kSrcNames[i]);
        srcTabs[(size_t)i]->onClick = [this, i] { setSource(i); };
        addAndMakeVisible(*srcTabs[(size_t)i]);

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

    hint.setFont(juce::Font(juce::FontOptions(12.0f)));
    hint.setColour(juce::Label::textColourId, LiftColors::textDim);
    hint.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hint);

    setSource(0);
}

int OscEnvPanel::curveIndex() const
{
    if (activeSource < 3)
        return CurveStore::oscCurve(activeSource, activeTarget);
    return CurveStore::noiseCurve(juce::jmin(activeTarget, 2));
}

void OscEnvPanel::setSource(int idx)
{
    activeSource = juce::jlimit(0, 3, idx);

    // ターゲットタブの名称/数をソースに合わせて更新
    const bool isNoise = (activeSource == 3);
    for (int i = 0; i < 4; ++i)
    {
        const bool visible = isNoise ? (i < 3) : true;
        tgtTabs[(size_t)i]->setVisible(visible);
        tgtTabs[(size_t)i]->setButtonText(isNoise ? (i < 3 ? kNoiseTgtNames[i] : "")
                                                  : kOscTgtNames[i]);
    }
    if (isNoise && activeTarget > 2)
        activeTarget = 0;

    setTarget(activeTarget);
}

void OscEnvPanel::setTarget(int idx)
{
    const bool isNoise = (activeSource == 3);
    activeTarget = juce::jlimit(0, isNoise ? 2 : 3, idx);

    const int ci = curveIndex();
    editor.setSnapshot(proc.getCurves().get(ci));
    editor.setAccent(LiftColors::curveAccent(ci));
    editor.setTitle(CurveStore::name(ci));

    // OSCピッチのみユニポーラ (下=StartKey / 上=EndKey)、他はバイポーラ (中央=ノブ値)
    editor.setBipolar(!(activeSource < 3 && activeTarget == 0));

    for (int i = 0; i < 4; ++i)
    {
        styleTabButton(*srcTabs[(size_t)i], i == activeSource,
                       i < 3 ? LiftColors::accentPitch : LiftColors::lilac);
        styleTabButton(*tgtTabs[(size_t)i], i == activeTarget, LiftColors::curveAccent(ci));
    }

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
    for (int i = 0; i < 4; ++i)
    {
        tgtTabs[(size_t)i]->setBounds(tgtRow.removeFromLeft(84));
        tgtRow.removeFromLeft(6);
    }

    r.removeFromTop(8);
    hint.setBounds(r.removeFromBottom(20));
    r.removeFromBottom(4);
    editor.setBounds(r);
}
