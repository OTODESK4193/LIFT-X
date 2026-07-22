// ==========================================
// File: FxPanel.cpp
// ==========================================
#include "FxPanel.h"

const char* FxPanel::groupName(int i)
{
    static const char* names[6] = { "SATURATION", "CHORUS", "DELAY", "FREEZE", "REVERB", "DUCKING" };
    return names[juce::jlimit(0, 5, i)];
}

FxPanel::FxPanel(LiftXAudioProcessor& p)
    : proc(p)
{
    // ---- 6スロット ----
    for (int s = 0; s < FxChain::kNumSlots; ++s)
    {
        const juce::String n(s + 1);

        slotLabel[(size_t)s].setText("SLOT " + n, juce::dontSendNotification);
        slotLabel[(size_t)s].setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        slotLabel[(size_t)s].setColour(juce::Label::textColourId, LiftColors::textDim);
        slotLabel[(size_t)s].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(slotLabel[(size_t)s]);

        slotType[(size_t)s].addItemList(FxChain::getTypeNames(), 1);
        addAndMakeVisible(slotType[(size_t)s]);
        comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.apvts, "fx" + n + "Type", slotType[(size_t)s]));

        mkKnob(slotAmt[(size_t)s], "AMT", "fx" + n + "Amt", LiftColors::accentFx);
        mkKnob(slotEnv[(size_t)s], "ENV", "fx" + n + "Env", LiftColors::accentFx);
    }

    // ---- Saturation ----
    mkCombo(satAlgoBox, satAlgoLabel, "ALGO", "satAlgo", FxChain::getSatAlgoNames());
    mkKnob(satDrive, "DRIVE", "satDrive", LiftColors::rose);
    mkKnob(satPre, "PRE HPF", "satPre", LiftColors::rose);
    mkKnob(satTrim, "TRIM", "satTrim", LiftColors::rose);

    // ---- Chorus ----
    mkKnob(choRate, "RATE", "choRate", LiftColors::mint);
    mkKnob(choDepth, "DEPTH", "choDepth", LiftColors::mint);
    mkKnob(choWidth, "WIDTH", "choWidth", LiftColors::mint);

    // ---- Delay ----
    mkCombo(dlyTimeBox, dlyTimeLabel, "TIME", "dlyTime", FxChain::getDelayTimeNames());
    mkKnob(dlyFb, "FB", "dlyFb", LiftColors::babyBlue);
    mkKnob(dlyDuck, "DUCK", "dlyDuck", LiftColors::babyBlue);
    mkKnob(dlyDamp, "DAMP", "dlyDamp", LiftColors::babyBlue);

    // ---- Freeze ----
    mkKnob(frzSize, "SIZE", "frzSize", LiftColors::lilac);
    mkKnob(frzFb, "FB", "frzFb", LiftColors::lilac);
    mkKnob(frzDamp, "DAMP", "frzDamp", LiftColors::lilac);

    // ---- Reverb ----
    mkKnob(revDecay, "DECAY", "revDecay", LiftColors::lavender);
    mkKnob(revShimmer, "SHIMMER", "revShimmer", LiftColors::lavender);
    mkKnob(revDamp, "DAMP", "revDamp", LiftColors::lavender);
    mkKnob(revMod, "MOD", "revMod", LiftColors::lavender);

    // ---- Ducking ----
    mkCombo(duckRateBox, duckRateLabel, "RATE", "duckRate", FxChain::getDuckRateNames());
    mkKnob(duckShape, "SHAPE", "duckShape", LiftColors::peach);

    // ---- FXカーブ ----
    addAndMakeVisible(editor);
    editor.setBipolar(false);
    editor.setAccent(LiftColors::curveAccent(CurveStore::FxCurve));
    editor.setTitle("FX ENV (0..1) x ENV knob -> slot Wet");
    editor.setProgressProvider([this] { return proc.getUiProgress(); });
    editor.onChanged = [this](const CurveSnapshot& s)
    {
        proc.getCurves().publish(CurveStore::FxCurve, s);
    };
    refresh();
}

void FxPanel::refresh()
{
    editor.setSnapshot(proc.getCurves().get(CurveStore::FxCurve));
}

void FxPanel::mkKnob(Cell& c, const juce::String& text, const juce::String& paramId, juce::Colour accent)
{
    c.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    c.knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 13);
    c.knob.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(c.knob);

    c.label.setText(text, juce::dontSendNotification);
    c.label.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
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
    label.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, LiftColors::textDim);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, paramId, box));
}

