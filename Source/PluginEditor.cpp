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

    // プリセットナビゲーション (ヘッダー右)
    addAndMakeVisible(prevPresetButton);
    addAndMakeVisible(nextPresetButton);
    addAndMakeVisible(presetNameLabel);
    prevPresetButton.onClick = [this] { proc.stepPreset(-1); refreshAfterPresetChange(); };
    nextPresetButton.onClick = [this] { proc.stepPreset(1);  refreshAfterPresetChange(); };
    presetNameLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    presetNameLabel.setColour(juce::Label::textColourId, LiftColors::text);
    presetNameLabel.setJustificationType(juce::Justification::centred);
    startTimerHz(2);
    timerCallback();

    addChildComponent(mainPanel);
    addChildComponent(oscEnvPanel);
    addChildComponent(filterPanel);
    addChildComponent(fxPanel);
    addChildComponent(presetPanel);
    addChildComponent(configPanel);

    // PRESETタブのCloseボタン → MAINタブへ戻る
    presetPanel.onClose = [this] { setActiveTab(Tab::Main); };

    setActiveTab(Tab::Main);
    setSize(1020, 640);
}

LiftXAudioProcessorEditor::~LiftXAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void LiftXAudioProcessorEditor::timerCallback()
{
    auto name = proc.getCurrentPresetName();
    if (name.isEmpty()) name = "-";
    if (presetNameLabel.getText() != name)
        presetNameLabel.setText(name, juce::dontSendNotification);
}

// ==========================================================
// プリセットが「タブ操作以外の経路」で切り替わったときの再同期。
//  ヘッダーの ◀ ▶ はタブを切り替えないため、これを呼ばないと
//   ・PRESETタブのリストのハイライトが古いまま
//   ・OSC ENV / FILTER / FX のカーブエディタが前のプリセットのカーブを表示したまま
//  になる。カーブは CurveStore が真実の源なので、各パネルに読み直させる。
//  (MAINタブは30Hzタイマーが波形表示とキー表示を自動で追従させるため対象外)
// ==========================================================
void LiftXAudioProcessorEditor::refreshAfterPresetChange()
{
    oscEnvPanel.refresh();
    filterPanel.refresh();
    fxPanel.refresh();
    presetPanel.refresh();
    timerCallback();          // ヘッダーのプリセット名
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

    // ヘッダー (バージョン情報はCONFIGタブへ移動)
    g.setColour(LiftColors::text);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText("LIFT-X", 16, 8, 100, 28, juce::Justification::centredLeft);
}

void LiftXAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // ヘッダー行: ロゴ(左) → LIFT MANUAL/AUTO → タブ (全て左寄せ)
    //             右側: プリセットナビ (◀ 名前 ▶)
    auto header = r.removeFromTop(44);
    header.removeFromLeft(112);                 // ロゴ分
    header = header.reduced(0, 8);

    // 右: プリセットナビ
    auto nav = header.removeFromRight(240);
    prevPresetButton.setBounds(nav.removeFromLeft(26));
    nextPresetButton.setBounds(nav.removeFromRight(26));
    presetNameLabel.setBounds(nav.reduced(4, 0));

    liftModeButton.setBounds(header.removeFromLeft(108));
    header.removeFromLeft(10);

    juce::TextButton* tabs[6] = { &mainTabButton, &oscEnvTabButton, &filterTabButton,
                                  &fxTabButton, &presetTabButton, &configTabButton };
    for (auto* b : tabs)
    {
        b->setBounds(header.removeFromLeft(78));
        header.removeFromLeft(5);
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
