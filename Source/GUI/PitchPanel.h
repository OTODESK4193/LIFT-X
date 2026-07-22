// ==========================================
// File: PitchPanel.h
// PITCHタブ: マルチENVカーブ ×4 (OSC1/OSC2/OSC3/NOISE) をサブタブで切替
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

#include "../PluginProcessor.h"
#include "CurveEditor.h"
#include "ValueKnob.h"
#include "ColorPalette.h"

class PitchPanel : public juce::Component
{
public:
    explicit PitchPanel(LiftXAudioProcessor& p);

    // タブ表示時にCurveStoreから再読込 (プリセット/ステート復元対応)
    void refresh() { setSub(activeSub); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setSub(int idx);
    void styleTabButton(juce::TextButton& b, bool active, juce::Colour accent);

    LiftXAudioProcessor& proc;

    std::array<std::unique_ptr<juce::TextButton>, 4> subTabs;
    int activeSub = 0;

    CurveEditor editor;

    ValueKnob rangeKnob;
    juce::Label rangeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAtt;

    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchPanel)
};
