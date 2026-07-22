// ==========================================
// File: FilterPanel.h
// FILTERタブ: ZDF/TPTフィルター4系統のマルチENVカーブをサブタブで切替
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

#include "../PluginProcessor.h"
#include "CurveEditor.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ColorPalette.h"

class FilterPanel : public juce::Component
{
public:
    explicit FilterPanel(LiftXAudioProcessor& p);

    void refresh() { setSub(activeSub); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setSub(int idx);
    void styleTabButton(juce::TextButton& b, bool active);

    LiftXAudioProcessor& proc;

    std::array<std::unique_ptr<juce::TextButton>, 4> subTabs;
    int activeSub = 0;

    CurveEditor editor;

    // 選択中フィルターのコントロール (サブタブ切替時にアタッチメント再接続)
    std::unique_ptr<GlowToggle> onToggle;
    juce::ComboBox typeBox;
    juce::Label typeLabel;
    ValueKnob cutoffKnob, resKnob, envKnob;
    juce::Label cutoffLabel, resLabel, envLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAtt, resAtt, envAtt;

    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPanel)
};
