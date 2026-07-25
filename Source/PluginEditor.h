// ==========================================
// File: PluginEditor.h
// LIFT-X エディタ (タブ方式: MAIN / PITCH / FILTER / FX)
// ==========================================
#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "GUI/ArcDial.h"
#include "GUI/ColorPalette.h"
#include "GUI/MainPanel.h"
#include "GUI/OscEnvPanel.h"
#include "GUI/FilterPanel.h"
#include "GUI/FxPanel.h"
#include "GUI/PresetPanel.h"
#include "GUI/ConfigPanel.h"

// LIFT動作モードのトグル (押すたびに MANUAL ⇔ AUTO 表示が切り替わる)
//  MANUAL: LIFTノブは手動/DAWオートメーション
//  AUTO  : LIFTノブがProgressに連動して動的に動く
class LiftModeButton : public juce::Button
{
public:
    LiftModeButton() : juce::Button("liftMode")
    {
        setClickingTogglesState(true);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool) override
    {
        const bool on = getToggleState(); // true = AUTO
        auto r = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(on ? LiftColors::accentMaster
                       : (highlighted ? LiftColors::knobTrack.brighter(0.15f) : LiftColors::knobTrack));
        g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
        g.setColour(on ? LiftColors::accentMaster.brighter(0.4f) : LiftColors::panelLine);
        g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.2f);

        g.setColour(on ? LiftColors::bg : LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(on ? "LIFT: AUTO" : "LIFT: MANUAL",
                   getLocalBounds(), juce::Justification::centred);
    }
};

class LiftXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit LiftXAudioProcessorEditor(LiftXAudioProcessor&);
    ~LiftXAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class Tab { Main, OscEnv, Filter, Fx, Preset, Config };

    void timerCallback() override;

    void setActiveTab(Tab t);
    void styleTabButton(juce::TextButton& b, bool active);

    LiftXAudioProcessor& proc;
    ArcDialLookAndFeel lnf;

    juce::TextButton mainTabButton   { "MAIN" };
    juce::TextButton oscEnvTabButton { "OSC ENV" };
    juce::TextButton filterTabButton { "FILTER" };
    juce::TextButton fxTabButton     { "FX" };
    juce::TextButton presetTabButton { "PRESET" };
    juce::TextButton configTabButton { "CONFIG" };
    Tab activeTab = Tab::Main;

    LiftModeButton liftModeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> liftModeAtt;

    // ヘッダー右: プリセットナビゲーション (◀ 名前 ▶)
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };
    juce::Label presetNameLabel;

    MainPanel mainPanel;
    OscEnvPanel oscEnvPanel;
    FilterPanel filterPanel;
    FxPanel fxPanel;
    PresetPanel presetPanel;
    ConfigPanel configPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiftXAudioProcessorEditor)
};