void FxPanel::layoutCell(juce::Rectangle<int> area, Cell& c)
{
    c.label.setBounds(area.removeFromTop(13));
    c.knob.setBounds(area.reduced(2));
}

void FxPanel::paint(juce::Graphics& g)
{
    g.setColour(LiftColors::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 8.0f);

    // 詳細グループ枠
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    for (int i = 0; i < 6; ++i)
    {
        const auto r = groupRects[(size_t)i];
        if (r.isEmpty()) continue;
        g.setColour(LiftColors::panelLine);
        g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.0f);
        g.setColour(LiftColors::textDim);
        g.drawText(groupName(i), r.getX() + 8, r.getY() + 3, r.getWidth() - 16, 12,
                   juce::Justification::centredLeft);
    }
}

void FxPanel::resized()
{
    auto r = getLocalBounds().reduced(12, 10);

    // ---- 上段: 6スロット ----
    auto slotsRow = r.removeFromTop(128);
    const int slotW = slotsRow.getWidth() / FxChain::kNumSlots;
    for (int s = 0; s < FxChain::kNumSlots; ++s)
    {
        auto slot = slotsRow.removeFromLeft(slotW).reduced(4, 0);
        slotLabel[(size_t)s].setBounds(slot.removeFromTop(14));
        slotType[(size_t)s].setBounds(slot.removeFromTop(22));
        slot.removeFromTop(2);

        const int half = slot.getWidth() / 2;
        auto amtArea = slot.removeFromLeft(half);
        layoutCell(amtArea, slotAmt[(size_t)s]);
        layoutCell(slot, slotEnv[(size_t)s]);
    }

    r.removeFromTop(10);

    // ---- 右: FXカーブエディタ ----
    auto curveArea = r.removeFromRight(370).reduced(4, 0);
    editor.setBounds(curveArea);
    r.removeFromRight(6);

    // ---- 左: 詳細グループ 3行x2列 ----
    const int rows = 3, cols = 2;
    const int gw = r.getWidth() / cols;
    const int gh = r.getHeight() / rows;

    for (int i = 0; i < 6; ++i)
    {
        juce::Rectangle<int> cell(r.getX() + (i % cols) * gw,
                                  r.getY() + (i / cols) * gh, gw, gh);
        cell = cell.reduced(3);
        groupRects[(size_t)i] = cell;

        auto inner = cell.reduced(8).withTrimmedTop(14);

        switch (i)
        {
        case 0: // SATURATION
        {
            auto comboCol = inner.removeFromLeft(92);
            satAlgoLabel.setBounds(comboCol.removeFromTop(13));
            satAlgoBox.setBounds(comboCol.removeFromTop(22));
            const int w = inner.getWidth() / 3;
            layoutCell(inner.removeFromLeft(w), satDrive);
            layoutCell(inner.removeFromLeft(w), satPre);
            layoutCell(inner, satTrim);
            break;
        }
        case 1: // CHORUS
        {
            const int w = inner.getWidth() / 3;
            layoutCell(inner.removeFromLeft(w), choRate);
            layoutCell(inner.removeFromLeft(w), choDepth);
            layoutCell(inner, choWidth);
            break;
        }
        case 2: // DELAY
        {
            auto comboCol = inner.removeFromLeft(78);
            dlyTimeLabel.setBounds(comboCol.removeFromTop(13));
            dlyTimeBox.setBounds(comboCol.removeFromTop(22));
            const int w = inner.getWidth() / 3;
            layoutCell(inner.removeFromLeft(w), dlyFb);
            layoutCell(inner.removeFromLeft(w), dlyDuck);
            layoutCell(inner, dlyDamp);
            break;
        }
        case 3: // FREEZE
        {
            const int w = inner.getWidth() / 3;
            layoutCell(inner.removeFromLeft(w), frzSize);
            layoutCell(inner.removeFromLeft(w), frzFb);
            layoutCell(inner, frzDamp);
            break;
        }
        case 4: // REVERB
        {
            const int w = inner.getWidth() / 4;
            layoutCell(inner.removeFromLeft(w), revDecay);
            layoutCell(inner.removeFromLeft(w), revShimmer);
            layoutCell(inner.removeFromLeft(w), revDamp);
            layoutCell(inner, revMod);
            break;
        }
        case 5: // DUCKING
        {
            auto comboCol = inner.removeFromLeft(92);
            duckRateLabel.setBounds(comboCol.removeFromTop(13));
            duckRateBox.setBounds(comboCol.removeFromTop(22));
            layoutCell(inner.removeFromLeft(inner.getWidth() / 2), duckShape);
            break;
        }
        default: break;
        }
    }
}
