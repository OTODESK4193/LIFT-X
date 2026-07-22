// ==========================================
// File: MainPanel.cpp
// ==========================================
#include "MainPanel.h"

MainPanel::MainPanel(LiftXAudioProcessor& p)
    : proc(p), progressStrip(p)
{
    // ---- グローバル ----
    setupKnob(liftCell, "LIFT", "lift", LiftColors::accentMaster);
    setupKnob(attackCell, "ATTACK", "attack", LiftColors::accentMaster);
    setupKnob(releaseCell, "RELEASE", "release", LiftColors::accentMaster);
    setupKnob(masterCell, "MASTER", "master", LiftColors::accentMaster);
    setupCombo(barsBox, barsLabel, "BARS", "bars", { "1", "2", "4", "8", "16" });

    addAndMakeVisible(progressStrip);

    // ---- オシレーター ----
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n(i + 1);
        oscOn[(size_t)i] = std::make_unique<GlowToggle>("OSC " + n, LiftColors::accentOsc);
        addAndMakeVisible(*oscOn[(size_t)i]);
        buttonAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, "osc" + n + "On", *oscOn[(size_t)i]));

        setupKnob(oscWave[(size_t)i], "WAVE", "osc" + n + "Wave", LiftColors::accentOsc);
        setupKnob(oscLevel[(size_t)i], "LEVEL", "osc" + n + "Level", LiftColors::accentOsc);
        setupKnob(oscCoarse[(size_t)i], "COARSE", "osc" + n + "Coarse", LiftColors::accentOsc);
        setupKnob(oscUni[(size_t)i], "UNISON", "osc" + n + "Uni", LiftColors::accentOsc);
        setupKnob(oscDet[(size_t)i], "DETUNE", "osc" + n + "Det", LiftColors::accentOsc);
        setupKnob(oscSpread[(size_t)i], "SPREAD", "osc" + n + "Spread", LiftColors::accentOsc);
    }

    // ---- ノイズ ----
    setupCombo(noiseTypeBox, noiseTypeLabel, "NOISE", "noiseType", { "White", "Pink", "Brown" });
    setupKnob(noiseLevel, "LEVEL", "noiseLevel", LiftColors::lilac);
    setupKnob(noisePitch, "PITCH", "noisePitch", LiftColors::lilac);
    setupKnob(noiseRes, "RES", "noiseRes", LiftColors::lilac);

    // ---- カスタムWavetable ----
    addAndMakeVisible(loadWtButton);
    addAndMakeVisible(clearWtButton);
    addAndMakeVisible(wtLabel);
    wtLabel.setFont(juce::Font(juce::FontOptions(10.5f)));
    wtLabel.setColour(juce::Label::textColourId, LiftColors::textDim);
    wtLabel.setJustificationType(juce::Justification::centredLeft);

    loadWtButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Load Wavetable (wav/aiff, 2048 samples/frame)",
            juce::File(), "*.wav;*.aif;*.aiff");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile())
                    proc.loadCustomWavetable(file);
                updateWtLabel();
            });
    };

    clearWtButton.onClick = [this]
    {
        proc.clearCustomWavetable();
        updateWtLabel();
    };

    updateWtLabel();
}

void MainPanel::updateWtLabel()
{
    const auto path = proc.getCustomWavetablePath();
    wtLabel.setText(proc.hasCustomWavetable()
                        ? "WT: " + juce::File(path).getFileName()
                        : "WT: Built-in (Sine>Tri>Sqr>Saw>FM)",
                    juce::dontSendNotification);
}

void MainPanel::setupKnob(KnobCell& c, const juce::String& text, const juce::String& paramId,
                          juce::Colour accent)
{
    c.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    c.knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 14);
    c.knob.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(c.knob);

    c.label.setText(text, juce::dontSendNotification);
    c.label.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    c.label.setColour(juce::Label::textColourId, LiftColors::textDim);
    c.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(c.label);

    sliderAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramId, c.knob));
}

void MainPanel::setupCombo(juce::ComboBox& box, juce::Label& label, const juce::String& text,
                           const juce::String& paramId, const juce::StringArray& items)
{
    box.addItemList(items, 1);
    addAndMakeVisible(box);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, LiftColors::textDim);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    comboAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, paramId, box));
}

