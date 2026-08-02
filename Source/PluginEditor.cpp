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

    // 全UIは content の子にする (content にスケール変換を掛けてリサイズを実現)
    addAndMakeVisible(content);
    content.onPaint  = [this](juce::Graphics& g) { paintContent(g); };
    content.onLayout = [this] { layoutContent(); };

    auto initTab = [this](juce::TextButton& b, Tab t)
    {
        b.onClick = [this, t] { setActiveTab(t); };
        content.addAndMakeVisible(b);
    };
    initTab(mainTabButton, Tab::Main);
    initTab(oscEnvTabButton, Tab::OscEnv);
    initTab(filterTabButton, Tab::Filter);
    initTab(fxTabButton, Tab::Fx);
    initTab(presetTabButton, Tab::Preset);
    initTab(configTabButton, Tab::Config);

    // LIFT: MANUAL/AUTO トグル (MAINタブの左)
    content.addAndMakeVisible(liftModeButton);
    liftModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "liftMode", liftModeButton);

    // プリセットナビゲーション (ヘッダー右)
    content.addAndMakeVisible(prevPresetButton);
    content.addAndMakeVisible(nextPresetButton);
    content.addAndMakeVisible(presetNameLabel);
    prevPresetButton.onClick = [this] { proc.stepPreset(-1); refreshAfterPresetChange(); };
    nextPresetButton.onClick = [this] { proc.stepPreset(1);  refreshAfterPresetChange(); };
    presetNameLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    presetNameLabel.setColour(juce::Label::textColourId, LiftColors::text);
    presetNameLabel.setJustificationType(juce::Justification::centred);
    // メーターの追従のため 2Hz → 24Hz へ (プリセット名の更新も兼ねる)
    startTimerHz(24);
    timerCallback();

    content.addChildComponent(mainPanel);
    content.addChildComponent(oscEnvPanel);
    content.addChildComponent(filterPanel);
    content.addChildComponent(fxPanel);
    content.addChildComponent(presetPanel);
    content.addChildComponent(configPanel);

    // PRESETタブのCloseボタン → MAINタブへ戻る
    presetPanel.onClose = [this] { setActiveTab(Tab::Main); };

    setActiveTab(Tab::Main);

    // ---- リサイズ (アスペクト比固定) ----
    //  設計サイズ 1020x640 を基準に、ウィンドウサイズとの比率を
    //  content の AffineTransform へ渡す。各パネルの resized() は
    //  常に基準サイズで動くため、レイアウトコードは一切変えなくてよい。
    constrainer.setFixedAspectRatio((double)kBaseW / (double)kBaseH);
    constrainer.setSizeLimits(kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(kBaseW, kBaseH);
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

    // ---- アウトプットメーター: 立ち上がりは即時、減衰はゆっくり ----
    bool meterChanged = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float peak = juce::jlimit(0.0f, 1.0f, proc.getOutPeak(ch));
        float& lvl = meterLevel[(size_t)ch];
        const float next = (peak > lvl) ? peak : lvl * 0.72f;
        if (std::abs(next - lvl) > 0.004f) { lvl = next; meterChanged = true; }
        else if (next <= 0.0f && lvl > 0.0f) { lvl = 0.0f; meterChanged = true; }
    }
    if (meterChanged)
        content.repaint(meterArea.expanded(2));   // meterArea は content 座標系
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

    // タブごとにセクション色を割り当てる。各パネル内のアクセント色と対応させると
    // 「今どのセクションにいるか」が色で分かるようになる。
    struct TabDef { juce::TextButton* btn; Tab tab; juce::Colour accent; };
    const TabDef defs[6] = {
        { &mainTabButton,   Tab::Main,   LiftColors::accentMaster },
        { &oscEnvTabButton, Tab::OscEnv, LiftColors::accentPitch  },
        { &filterTabButton, Tab::Filter, LiftColors::accentFilter },
        { &fxTabButton,     Tab::Fx,     LiftColors::accentFx     },
        { &presetTabButton, Tab::Preset, LiftColors::accentOsc    },
        { &configTabButton, Tab::Config, LiftColors::lilac        },
    };
    for (const auto& d : defs)
    {
        const bool on = (t == d.tab);
        styleTabButton(*d.btn, on, d.accent);
        if (on) { activeTabButton = d.btn; activeTabAccent = d.accent; }
    }

    repaint();   // アクティブタブの下線を描き直す
}

