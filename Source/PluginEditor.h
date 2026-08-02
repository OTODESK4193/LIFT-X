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
#include "GUI/OscEnvPanel.h"
#include "GUI/FilterPanel.h"
#include "GUI/FxPanel.h"
#include "GUI/PresetPanel.h"
#include "GUI/ConfigPanel.h"

// LIFT動作モードのトグル (押すたびに MANUAL ⇔ AUTO 表示が切り替わる)
//  MANUAL: LIFTノブは手動/DAWオートメーション
//  AUTO  : LIFTノブがProgressに連動して動的に動く
class LiftModeButton : public juce::Button
{
public:
    LiftModeButton() : juce::Button("liftMode")
    {
        setClickingTogglesState(true);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool) override
    {
        const bool on = getToggleState(); // true = AUTO
        auto r = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(on ? LiftColors::accentMaster
                       : (highlighted ? LiftColors::knobTrack.brighter(0.15f) : LiftColors::knobTrack));
        g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
        g.setColour(on ? LiftColors::accentMaster.brighter(0.4f) : LiftColors::panelLine);
        g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.2f);

        g.setColour(on ? LiftColors::bg : LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(on ? "LIFT: AUTO" : "LIFT: MANUAL",
                   getLocalBounds(), juce::Justification::centred);
    }
};

class LiftXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit LiftXAudioProcessorEditor(LiftXAudioProcessor&);
    ~LiftXAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class Tab { Main, OscEnv, Filter, Fx, Preset, Config };

    // ---- リサイズ対応 (アスペクト比固定スケーリング) ----
    //  設計上のUIサイズは常に kBaseW x kBaseH のまま扱い、実際のウィンドウ
    //  サイズとの比率を AffineTransform で内側コンテナへ掛ける。
    //  こうすることで各パネルの resized() を1行も書き換えずにリサイズできる。
    static constexpr int kBaseW = 1020;
    static constexpr int kBaseH = 640;

    // 全UIを載せる内側コンテナ (これにスケール変換を掛ける)。
    //  描画とレイアウトは親エディタ側の関数へ委譲する。
    struct ContentComponent : juce::Component
    {
        std::function<void(juce::Graphics&)> onPaint;
        std::function<void()> onLayout;
        // 子パネル (MainPanel) からカーブ変更を伝えるための経路
        std::function<void(int)> onCommand;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
        void resized() override { if (onLayout) onLayout(); }
        void handleCommandMessage(int id) override { if (onCommand) onCommand(id); }
    };

    ContentComponent content;
    juce::ComponentBoundsConstrainer constrainer;

    void paintContent(juce::Graphics& g);   // 旧 paint() の中身
    void layoutContent();                   // 旧 resized() の中身

    void timerCallback() override;

    void setActiveTab(Tab t);
    // ヘッダーの ◀ ▶ など、タブ操作を伴わないプリセット変更後の再同期
    void refreshAfterPresetChange();
    void styleTabButton(juce::TextButton& b, bool active, juce::Colour accent);

    // アクティブタブの下線描画用
    juce::TextButton* activeTabButton = nullptr;
    juce::Colour activeTabAccent { 0xffffb7c5 };

    // ヘッダー右端のアウトプットメーター
    juce::Rectangle<int> meterArea;
    std::array<float, 2> meterLevel { 0.0f, 0.0f };

    LiftXAudioProcessor& proc;
    ArcDialLookAndFeel lnf;

    juce::TextButton mainTabButton   { "MAIN" };
    juce::TextButton oscEnvTabButton { "OSC ENV" };
    juce::TextButton filterTabButton { "FILTER" };
    juce::TextButton fxTabButton     { "FX" };
    juce::TextButton presetTabButton { "PRESET" };
    juce::TextButton configTabButton { "CONFIG" };
    Tab activeTab = Tab::Main;

    LiftModeButton liftModeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> liftModeAtt;

    // ヘッダー右: プリセットナビゲーション (◀ 名前 ▶)
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };
    juce::Label presetNameLabel;

    MainPanel mainPanel;
    OscEnvPanel oscEnvPanel;
    FilterPanel filterPanel;
    FxPanel fxPanel;
    PresetPanel presetPanel;
    ConfigPanel configPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiftXAudioProcessorEditor)
};
