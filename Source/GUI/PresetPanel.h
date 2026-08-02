// ==========================================
// File: PresetPanel.h
// PRESETタブ: NextGenKick2のPresetBrowserを常設タブとして統合
//  - Factory 30種 (Riser/Downer, ジャンル別) + Userプリセット
//  - サブカテゴリ入力 / プリセット名入力 / ★お気に入り / 検索
// ==========================================
#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "../FactoryPresets.h"
#include "PresetBrowser.h"
#include "ColorPalette.h"

class PresetPanel : public juce::Component
{
public:
    // Closeボタン → エディタがMAINタブへ戻す
    std::function<void()> onClose;

    explicit PresetPanel(LiftXAudioProcessor& p)
        : proc(p), browser(LiftXAudioProcessor::getUserPresetDir())
    {
        // Factoryプリセット登録
        juce::Array<PresetBrowser::FactoryItem> items;
        const auto& facs = FactoryPresets::items();
        for (int i = 0; i < (int)facs.size(); ++i)
            items.add({ facs[(size_t)i].name, facs[(size_t)i].category, i });
        browser.setFactoryPresets(items);

        browser.onLoad = [this](const juce::File& f)
        {
            proc.loadUserPreset(f);
            updateCurrentLabel();
        };
        browser.onLoadFactory = [this](int idx)
        {
            proc.loadFactoryPreset(idx);
            updateCurrentLabel();
        };
        browser.onSave = [this](const juce::String& name, const juce::String& subCat)
        {
            proc.saveUserPreset(name, subCat);
            updateCurrentLabel();
        };
        browser.onInit = [this]
        {
            proc.initPreset();
            syncBrowserHighlight();   // Init は Factory/User どちらでもないため選択解除
            updateCurrentLabel();
        };
        browser.onClose = [this]
        {
            if (onClose != nullptr)
                onClose();   // MAINタブへ戻る
        };

        addAndMakeVisible(browser);

        currentLabel.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
        currentLabel.setColour(juce::Label::textColourId, LiftColors::text);
        currentLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(currentLabel);
        updateCurrentLabel();
    }

    void refresh()
    {
        browser.refresh();
        syncBrowserHighlight();
        updateCurrentLabel();
    }

    void paint(juce::Graphics& g) override
    {
        LiftColors::paintPanel(g, getLocalBounds().toFloat().reduced(2.0f));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(12, 10);
        currentLabel.setBounds(r.removeFromTop(22));
        r.removeFromTop(4);
        browser.setBounds(r);
    }

private:
    // プロセッサー側の「今読み込まれている実体」をブラウザのハイライトへ反映する。
    //  ヘッダーの ◀ ▶ でプリセットを送った場合やステート復元の直後は、
    //  ブラウザのクリックを経由しないため、これを呼ばないと選択表示が古いまま残る。
    void syncBrowserHighlight()
    {
        const int fi = proc.getCurrentFactoryIndex();
        if (fi >= 0) browser.setCurrentFactory(fi);
        else         browser.setCurrentFile(proc.getCurrentUserFile());
    }

    void updateCurrentLabel()
    {
        auto name = proc.getCurrentPresetName();
        if (name.isEmpty()) name = "-";
        currentLabel.setText("CURRENT PRESET:  " + name, juce::dontSendNotification);
    }

    LiftXAudioProcessor& proc;
    PresetBrowser browser;
    juce::Label currentLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetPanel)
};
