// ==========================================
// File: MainPanel.h
// MAINタブ (v0.2):
//  LIFT/Bars/Attack/Release/Master/Progress +
//  OSC1-3 (On/Solo/Mute, WAVEコンボ, 波形表示, BROWSE/RND, POS等6ノブ,
//          StartKey/EndKey MIDIラーン) + NOISE列 + Wavetableブラウザ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "WaveDisplay.h"
#include "WavetableBrowser.h"
#include "ColorPalette.h"

class MainPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit MainPanel(LiftXAudioProcessor& p);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // ---- Progress表示ストリップ (VBlank同期) ----
    class ProgressStrip : public juce::Component
    {
    public:
        explicit ProgressStrip(LiftXAudioProcessor& p)
            : proc(p),
              vblank(this, [this](double)
              {
                  const float v = proc.getUiProgress();
                  if (std::abs(v - last) > 0.001f) { last = v; repaint(); }
              })
        {
            setOpaque(false);
        }

        void paint(juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            g.setColour(LiftColors::knobTrack);
            g.fillRoundedRectangle(r, 6.0f);

            const float v = juce::jlimit(0.0f, 1.0f, proc.getUiProgress());
            if (v > 0.001f)
            {
                auto fr = r.reduced(2.0f);
                fr = fr.withWidth(fr.getWidth() * v);
                g.setColour(LiftColors::accentMaster.withAlpha(0.85f));
                g.fillRoundedRectangle(fr, 5.0f);
            }

            g.setColour(LiftColors::text);
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText("PROGRESS " + juce::String((int)std::round(v * 100.0f)) + "%",
                       getLocalBounds(), juce::Justification::centred);
        }

    private:
        LiftXAudioProcessor& proc;
        float last = -1.0f;
        juce::VBlankAttachment vblank;
    };

    struct KnobCell
    {
        ValueKnob knob;
        juce::Label label;
    };

    void timerCallback() override;
    void setupKnob(KnobCell& c, const juce::String& text, const juce::String& paramId,
                   juce::Colour accent);
    void setupCombo(juce::ComboBox& box, const juce::String& paramId,
                    const juce::StringArray& items);
    void layoutKnobGrid(juce::Rectangle<int> area, KnobCell** cells, int count, int cols);
    void refreshWaveDisplay(int osc);
    void refreshKeyButtons();
    void armLearn(const juce::String& paramId, juce::TextButton& btn);

    LiftXAudioProcessor& proc;

    // ---- グローバル ----
    KnobCell liftCell, attackCell, releaseCell, masterCell;
    juce::ComboBox barsBox;
    juce::Label barsLabel;
    ProgressStrip progressStrip;

    // ---- オシレーター 1-3 ----
    std::array<std::unique_ptr<GlowToggle>, 3> oscOn, oscSolo, oscMute;
    std::array<juce::ComboBox, 3> waveBox;
    std::array<WaveDisplay, 3> waveDisp;
    std::array<juce::TextButton, 3> browseBtn, rndBtn;
    std::array<KnobCell, 3> oscPos, oscLevel, oscCoarse, oscUni, oscDet, oscSpread;
    std::array<juce::TextButton, 3> keyStartBtn, keyEndBtn;

    // 波形表示の更新検知用
    std::array<int, 3> lastWaveMode { -1, -1, -1 };
    std::array<float, 3> lastPos { -1.0f, -1.0f, -1.0f };
    std::array<juce::String, 3> lastWtPath;

    // ---- ノイズ ----
    std::unique_ptr<GlowToggle> noiseSolo, noiseMute;
    juce::Label noiseTitle;
    juce::ComboBox noiseTypeBox;
    KnobCell noiseLevel, noisePitch, noiseRes, noiseRange;

    // ---- MIDIラーン ----
    juce::String armedParamId;          // 空=非武装
    juce::TextButton* armedButton = nullptr;
    int lastNoteEvents = 0;

    // ---- Wavetableブラウザ (オーバーレイ) ----
    WavetableBrowser browser;

    // ---- アタッチメント ----
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};
