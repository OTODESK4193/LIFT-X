// ==========================================
// File: FxPanel.h
// FXタブ (v0.2):
//  - 5スロットの適用順序 (SPECTRA8方式: スロット毎にFXタイプを選択)
//  - FX毎サブタブ (SAT/CHORUS/DELAY/REVERB/DUCK):
//    詳細ノブ + そのFXのENVカーブ (AMT/DRIVE等) を入れ子タブで編集
//  - カーブはバイポーラ加算式 (中央=ノブ値, ±レンジ半分)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "CurveEditor.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ModBand.h"
#include "ColorPalette.h"

class FxPanel : public juce::Component,
                private juce::Timer
{
public:
    explicit FxPanel(LiftXAudioProcessor& p);

    void refresh() { setCurve(activeCurve); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Cell
    {
        ValueKnob knob;
        juce::Label label;
    };

    struct FxDef
    {
        const char* name;
        int numCurves;
        int curveIdx[3];
        const char* curveNames[3];
    };
    static const std::array<FxDef, 6>& defs();

    void timerCallback() override;
    void updateDelayTimeLive(float envPos);
    void updateDuckRateLive(float envPos);
    void updateStutterRateLive(float envPos);
    void mkKnob(Cell& c, const juce::String& text, const juce::String& paramId, int accentId);
    void mkCombo(juce::ComboBox& box, juce::Label& label, const juce::String& text,
                 const juce::String& paramId, const juce::StringArray& items);
    void layoutCell(juce::Rectangle<int> area, Cell& c);
    void layoutDetailGrid(juce::Rectangle<int> area, std::vector<Cell*> cells,
                          juce::ComboBox* combo, juce::Label* comboLabel,
                          juce::Label* liveLabel = nullptr);
    void setFx(int idx);
    void setCurve(int idx);
    void styleTabButton(juce::TextButton& b, bool active, juce::Colour accent);
    std::vector<juce::Component*> componentsFor(int fx);

    LiftXAudioProcessor& proc;

    // ---- 5スロット (適用順序) ----
    juce::Label chainLabel;
    std::array<juce::ComboBox, FxChain::kNumSlots> slotType;

    // ---- FXサブタブ + カーブサブタブ ----
    std::array<std::unique_ptr<juce::TextButton>, 6> fxTabs;
    std::array<std::unique_ptr<juce::TextButton>, 3> curveTabs;
    int activeFx = 0;
    int activeCurve = 0;

    CurveEditor editor;
    juce::Label hint;

    // ---- 詳細コントロール ----
    Cell satAmt, satDrive, satPre, satTrim;
    juce::ComboBox satAlgoBox;
    juce::Label satAlgoLabel;

    Cell choAmt, choRate, choDepth, choWidth;

    Cell dlyAmt, dlyFb, dlyDuck, dlyDamp;
    juce::ComboBox dlyTimeBox;
    juce::Label dlyTimeLabel;
    // TIME / RATE はコンボボックスなのでノブの変調帯 (ModBand) が付けられない。
    // カーブでどれだけ動いているかが全く見えないため、実効値をテキストで出す。
    juce::Label dlyTimeLive, duckRateLive;

    Cell revAmt, revDecay, revShimmer, revDamp, revMod;

    Cell duckAmt, duckShape;
    juce::ComboBox duckRateBox;
    juce::Label duckRateLabel;

    Cell stutAmt;
    juce::ComboBox stutRateBox;
    juce::Label stutRateLabel, stutRateLive;

    // ---- ソース別ルーティング (選択中のエフェクトに追従) ----
    void rebuildRouteAttachments();
    juce::Label routeLabel;
    std::array<std::unique_ptr<GlowToggle>, RiserEngine::kNumSources> routeToggles;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,
               RiserEngine::kNumSources> routeAtts;

    juce::Rectangle<int> detailArea;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPanel)
};
