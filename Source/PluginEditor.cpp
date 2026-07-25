// ==========================================
// File: PluginEditor.cpp
// ==========================================
#include "PluginEditor.h"

LiftXAudioProcessorEditor::LiftXAudioProcessorEditor(LiftXAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      mainPanel(p), oscEnvPanel(p), filterPanel(p), fxPanel(p),
      presetPanel(p), configPanel(p)
{
    setLookAndFeel(&lnf);

    auto initTab = [this](juce::TextButton& b, Tab t)
    {
        b.onClick = [this, t] { setActiveTab(t); };
        addAndMakeVisible(b);
    };
    initTab(mainTabButton, Tab::Main);
    initTab(oscEnvTabButton, Tab::OscEnv);
    initTab(filterTabButton, Tab::Filter);
    initTab(fxTabButton, Tab::Fx);
    initTab(presetTabButton, Tab::Preset);
    initTab(configTabButton, Tab::Config);

    // LIFT: MANUAL/AUTO トグル (MAINタブの左)
    addAndMakeVisible(liftModeButton);
    liftModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "liftMode", liftModeButton);

    addChildComponent(mainPanel);
    addChildComponent(oscEnvPanel);
    addChildComponent(filterPanel);
    addChildComponent(fxPanel);
    addChildComponent(presetPanel);
    addChildComponent(configPanel);

    setActiveTab(Tab::Main);
    setSize(1020, 640);
}

LiftXAudioProcessorEditor::~LiftXAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void LiftXAudioProcessorEditor::setActiveTab(Tab t)
{
    activeTab = t;

    mainPanel.setVisible(t == Tab::Main);
    oscEnvPanel.setVisible(t == Tab::OscEnv);
    filterPanel.setVisible(t == Tab::Filter);
    fxPanel.setVisible(t == Tab::Fx);
    presetPanel.setVisible(t == Tab::Preset);
    configPanel.setVisible(t == Tab::Config);

    // カーブはCurveStoreが真実の源: タブ表示時に再読込 (ステート/プリセット復元対応)
    if (t == Tab::OscEnv) oscEnvPanel.refresh();
    if (t == Tab::Filter) filterPanel.refresh();
    if (t == Tab::Fx)     fxPanel.refresh();
    if (t == Tab::Preset) presetPanel.refresh();

    styleTabButton(mainTabButton, t == Tab::Main);
    styleTabButton(oscEnvTabButton, t == Tab::OscEnv);
    styleTabButton(filterTabButton, t == Tab::Filter);
    styleTabButton(fxTabButton, t == Tab::Fx);
    styleTabButton(presetTabButton, t == Tab::Preset);
    styleTabButton(configTabButton, t == Tab::Config);
}

void LiftXAudioProcessorEditor::styleTabButton(juce::TextButton& b, bool active)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? LiftColors::accentMaster.withAlpha(0.22f) : LiftColors::knobTrack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? LiftColors::text : LiftColors::textDim);
    b.repaint();
}

void LiftXAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(LiftColors::bg);

    // ヘッダー
    g.setColour(LiftColors::text);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText("LIFT-X", 16, 8, 140, 28, juce::Justification::centredLeft);

    g.setColour(LiftColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("RISER SYNTH  v" LIFTX_VERSION "  -  OTODESK",
               getWidth() - 260, 8, 244, 28, juce::Justification::centredRight);
}

void LiftXAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // ヘッダー行: タイトル(左) + LIFT MANUAL/AUTO + タブボタン(中央)
    auto header = r.removeFromTop(44);
    header.removeFromLeft(120);
    header.removeFromRight(30);
    header = header.withSizeKeepingCentre(112 + 12 + 6 * 84 + 5 * 6, 28);

    liftModeButton.setBounds(header.removeFromLeft(112));
    header.removeFromLeft(12);

    juce::TextButton* tabs[6] = { &mainTabButton, &oscEnvTabButton, &filterTabButton,
                                  &fxTabButton, &presetTabButton, &configTabButton };
    for (auto* b : tabs)
    {
        b->setBounds(header.removeFromLeft(84));
        header.removeFromLeft(6);
    }

    // パネル領域
    const auto panelArea = r.reduced(8, 4);
    mainPanel.setBounds(panelArea);
    oscEnvPanel.setBounds(panelArea);
    filterPanel.setBounds(panelArea);
    fxPanel.setBounds(panelArea);
    presetPanel.setBounds(panelArea);
    configPanel.setBounds(panelArea);
}