void MainPanel::layoutKnobGrid(juce::Rectangle<int> area, KnobCell** cells, int count, int cols)
{
    const int rows = (count + cols - 1) / cols;
    const int cw = area.getWidth() / cols;
    const int ch = area.getHeight() / juce::jmax(1, rows);

    for (int i = 0; i < count; ++i)
    {
        const int cx = area.getX() + (i % cols) * cw;
        const int cy = area.getY() + (i / cols) * ch;
        juce::Rectangle<int> cell(cx, cy, cw, ch);

        cells[i]->label.setBounds(cell.removeFromTop(14));
        cells[i]->knob.setBounds(cell.reduced(2));
    }
}

void MainPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // セクション背景 (上段グローバル / 下段オシレーター群)
    g.setColour(LiftColors::panel);
    g.fillRoundedRectangle(r.removeFromTop(158.0f).reduced(2.0f), 8.0f);
    g.fillRoundedRectangle(r.reduced(2.0f).withTrimmedTop(4.0f), 8.0f);

    // 列区切り線
    const auto cols = getLocalBounds().withTrimmedTop(166);
    g.setColour(LiftColors::panelLine);
    for (int i = 1; i < 4; ++i)
    {
        const int x = cols.getX() + cols.getWidth() * i / 4;
        g.drawVerticalLine(x, (float)cols.getY() + 12.0f, (float)cols.getBottom() - 12.0f);
    }
}

void MainPanel::resized()
{
    auto r = getLocalBounds();

    // ---- 上段: グローバル ----
    auto top = r.removeFromTop(158).reduced(10, 6);

    // LIFT大型ノブ
    {
        auto liftArea = top.removeFromLeft(150);
        liftCell.label.setBounds(liftArea.removeFromTop(16));
        liftCell.knob.setBounds(liftArea.reduced(2));
    }

    // BARS
    {
        auto barsArea = top.removeFromLeft(86).reduced(4, 0);
        barsLabel.setBounds(barsArea.removeFromTop(16));
        barsBox.setBounds(barsArea.removeFromTop(26).reduced(2, 0));
    }

    // ATTACK / RELEASE / MASTER
    KnobCell* globals[3] = { &attackCell, &releaseCell, &masterCell };
    for (auto* c : globals)
    {
        auto cell = top.removeFromLeft(92);
        c->label.setBounds(cell.removeFromTop(16));
        c->knob.setBounds(cell.reduced(4));
    }

    // Progress + WT
    auto right = top.reduced(8, 0);
    progressStrip.setBounds(right.removeFromTop(34));
    right.removeFromTop(8);
    auto wtRow = right.removeFromTop(28);
    loadWtButton.setBounds(wtRow.removeFromLeft(92));
    wtRow.removeFromLeft(6);
    clearWtButton.setBounds(wtRow.removeFromLeft(92));
    right.removeFromTop(4);
    wtLabel.setBounds(right.removeFromTop(18));

    // ---- 下段: OSC1-3 + NOISE の4列 ----
    r.removeFromTop(8);
    const int colW = r.getWidth() / 4;

    for (int i = 0; i < 3; ++i)
    {
        auto col = r.removeFromLeft(colW).reduced(10, 8);
        oscOn[(size_t)i]->setBounds(col.removeFromTop(26));
        col.removeFromTop(6);

        KnobCell* cells[6] = { &oscWave[(size_t)i], &oscLevel[(size_t)i],
                               &oscCoarse[(size_t)i], &oscUni[(size_t)i],
                               &oscDet[(size_t)i], &oscSpread[(size_t)i] };
        layoutKnobGrid(col, cells, 6, 2);
    }

    // ノイズ列
    {
        auto col = r.reduced(10, 8);
        auto head = col.removeFromTop(26);
        noiseTypeLabel.setBounds(head.removeFromLeft(48));
        noiseTypeBox.setBounds(head);
        col.removeFromTop(6);

        KnobCell* cells[3] = { &noiseLevel, &noisePitch, &noiseRes };
        layoutKnobGrid(col.removeFromTop(col.getHeight() * 2 / 3), cells, 3, 2);
    }
}
