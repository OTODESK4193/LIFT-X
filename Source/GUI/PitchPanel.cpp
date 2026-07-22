// ==========================================
// File: PitchPanel.cpp
// ==========================================
#include "PitchPanel.h"

namespace
{
    const char* kSubNames[4] = { "OSC 1", "OSC 2", "OSC 3", "NOISE" };
    const char* kRangeIds[4] = { "osc1Range", "osc2Range", "osc3Range", "noiseRange" };
    const char* kRangeUnits[4] = { "st", "st", "st", "oct" };
}

PitchPanel::PitchPanel(LiftXAudioProcessor& p)
    : proc(p)
{
    for (int i = 0; i < 4; ++i)
    {
        subTabs[(size_t)i] = std::make_unique<juce::TextButton>(kSubNames[i]);
        subTabs[(size_t)i]->onClick = [this, i] { setSub(i); };
        addAndMakeVisible(*subTabs[(size_t)i]);
    }

    addAndMakeVisible(editor);
    editor.setBipolar(true);
    editor.setProgressProvider([this] { return proc.getUiProgress(); });
    editor.onChanged = [this](const CurveSnapshot& s)
    {
        proc.getCurves().publish(CurveStore::PitchOsc1 + activeSub, s);
    };

    rangeKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    rangeKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 14);
    addAndMakeVisible(rangeKnob);

    rangeLabel.setText("RANGE", juce::dontSendNotification);
    rangeLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    rangeLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
    rangeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(rangeLabel);

    hint.setFont(juce::Font(juce::FontOptions(10.5f)));
    hint.setColour(juce::Label::textColourId, LiftColors::textDim);
    hint.setJustificationType(juce::Justification::centredLeft);
    hint.setText(juce::CharPointer_UTF8(
        "\xe4\xb8\xad\xe5\xa4\xae=\xe5\xa4\x89\xe5\x8c\x96\xe3\x81\xaa\xe3\x81\x97 / "
        "\xe4\xb8\x8a=+RANGE / \xe4\xb8\x8b=-RANGE   "
        "\xe3\x83\x80\xe3\x83\x96\xe3\x83\xab\xe3\x82\xaf\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xaf:"
        "\xe3\x83\x9d\xe3\x82\xa4\xe3\x83\xb3\xe3\x83\x88\xe8\xbf\xbd\xe5\x8a\xa0/\xe5\x89\x8a\xe9\x99\xa4  "
        "\xe2\x97\x86\xe3\x83\x89\xe3\x83\xa9\xe3\x83\x83\xe3\x82\xb0:\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x96\xe8\xaa\xbf\xe6\x95\xb4"),
        juce::dontSendNotification);
    addAndMakeVisible(hint);

    setSub(0);
}

void PitchPanel::setSub(int idx)
{
    activeSub = juce::jlimit(0, 3, idx);
    const int curveIdx = CurveStore::PitchOsc1 + activeSub;

    editor.setSnapshot(proc.getCurves().get(curveIdx));
    editor.setAccent(LiftColors::curveAccent(curveIdx));
    editor.setTitle(juce::String(CurveStore::name(curveIdx))
                    + "  (RANGE: " + kRangeUnits[activeSub] + ")");

    rangeAtt.reset();
    rangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, kRangeIds[activeSub], rangeKnob);
    rangeKnob.setColour(juce::Slider::rotarySliderFillColourId, LiftColors::curveAccent(curveIdx));

    for (int i = 0; i < 4; ++i)
        styleTabButton(*subTabs[(size_t)i], i == activeSub,
                       LiftColors::curveAccent(CurveStore::PitchOsc1 + i));
}

void PitchPanel::styleTabButton(juce::TextButton& b, bool active, juce::Colour accent)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? accent.withAlpha(0.22f) : LiftColors::knobTrack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? LiftColors::text : LiftColors::textDim);
    b.repaint();
}

void PitchPanel::paint(juce::Graphics& g)
{
    g.setColour(LiftColors::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 8.0f);
}

void PitchPanel::resized()
{
    auto r = getLocalBounds().reduced(12, 10);

    // サブタブ行 + RANGEノブ
    auto top = r.removeFromTop(30);
    for (int i = 0; i < 4; ++i)
    {
        subTabs[(size_t)i]->setBounds(top.removeFromLeft(96));
        top.removeFromLeft(6);
    }

    // RANGEノブ (右側縦積み)
    auto rightCol = r.removeFromRight(96).reduced(4);
    rangeLabel.setBounds(rightCol.removeFromTop(16));
    rangeKnob.setBounds(rightCol.removeFromTop(110).reduced(4));

    r.removeFromTop(8);
    hint.setBounds(r.removeFromBottom(18));
    r.removeFromBottom(4);
    editor.setBounds(r);
}