void LiftXAudioProcessorEditor::styleTabButton(juce::TextButton& b, bool active, juce::Colour accent)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? accent.withAlpha(0.20f) : LiftColors::knobTrack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? LiftColors::text : LiftColors::textDim);
    b.repaint();
}

void LiftXAudioProcessorEditor::paintContent(juce::Graphics& g)
{
    g.fillAll(LiftColors::bg);

    // ---- ヘッダー ----
    //  ロゴの左にセクション色のグラデーションバーを立てて「製品の顔」を作る
    {
        juce::ColourGradient bar(LiftColors::accentMaster, 0.0f, 8.0f,
                                 LiftColors::accentFilter, 0.0f, 36.0f, false);
        bar.addColour(0.5, LiftColors::accentOsc);
        g.setGradientFill(bar);
        g.fillRoundedRectangle(14.0f, 9.0f, 3.5f, 26.0f, 1.75f);
    }

    g.setColour(LiftColors::text);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText("LIFT-X", 26, 8, 100, 28, juce::Justification::centredLeft);

    // ヘッダー下の区切り線 (content 座標系なので基準幅を使う)
    g.setColour(LiftColors::panelLine);
    g.drawHorizontalLine(43, 8.0f, (float)kBaseW - 8.0f);

    // ---- アクティブタブの下線 ----
    if (activeTabButton != nullptr)
    {
        const auto b = activeTabButton->getBounds();
        g.setColour(activeTabAccent);
        g.fillRoundedRectangle((float)b.getX() + 2.0f, (float)b.getBottom() + 1.0f,
                               (float)b.getWidth() - 4.0f, 2.0f, 1.0f);
    }

    // ---- アウトプットメーター (ヘッダー右端) ----
    {
        const auto r = meterArea.toFloat();
        if (!r.isEmpty())
        {
            g.setColour(LiftColors::bg.darker(0.4f));
            g.fillRoundedRectangle(r, 2.0f);

            const float chW = (r.getWidth() - 2.0f) * 0.5f;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float lvl = juce::jlimit(0.0f, 1.0f, meterLevel[(size_t)ch]);
                if (lvl <= 0.001f) continue;

                const float h = r.getHeight() * lvl;
                const auto bar = juce::Rectangle<float>(
                    r.getX() + (chW + 2.0f) * (float)ch, r.getBottom() - h, chW, h);

                // 0.85 を超えたら警告色へ
                g.setColour(lvl > 0.85f ? LiftColors::rose : LiftColors::accentOsc);
                g.fillRoundedRectangle(bar, 1.5f);
            }
        }
    }
}

void LiftXAudioProcessorEditor::layoutContent()
{
    auto r = content.getLocalBounds();

    // ヘッダー行: ロゴ(左) → LIFT MANUAL/AUTO → タブ (全て左寄せ)
    //             右側: プリセットナビ (◀ 名前 ▶)
    auto header = r.removeFromTop(44);
    header.removeFromLeft(120);                 // ロゴ + アクセントバー分
    header = header.reduced(0, 8);

    // 右端: アウトプットメーター
    meterArea = header.removeFromRight(12).reduced(0, 2);
    header.removeFromRight(10);

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

// ==========================================================
// スケーリング (アスペクト比固定リサイズ)
// ==========================================================
void LiftXAudioProcessorEditor::paint(juce::Graphics& g)
{
    // content が全面を覆うが、変換の丸め誤差で1pxの隙間が出ることがあるため下地を塗る
    g.fillAll(LiftColors::bg);
}

void LiftXAudioProcessorEditor::resized()
{
    const float scale = juce::jmax(0.25f, (float)getWidth() / (float)kBaseW);
    content.setTransform(juce::AffineTransform::scale(scale));
    content.setBounds(0, 0, kBaseW, kBaseH);   // 変換前の論理サイズは常に基準サイズ
}
