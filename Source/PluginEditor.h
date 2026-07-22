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
#include "GUI/PitchPanel.h"
#include "GUI/FilterPanel.h"
#include "GUI/FxPanel.h"

class LiftXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LiftXAudioProcessorEditor(LiftXAudioProcessor&);
    ~LiftXAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class Tab { Main, Pitch, Filter, Fx };

    void setActiveTab(Tab t);
    void styleTabButton(juce::TextButton& b, bool active);

    LiftXAudioProcessor& proc;
    ArcDialLookAndFeel lnf;

    juce::TextButton mainTabButton   { "MAIN" };
    juce::TextButton pitchTabButton  { "PITCH" };
    juce::TextButton filterTabButton { "FILTER" };
    juce::TextButton fxTabButton     { "FX" };
    Tab activeTab = Tab::Main;

    MainPanel mainPanel;
    PitchPanel pitchPanel;
    FilterPanel filterPanel;
    FxPanel fxPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiftXAudioProcessorEditor)
};
