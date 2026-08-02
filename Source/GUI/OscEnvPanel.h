// ==========================================
// File: OscEnvPanel.h
// OSC ENVタブ: ソース (OSC1-3/NOISE) × ターゲット
//  (PITCH/LEVEL/DETUNE/SPREAD, ノイズはPITCH/LEVEL/RES) の入れ子タブで
//  マルチENVカーブを編集する
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

#include "../PluginProcessor.h"
#include "CurveEditor.h"
#include "ColorPalette.h"

class OscEnvPanel : public juce::Component
{
public:
    explicit OscEnvPanel(LiftXAudioProcessor& p);

    // タブ表示時にCurveStoreから再読込
    void refresh() { setTarget(activeTarget); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setSource(int idx);   // 0-2=OSC1-3, 3=NOISE
    void setTarget(int idx);   // 0=PITCH 1=LEVEL 2=DETUNE 3=SPREAD 4=PAN (ノイズ: 2=RES)
    int curveIndex() const;
    void styleTabButton(juce::TextButton& b, bool active, juce::Colour accent);
    void updateHint();

    LiftXAudioProcessor& proc;

    // ターゲット: 0=PITCH 1=LEVEL 2=DETUNE(ノイズはRES) 3=SPREAD 4=PAN
    static constexpr int kNumTargets = 5;
    static constexpr int kPanTarget  = 4;

    std::array<std::unique_ptr<juce::TextButton>, 4> srcTabs;            // OSC1-3 / NOISE
    std::array<std::unique_ptr<juce::TextButton>, kNumTargets> tgtTabs;  // ターゲット
    int activeSource = 0;
    int activeTarget = 0;

    CurveEditor editor;
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscEnvPanel)
};
