// ==========================================
// File: WavetableBrowser.h
// カスタムWavetableブラウザ (SPECTRA8方式を移植 / OSC毎ターゲット対応)
//
//  - 登録フォルダはグローバル設定へ永続化 (一度設定すれば保持)
//  - サブフォルダ = カテゴリとして左ペインに表示、右ペインでファイル選択
//  - RANDOM: 全カテゴリから一様ランダムに1つロード
//  - FACTORY: カスタムWTを解除しビルトイン波形へ戻す
//  - MainPanel の上にオーバーレイ表示される
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "ColorPalette.h"

class WavetableBrowser : public juce::Component
{
public:
    explicit WavetableBrowser(LiftXAudioProcessor& p);

    // 対象OSCを指定して開く (フォルダ未登録なら登録ダイアログへ)
    void openFor(int oscIdx);
    void close();

    // RNDボタン用: ブラウザを開かずランダムロード
    void loadRandomFor(int oscIdx);

    // ロード完了通知 (MainPanelが波形表示等を更新)
    std::function<void()> onLoaded;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct WtCategory
    {
        juce::String name;
        std::vector<juce::File> files;
    };

    class CatModel : public juce::ListBoxModel
    {
    public:
        explicit CatModel(WavetableBrowser& o) : owner(o) {}
        int getNumRows() override { return (int)owner.mCategories.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    private:
        WavetableBrowser& owner;
    };

    class FileModel : public juce::ListBoxModel
    {
    public:
        explicit FileModel(WavetableBrowser& o) : owner(o) {}
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    private:
        WavetableBrowser& owner;
    };

    void rescanFolder();
    void chooseFolder();
    void loadFileAt(int catIdx, int fileIdx);
    void setOscWaveToWavetable();

    LiftXAudioProcessor& proc;
    int targetOsc = 0;

    std::vector<WtCategory> mCategories;
    int mSelectedCat = 0;

    CatModel catModel { *this };
    FileModel fileModel { *this };
    juce::ListBox mCatList { "cats", &catModel };
    juce::ListBox mWtList { "files", &fileModel };

    juce::Label mTitle;
    juce::TextButton mBtnAddDir { "ADD DIR" };
    juce::TextButton mBtnFactory { "FACTORY" };
    juce::TextButton mBtnRandom { "RANDOM" };
    juce::TextButton mBtnClose { "CLOSE" };

    std::unique_ptr<juce::FileChooser> mChooser;
    juce::Random mRng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableBrowser)
};
