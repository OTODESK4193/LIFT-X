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
#include "FilterResponseDisplay.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ModBand.h"
#include "ColorPalette.h"

class FilterPanel : public juce::Component,
                    private juce::Timer
{
public:
    explicit FilterPanel(LiftXAudioProcessor& p);

    void refresh() { setSub(activeSub); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void setSub(int idx);
    void styleTabButton(juce::TextButton& b, bool active);

    LiftXAudioProcessor& proc;

    std::array<std::unique_ptr<juce::TextButton>, 4> subTabs;
    int activeSub = 0;

    // ENV ターゲット切り替え (0=CUTOFF, 1=RES)
    std::array<std::unique_ptr<juce::TextButton>, 2> envTargetTabs;
    int activeEnvTarget = 0;

    CurveEditor editor;
    FilterResponseDisplay responseDisplay;

    // 選択中フィルターのコントロール (サブタブ切替時にアタッチメント再接続)
    std::unique_ptr<GlowToggle> onToggle;
    juce::ComboBox typeBox;
    juce::Label typeLabel;
    ValueKnob cutoffKnob, resKnob, envKnob;
    juce::Label cutoffLabel, resLabel, envLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAtt, resAtt, envAtt;

    // ソース別ルーティング (点灯=このフィルターを通る / 消灯=バイパス)
    std::array<std::unique_ptr<GlowToggle>, 4> routeToggles;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 4> routeAtts;
    juce::Label routeLabel;

    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPanel)
};
