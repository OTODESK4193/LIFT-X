// ==========================================
// File: ConfigPanel.h
// CONFIGタブ: マスターリミッター設定 / カラーテーマ (Granular参照)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ColorPalette.h"

class ConfigPanel : public juce::Component
{
public:
    explicit ConfigPanel(LiftXAudioProcessor& p)
        : proc(p)
    {
        // ---- リミッター ----
        limOn = std::make_unique<GlowToggle>("LIMITER ON", LiftColors::accentMaster);
        addAndMakeVisible(*limOn);
        btnAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "limOn", *limOn));

        setupKnob(ceilKnob, ceilLabel, "CEILING", "limCeiling");
        setupKnob(relKnob, relLabel, "RELEASE", "limRelease");

        limInfo.setFont(juce::Font(juce::FontOptions(12.0f)));
        limInfo.setColour(juce::Label::textColourId, LiftColors::textDim);
        limInfo.setText(juce::CharPointer_UTF8(
            "\xe3\x83\x96\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xaf\xe3\x82\xa6\xe3\x82\xa9\xe3\x83\xbc\xe3\x83\xab\xe6\x96\xb9\xe5\xbc\x8f: "
            "\xe7\x9e\xac\xe9\x96\x93\xe3\x82\xa2\xe3\x82\xbf\xe3\x83\x83\xe3\x82\xaf / \xe3\x83\xac\xe3\x82\xa4\xe3\x83\x86\xe3\x83\xb3\xe3\x82\xb7""0 (PDC\xe4\xb8\x8d\xe8\xa6\x81)\xe3\x80\x82"
            "\xe5\x87\xba\xe5\x8a\x9b\xe3\x81\xaf\xe5\xbf\x85\xe3\x81\x9a""CEILING\xe4\xbb\xa5\xe4\xb8\x8b\xe3\x81\xab\xe5\x8f\x8e\xe3\x81\xbe\xe3\x82\x8a\xe3\x81\xbe\xe3\x81\x99\xe3\x80\x82"),
            juce::dontSendNotification);
        addAndMakeVisible(limInfo);

        // ---- カラーテーマ ----
        themeLabel.setText("COLOR THEME", juce::dontSendNotification);
        themeLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        themeLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
        addAndMakeVisible(themeLabel);

        themeBox.addItemList(LiftColors::getThemeNames(), 1);
        themeBox.setSelectedItemIndex(
            LiftXAudioProcessor::getGlobalSettings().getIntValue("colorTheme", 0),
            juce::dontSendNotification);
        themeBox.onChange = [this]
        {
            const int idx = themeBox.getSelectedItemIndex();
            auto& s = LiftXAudioProcessor::getGlobalSettings();
            s.setValue("colorTheme", idx);
            s.saveIfNeeded();
            LiftColors::setTheme(idx);
            themeBanner.setVisible(true);
            if (auto* top = getTopLevelComponent())
                top->repaint();
        };
        addAndMakeVisible(themeBox);

        themeBanner.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        themeBanner.setColour(juce::Label::textColourId, LiftColors::peach);
        themeBanner.setText(juce::CharPointer_UTF8(
            "\xe3\x83\x86\xe3\x83\xbc\xe3\x83\x9e\xe5\xa4\x89\xe6\x9b\xb4\xe3\x82\x92\xe5\xae\x8c\xe5\x85\xa8\xe3\x81\xab\xe9\x81\xa9\xe7\x94\xa8\xe3\x81\x99\xe3\x82\x8b\xe3\x81\xab\xe3\x81\xaf\xe3\x80\x81"
            "\xe3\x83\x97\xe3\x83\xa9\xe3\x82\xb0\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xa6\xe3\x82\xa3\xe3\x83\xb3\xe3\x83\x89\xe3\x82\xa6\xe3\x82\x92\xe9\x96\x8b\xe3\x81\x8d\xe7\x9b\xb4\xe3\x81\x97\xe3\x81\xa6\xe3\x81\x8f\xe3\x81\xa0\xe3\x81\x95\xe3\x81\x84"),
            juce::dontSendNotification);
        themeBanner.setVisible(false);
        addAndMakeVisible(themeBanner);

        // ---- バージョン情報 ----
        verInfo.setFont(juce::Font(juce::FontOptions(11.5f)));
        verInfo.setColour(juce::Label::textColourId, LiftColors::textDim);
        verInfo.setText("LIFT-X v" LIFTX_VERSION "  -  OTODESK  /  JUCE 8  /  SR 44.1-192kHz",
                        juce::dontSendNotification);
        addAndMakeVisible(verInfo);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(LiftColors::panel);
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 8.0f);

        g.setColour(LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("MASTER LIMITER", limArea.getX(), limArea.getY() - 20,
                   limArea.getWidth(), 16, juce::Justification::centredLeft);
        g.drawText("APPEARANCE", themeArea.getX(), themeArea.getY() - 20,
                   themeArea.getWidth(), 16, juce::Justification::centredLeft);

        g.setColour(LiftColors::panelLine);
        g.drawRoundedRectangle(limArea.toFloat(), 6.0f, 1.0f);
        g.drawRoundedRectangle(themeArea.toFloat(), 6.0f, 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(20, 16);
        r.removeFromTop(24);

        // リミッターセクション
        limArea = r.removeFromTop(180);
        auto lim = limArea.reduced(14, 12);
        limOn->setBounds(lim.removeFromTop(26).removeFromLeft(150));
        lim.removeFromTop(8);
        auto knobRow = lim.removeFromTop(110);
        auto c1 = knobRow.removeFromLeft(110);
        ceilLabel.setBounds(c1.removeFromTop(16));
        ceilKnob.setBounds(c1.reduced(4));
        auto c2 = knobRow.removeFromLeft(110);
        relLabel.setBounds(c2.removeFromTop(16));
        relKnob.setBounds(c2.reduced(4));
        limInfo.setBounds(knobRow.reduced(8, 30));

        r.removeFromTop(36);

        // テーマセクション
        themeArea = r.removeFromTop(110);
        auto th = themeArea.reduced(14, 12);
        themeLabel.setBounds(th.removeFromTop(18));
        th.removeFromTop(4);
        themeBox.setBounds(th.removeFromTop(26).removeFromLeft(220));
        th.removeFromTop(8);
        themeBanner.setBounds(th.removeFromTop(20));

        r.removeFromTop(16);
        verInfo.setBounds(r.removeFromTop(20));
    }

private:
    void setupKnob(ValueKnob& k, juce::Label& l, const juce::String& text, const juce::String& paramId)
    {
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 15);
        k.setColour(juce::Slider::rotarySliderFillColourId, LiftColors::accentMaster);
        addAndMakeVisible(k);

        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, LiftColors::textDim);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);

        sliderAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            proc.apvts, paramId, k));
    }

    LiftXAudioProcessor& proc;

    std::unique_ptr<GlowToggle> limOn;
    ValueKnob ceilKnob, relKnob;
    juce::Label ceilLabel, relLabel, limInfo;

    juce::Label themeLabel, themeBanner, verInfo;
    juce::ComboBox themeBox;

    juce::Rectangle<int> limArea, themeArea;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> btnAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigPanel)
};
