// ==========================================
// File: MainPanel.h
// MAINタブ: LIFTノブ / Bars / オシレーター3基 / ノイズ / カスタムWT / Progress表示
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
#include "ColorPalette.h"

class MainPanel : public juce::Component
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
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText("PROGRESS " + juce::String((int)std::round(v * 100.0f)) + "%",
                       getLocalBounds(), juce::Justification::centred);
        }

    private:
        LiftXAudioProcessor& proc;
        float last = -1.0f;
        juce::VBlankAttachment vblank; // デストラクタで自動解除
    };

    struct KnobCell
    {
        ValueKnob knob;
        juce::Label label;
    };

    void setupKnob(KnobCell& c, const juce::String& text, const juce::String& paramId,
                   juce::Colour accent);
    void setupCombo(juce::ComboBox& box, juce::Label& label, const juce::String& text,
                    const juce::String& paramId, const juce::StringArray& items);
    void layoutKnobGrid(juce::Rectangle<int> area, KnobCell** cells, int count, int cols);
    void updateWtLabel();

    LiftXAudioProcessor& proc;

    // ---- LIFT / グローバル ----
    KnobCell liftCell, attackCell, releaseCell, masterCell;
    juce::ComboBox barsBox;
    juce::Label barsLabel;
    ProgressStrip progressStrip;

    // ---- オシレーター 1-3 ----
    std::array<std::unique_ptr<GlowToggle>, 3> oscOn;
    std::array<KnobCell, 3> oscWave, oscLevel, oscCoarse, oscUni, oscDet, oscSpread;

    // ---- ノイズ ----
    juce::ComboBox noiseTypeBox;
    juce::Label noiseTypeLabel;
    KnobCell noiseLevel, noisePitch, noiseRes;

    // ---- カスタムWavetable ----
    juce::TextButton loadWtButton { "LOAD WT" };
    juce::TextButton clearWtButton { "CLEAR WT" };
    juce::Label wtLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // ---- アタッチメント ----
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};
