// ==========================================
// File: FxPanel.h
// FXタブ: 6スロット直列FXチェーン + FXマルチENVカーブ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "CurveEditor.h"
#include "ValueKnob.h"
#include "ColorPalette.h"

class FxPanel : public juce::Component
{
public:
    explicit FxPanel(LiftXAudioProcessor& p);

    void refresh();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Cell
    {
        ValueKnob knob;
        juce::Label label;
    };

    void mkKnob(Cell& c, const juce::String& text, const juce::String& paramId, juce::Colour accent);
    void mkCombo(juce::ComboBox& box, juce::Label& label, const juce::String& text,
                 const juce::String& paramId, const juce::StringArray& items);
    void layoutCell(juce::Rectangle<int> area, Cell& c);

    LiftXAudioProcessor& proc;

    // ---- 6スロット ----
    std::array<juce::ComboBox, FxChain::kNumSlots> slotType;
    std::array<juce::Label, FxChain::kNumSlots> slotLabel;
    std::array<Cell, FxChain::kNumSlots> slotAmt, slotEnv;

    // ---- 詳細: Saturation ----
    juce::ComboBox satAlgoBox;
    juce::Label satAlgoLabel;
    Cell satDrive, satPre, satTrim;

    // ---- 詳細: Chorus ----
    Cell choRate, choDepth, choWidth;

    // ---- 詳細: Delay ----
    juce::ComboBox dlyTimeBox;
    juce::Label dlyTimeLabel;
    Cell dlyFb, dlyDuck, dlyDamp;

    // ---- 詳細: Freeze ----
    Cell frzSize, frzFb, frzDamp;

    // ---- 詳細: Reverb ----
    Cell revDecay, revShimmer, revDamp, revMod;

    // ---- 詳細: Ducking ----
    juce::ComboBox duckRateBox;
    juce::Label duckRateLabel;
    Cell duckShape;

    // ---- FXカーブ (マルチENV) ----
    CurveEditor editor;

    // グループ枠 (paint用)
    std::array<juce::Rectangle<int>, 6> groupRects;
    static const char* groupName(int i);

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPanel)
};
