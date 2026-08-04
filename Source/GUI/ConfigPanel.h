// ==========================================
// File: ConfigPanel.h
// CONFIGタブ: PITCH ENV スケール量子化 / マスターリミッター設定 / カラーテーマ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "../DSP/ScaleQuantizer.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ColorPalette.h"

class ConfigPanel : public juce::Component
{
public:
    explicit ConfigPanel(LiftXAudioProcessor& p)
        : proc(p)
    {
        // ================= PITCH ENV スケール量子化 =================
        scaleOn = std::make_unique<GlowToggle>("SCALE QUANTIZE", LiftColors::accentOsc);
        addAndMakeVisible(*scaleOn);
        btnAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "scaleOn", *scaleOn));

        setupLabel(keyLabel, "KEY");
        setupLabel(scaleLabel, "SCALE");
        setupLabel(oscApplyLabel, "APPLY TO");

        {
            juce::StringArray keys;
            for (int i = 0; i < 12; ++i) keys.add(ScaleQuantizer::keyName(i));
            keyBox.addItemList(keys, 1);
            addAndMakeVisible(keyBox);
            comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                proc.apvts, "scaleKey", keyBox));
        }
        {
            // 70種あるためカテゴリ見出しでグループ化する。
            // 見出し (addSectionHeading) はアイテムとして数えられないため、
            // ComboBoxAttachment のインデックス対応は崩れない。
            const auto& scales = ScaleQuantizer::getScales();
            struct Group { const char* head; int count; };
            static const Group groups[] = {
                { "BASIC",              16 },
                { "MODES & VARIANTS",   15 },
                { "WORLD",               9 },
                { "INDIAN",              4 },
                { "JAPAN / ASIA",       10 },
                { "SYMMETRIC / BEBOP",   7 },
                { "CHORD TONES",         9 },
            };

            int id = 1;
            for (const auto& g : groups)
            {
                scaleBox.addSectionHeading(g.head);
                for (int k = 0; k < g.count && id <= (int)scales.size(); ++k, ++id)
                    scaleBox.addItem(scales[(size_t)(id - 1)].name, id);
            }
            // 想定外のグループ合計ズレに備えた保険 (残りを末尾へ追加)
            for (; id <= (int)scales.size(); ++id)
                scaleBox.addItem(scales[(size_t)(id - 1)].name, id);

            addAndMakeVisible(scaleBox);
            comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                proc.apvts, "scaleType", scaleBox));
        }

        // OSC毎の適用トグル
        for (int i = 0; i < RiserEngine::kNumOscs; ++i)
        {
            oscScale[(size_t)i] = std::make_unique<GlowToggle>("OSC " + juce::String(i + 1),
                                                              LiftColors::accentOsc);
            addAndMakeVisible(*oscScale[(size_t)i]);
            btnAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                proc.apvts, "osc" + juce::String(i + 1) + "Scale", *oscScale[(size_t)i]));
        }

        scaleInfo.setFont(juce::Font(juce::FontOptions(11.5f)));
        scaleInfo.setColour(juce::Label::textColourId, LiftColors::textDim);
        scaleInfo.setText("OFF: Pitch ENV follows the curve smoothly.   "
                          "ON: pitch snaps to the nearest note of KEY + SCALE (stepped riser). "
                          "COARSE is added after quantizing, so octave/interval offsets stay exact.\n"
                          "Changing KEY / SCALE / APPLY TO also re-snaps each OSC's START and END "
                          "keys to the closest scale note - you can freely edit them afterwards.",
                          juce::dontSendNotification);
        scaleInfo.setJustificationType(juce::Justification::topLeft);
        scaleInfo.setMinimumHorizontalScale(1.0f);   // 字を詰めずに折り返す
        addAndMakeVisible(scaleInfo);

        // ================= KEY FOLLOW / VELOCITY =================
        setupLabel(keyFollowLabel, "KEY FOLLOW");
        keyFollowBox.addItemList({ "Fixed", "Follow Start", "Follow End" }, 1);
        addAndMakeVisible(keyFollowBox);
        comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.apvts, "keyFollow", keyFollowBox));

        setupKnob(velCutKnob,   velCutLabel,   "VEL > CUTOFF",  "velToCutoff");
        setupKnob(velNoiseKnob, velNoiseLabel, "VEL > NOISE",   "velToNoise");
        setupKnob(velDriveKnob, velDriveLabel, "VEL > DRIVE",   "velToDrive");
        setupKnob(humanizeKnob, humanizeLabel, "HUMANIZE",      "humanize");

        playInfo.setFont(juce::Font(juce::FontOptions(11.5f)));
        playInfo.setColour(juce::Label::textColourId, LiftColors::textDim);
        playInfo.setText(
            "KEY FOLLOW - by default the MIDI note is only a trigger and pitch comes "
            "entirely from START / END. Follow Start makes the played note the starting "
            "pitch; Follow End makes it the landing pitch (play the root of your drop and "
            "the riser arrives on it). OSC 1's keys are the reference, so all oscillators "
            "shift together and their intervals are preserved.\n"
            "VELOCITY - 0% keeps the original behaviour. Above 0, playing softer darkens "
            "the filter, pulls the noise layer back and reduces saturation drive.\n"
            "HUMANIZE - adds a slow organic drift to the envelope position so the "
            "riser is not perfectly mechanical. 0% = off.",
            juce::dontSendNotification);
        playInfo.setJustificationType(juce::Justification::topLeft);
        playInfo.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(playInfo);

        // ---- リミッター ----
        limOn = std::make_unique<GlowToggle>("LIMITER ON", LiftColors::accentMaster);
        addAndMakeVisible(*limOn);
        btnAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "limOn", *limOn));

        setupKnob(ceilKnob, ceilLabel, "CEILING", "limCeiling");
        setupKnob(relKnob, relLabel, "RELEASE", "limRelease");

        limInfo.setFont(juce::Font(juce::FontOptions(12.0f)));
        limInfo.setColour(juce::Label::textColourId, LiftColors::textDim);
        limInfo.setText("Brickwall: instant attack, zero latency (no PDC). "
                        "Output never exceeds CEILING.",
                        juce::dontSendNotification);
        limInfo.setJustificationType(juce::Justification::topLeft);
        limInfo.setMinimumHorizontalScale(1.0f);
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
        themeBanner.setText("Reopen the plugin window to fully apply the new theme",
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
        LiftColors::paintPanel(g, getLocalBounds().toFloat().reduced(2.0f));

        g.setColour(LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("PITCH ENV - SCALE", scaleArea.getX(), scaleArea.getY() - 20,
                   scaleArea.getWidth(), 16, juce::Justification::centredLeft);
        g.drawText("PLAYABILITY - KEY FOLLOW / VELOCITY", playArea.getX(), playArea.getY() - 20,
                   playArea.getWidth(), 16, juce::Justification::centredLeft);
        g.drawText("MASTER LIMITER", limArea.getX(), limArea.getY() - 20,
                   limArea.getWidth(), 16, juce::Justification::centredLeft);
        g.drawText("APPEARANCE", themeArea.getX(), themeArea.getY() - 20,
                   themeArea.getWidth(), 16, juce::Justification::centredLeft);

        g.setColour(LiftColors::panelLine);
        g.drawRoundedRectangle(scaleArea.toFloat(), 6.0f, 1.0f);
        g.drawRoundedRectangle(playArea.toFloat(), 6.0f, 1.0f);
        g.drawRoundedRectangle(limArea.toFloat(), 6.0f, 1.0f);
        g.drawRoundedRectangle(themeArea.toFloat(), 6.0f, 1.0f);
    }

    void resized() override
    {
        // ---- 2カラムレイアウト ----
        //  1カラムだと右半分が丸ごと余り、そのぶん各セクションを縦に
        //  詰める必要があってノブが潰れていた。左右に分けて縦の余裕を作る。
        auto r = getLocalBounds().reduced(18, 12);
        r.removeFromTop(16);

        auto leftCol  = r.removeFromLeft((int)(r.getWidth() * 0.56f));
        r.removeFromLeft(24);
        auto rightCol = r;

        // ================= 左: スケール量子化 =================
        scaleArea = leftCol.removeFromTop(245);
        {
            auto sc = scaleArea.reduced(14, 12);

            scaleOn->setBounds(sc.removeFromTop(26).removeFromLeft(190));
            sc.removeFromTop(10);

            auto row2 = sc.removeFromTop(46);
            auto keyCol = row2.removeFromLeft(86);
            keyLabel.setBounds(keyCol.removeFromTop(16));
            keyBox.setBounds(keyCol.removeFromTop(26).withTrimmedRight(10));

            auto scaleCol = row2;
            scaleLabel.setBounds(scaleCol.removeFromTop(16));
            scaleBox.setBounds(scaleCol.removeFromTop(26).withTrimmedRight(10));

            sc.removeFromTop(6);
            auto applyRow = sc.removeFromTop(42);
            oscApplyLabel.setBounds(applyRow.removeFromTop(16));
            auto tRow = applyRow.removeFromTop(26);
            for (int i = 0; i < RiserEngine::kNumOscs; ++i)
            {
                oscScale[(size_t)i]->setBounds(tRow.removeFromLeft(84));
                tRow.removeFromLeft(6);
            }

            sc.removeFromTop(4);
            scaleInfo.setBounds(sc);
        }

        leftCol.removeFromTop(14);

        // ================= 左: KEY FOLLOW / VELOCITY =================
        playArea = leftCol.removeFromTop(288);
        {
            auto pa = playArea.reduced(14, 12);

            auto kfRow = pa.removeFromTop(44);
            keyFollowLabel.setBounds(kfRow.removeFromTop(16));
            keyFollowBox.setBounds(kfRow.removeFromTop(26).removeFromLeft(190));

            pa.removeFromTop(6);
            auto velRow = pa.removeFromTop(90);
            ValueKnob* vk[4] = { &velCutKnob, &velNoiseKnob, &velDriveKnob, &humanizeKnob };
            juce::Label* vl[4] = { &velCutLabel, &velNoiseLabel, &velDriveLabel, &humanizeLabel };
            const int vw = velRow.getWidth() / 4;
            for (int i = 0; i < 4; ++i)
            {
                auto c = velRow.removeFromLeft(vw);
                vl[i]->setBounds(c.removeFromTop(16));
                vk[i]->setBounds(c.reduced(6, 0));
            }

            pa.removeFromTop(6);
            playInfo.setBounds(pa);
        }

        // ================= 右: マスターリミッター =================
        limArea = rightCol.removeFromTop(196);
        {
            auto lim = limArea.reduced(14, 12);
            limOn->setBounds(lim.removeFromTop(26).removeFromLeft(150));
            lim.removeFromTop(10);

            auto knobRow = lim.removeFromTop(104);
            auto c1 = knobRow.removeFromLeft(118);
            ceilLabel.setBounds(c1.removeFromTop(16));
            ceilKnob.setBounds(c1.reduced(6, 0));
            auto c2 = knobRow.removeFromLeft(118);
            relLabel.setBounds(c2.removeFromTop(16));
            relKnob.setBounds(c2.reduced(6, 0));

            lim.removeFromTop(6);
            limInfo.setBounds(lim);
        }

        rightCol.removeFromTop(28);

        // ================= 右: 外観 =================
        themeArea = rightCol.removeFromTop(104);
        {
            auto th = themeArea.reduced(14, 10);
            themeLabel.setBounds(th.removeFromTop(18));
            th.removeFromTop(4);
            themeBox.setBounds(th.removeFromTop(26).removeFromLeft(220));
            th.removeFromTop(6);
            themeBanner.setBounds(th.removeFromTop(20));
        }

        rightCol.removeFromTop(14);
        verInfo.setBounds(rightCol.removeFromTop(20));

    }

private:
    void setupLabel(juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, LiftColors::textDim);
        addAndMakeVisible(l);
    }

    void setupKnob(ValueKnob& k, juce::Label& l, const juce::String& text, const juce::String& paramId)
    {
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 15);
        k.setColour(juce::Slider::rotarySliderFillColourId, LiftColors::accentMaster);
        k.getProperties().set("accentId", (int)LiftColors::IdPink); // テーマ連動
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

    // ---- スケール量子化 ----
    std::unique_ptr<GlowToggle> scaleOn;
    std::array<std::unique_ptr<GlowToggle>, RiserEngine::kNumOscs> oscScale;
    juce::ComboBox keyBox, scaleBox;
    juce::Label keyLabel, scaleLabel, oscApplyLabel, scaleInfo;

    // ---- KEY FOLLOW / VELOCITY ----
    juce::ComboBox keyFollowBox;
    juce::Label keyFollowLabel, playInfo;
    ValueKnob velCutKnob, velNoiseKnob, velDriveKnob, humanizeKnob;
    juce::Label velCutLabel, velNoiseLabel, velDriveLabel, humanizeLabel;

    std::unique_ptr<GlowToggle> limOn;
    ValueKnob ceilKnob, relKnob;
    juce::Label ceilLabel, relLabel, limInfo;

    juce::Label themeLabel, themeBanner, verInfo;
    juce::ComboBox themeBox;

    juce::Rectangle<int> scaleArea, playArea, limArea, themeArea;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> btnAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigPanel)
};
