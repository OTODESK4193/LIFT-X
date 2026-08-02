#pragma once
#include <JuceHeader.h>
#include "ColorPalette.h"

// ============================================================================
//  PresetBrowser  -  3カラムのプリセットブラウザ
//   ・左:   カテゴリ（All / User / Favorites）
//   ・中央: サブカテゴリ（保存時に作られるフォルダ）
//   ・右:   検索ボックス＋プリセット一覧（★お気に入り / シングルクリックで即読込）
//   ・下:   プリセット名入力 ＋ サブカテゴリ入力 ＋ Save / Init / Close
//
//  プリセットは presetDir 以下の *.xml。サブカテゴリ＝サブフォルダ名。
// ============================================================================
class PresetBrowser : public juce::Component
{
public:
    std::function<void(const juce::File&)> onLoad;   // シングルクリックで読込
    std::function<void()> onInit;                    // Init / Default
    std::function<void(const juce::String& name, const juce::String& subCat)> onSave;
    std::function<void()> onClose;
    std::function<void(int)> onLoadFactory;          // Factory プリセットを index で読込

    // Factory プリセット（コード埋め込み）を登録
    struct FactoryItem { juce::String name, category; int index = 0; };
    void setFactoryPresets(const juce::Array<FactoryItem>& items) { factoryAll = items; refresh(); }

    explicit PresetBrowser(juce::File presetDir) : rootDir(presetDir)
    {
        catModel.owner = this; subModel.owner = this; fileModel.owner = this;
        categories = { "All", "Factory", "User", "Favorites" };

        catList.setModel(&catModel); subList.setModel(&subModel); fileList.setModel(&fileModel);
        catList.setRowHeight(38); subList.setRowHeight(34); fileList.setRowHeight(30);
        catList.setColour(juce::ListBox::backgroundColourId, LiftColors::bg);
        subList.setColour(juce::ListBox::backgroundColourId, LiftColors::bg.brighter(0.03f));
        fileList.setColour(juce::ListBox::backgroundColourId, LiftColors::bg.brighter(0.06f));
        for (auto* l : { &catList, &subList, &fileList }) addAndMakeVisible(*l);

        auto styleEdit = [](juce::TextEditor& e, const juce::String& hint) {
            e.setColour(juce::TextEditor::backgroundColourId, LiftColors::bg);
            e.setColour(juce::TextEditor::textColourId, LiftColors::text);
            e.setColour(juce::TextEditor::outlineColourId, LiftColors::panelLine);
            e.setTextToShowWhenEmpty(hint, LiftColors::textDim);
        };
        addAndMakeVisible(searchBox); styleEdit(searchBox, "Search...");
        searchBox.onTextChange = [this] { updateFiles(); };
        addAndMakeVisible(nameBox);   styleEdit(nameBox, "Preset name");
        addAndMakeVisible(subBox);    styleEdit(subBox, "Subcategory (optional)");

        auto styleBtn = [](juce::TextButton& b, juce::Colour on) {
            b.setColour(juce::TextButton::buttonColourId, LiftColors::knobTrack);
            b.setColour(juce::TextButton::buttonOnColourId, on);
        };
        addAndMakeVisible(saveBtn);  saveBtn.setButtonText("Save");  styleBtn(saveBtn, LiftColors::mint);
        addAndMakeVisible(initBtn);  initBtn.setButtonText("Init");  styleBtn(initBtn, LiftColors::lavender);
        addAndMakeVisible(closeBtn); closeBtn.setButtonText("Close"); styleBtn(closeBtn, LiftColors::rose);
        saveBtn.onClick  = [this] { doSave(); };
        initBtn.onClick  = [this] { if (onInit) onInit(); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };

        refresh();
    }

    void refresh() { scan(); updateSubCategories(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(LiftColors::panel);
        g.setColour(LiftColors::panelLine);
        g.drawRect(getLocalBounds(), 1);
        g.setColour(LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("CATEGORY", colX(0) + 8, 4, 120, 14, juce::Justification::left);
        g.drawText("SUBCATEGORY", colX(1) + 8, 4, 160, 14, juce::Justification::left);
        g.drawText("PRESETS", colX(2) + 8, 4, 120, 14, juce::Justification::left);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4);
        area.removeFromTop(18); // 見出し
        auto bottom = area.removeFromBottom(34);
        area.removeFromBottom(4);

        // 下部バー: [name ........] [sub ....] [Save][Init][Close]
        closeBtn.setBounds(bottom.removeFromRight(66).reduced(2));
        initBtn.setBounds(bottom.removeFromRight(66).reduced(2));
        saveBtn.setBounds(bottom.removeFromRight(66).reduced(2));
        subBox.setBounds(bottom.removeFromRight(170).reduced(2));
        nameBox.setBounds(bottom.reduced(2));

        const int w = area.getWidth() / 4;
        catW = w; subW = w;
        catList.setBounds(area.removeFromLeft(w).reduced(2));
        subList.setBounds(area.removeFromLeft(w).reduced(2));
        searchBox.setBounds(area.removeFromTop(30).reduced(2));
        fileList.setBounds(area.reduced(2));
    }

private:
    int colX(int i) const { return i == 0 ? 4 : i == 1 ? 4 + getWidth() / 4 : 4 + getWidth() / 2; }

    juce::File rootDir;
    juce::ListBox catList{ "cat", nullptr }, subList{ "sub", nullptr }, fileList{ "file", nullptr };
    juce::TextEditor searchBox, nameBox, subBox;
    juce::TextButton saveBtn, initBtn, closeBtn;
    int catW = 100, subW = 100;

    juce::StringArray categories, subCategories, favorites, pendingRemovals;
    int selCat = 0, selSub = 0;
    juce::File currentFile;   // 現在読込中のユーザープリセット（ハイライト用）
    int currentFactoryIndex = -1;   // 現在読込中のFactoryプリセット index（ハイライト用）

public:
    // 外部（エディタ）から現在のプリセットを設定してハイライト同期
    void setCurrentFile(const juce::File& f)
    {
        currentFile = f; currentFactoryIndex = -1; revealCurrent();
    }
    void setCurrentFactory(int idx)
    {
        currentFactoryIndex = idx; currentFile = juce::File(); revealCurrent();
    }

private:

    struct PItem { juce::File file; juce::String name; juce::String subCat; };
    juce::Array<PItem> allPresets, currentList;

    juce::Array<FactoryItem> factoryAll, factoryCur;   // Factory プリセット（全体／表示中）
    bool allMode = false;   // "All" カテゴリ（Factory＋User を併記）
    bool isFactory() const { return selCat >= 0 && selCat < categories.size() && categories[selCat] == "Factory"; }
    bool showsFactory() const { return isFactory() || allMode; }
    int  factoryRowCount() const { return factoryCur.size(); }

    juce::File favFile() const { return rootDir.getChildFile("_favorites.txt"); }
    void loadFavorites()
    {
        favorites.clear();
        auto f = favFile();
        if (f.existsAsFile()) { favorites.addLines(f.loadFileAsString()); favorites.removeEmptyStrings(); }
    }
    void saveFavorites()
    {
        // 最新のファイルを読み込んで、他インスタンスの追加分とマージする
        juce::StringArray latestFavs;
        auto f = favFile();
        if (f.existsAsFile())
        {
            latestFavs.addLines(f.loadFileAsString());
            latestFavs.removeEmptyStrings();
        }
        
        // このインスタンスで明示的に解除されたお気に入りをファイルからも除去
        for (auto& rem : pendingRemovals)
            latestFavs.removeString(rem);
        
        // メモリ上のお気に入りを最新状態に追加マージ（重複は排除）
        for (auto& fav : favorites)
        {
            if (!latestFavs.contains(fav))
                latestFavs.add(fav);
        }
        
        // 保存
        f.replaceWithText(latestFavs.joinIntoString("\n"), false, false, "\n");
        
        // メモリ上の変数も同期
        favorites = latestFavs;
        pendingRemovals.clear();
    }

    juce::String factoryFavId(int index) const { return "factory::" + juce::String(index); }
    bool isFactoryFavorite(int index) const { return favorites.contains(factoryFavId(index)); }

    void scan()
    {
        allPresets.clear();
        if (rootDir.isDirectory())
        {
            auto files = rootDir.findChildFiles(juce::File::findFiles, true, "*.xml");
            for (auto& f : files)
            {
                juce::String sub = "Uncategorized";
                auto parent = f.getParentDirectory();
                if (parent != rootDir) sub = parent.getFileName();
                allPresets.add({ f, f.getFileNameWithoutExtension(), sub });
            }
        }
        loadFavorites();
    }

    void updateSubCategories()
    {
        subCategories.clear();
        subCategories.add("All");
        const juce::String sc = categories[selCat];
        if (isFactory() || sc == "All" || sc == "Favorites")
            for (auto& it : factoryAll) {
                if (sc == "Favorites" && !isFactoryFavorite(it.index)) continue;
                if (!subCategories.contains(it.category)) subCategories.add(it.category);
            }
        if (!isFactory())
            for (auto& p : allPresets) {
                if (sc == "Favorites" && !favorites.contains(p.file.getFullPathName())) continue;
                if (!subCategories.contains(p.subCat)) subCategories.add(p.subCat);
            }
        selSub = 0;
        subList.updateContent();
        updateFiles();
    }

    void updateFiles()
    {
        currentList.clear();
        factoryCur.clear();
        const juce::String c = categories[selCat];
        allMode = (c == "All");
        const juce::String s = (selSub >= 0 && selSub < subCategories.size()) ? subCategories[selSub] : "All";
        const juce::String q = searchBox.getText().trim().toLowerCase();
        auto match = [&](const juce::String& n) { return q.isEmpty() || n.toLowerCase().contains(q); };

        // Factory（"All" / "Factory" / "Favorites" のとき）
        if (showsFactory() || c == "Favorites")
            for (auto& it : factoryAll) {
                if (c == "Favorites" && !isFactoryFavorite(it.index)) continue;
                if (s != "All" && it.category != s) continue;
                if (!match(it.name)) continue;
                factoryCur.add(it);
            }
        // User ファイル（"Factory" 以外）
        if (!isFactory())
            for (auto& p : allPresets) {
                if (c == "Favorites" && !favorites.contains(p.file.getFullPathName())) continue;
                if (s != "All" && p.subCat != s) continue;
                if (!match(p.name)) continue;
                currentList.add(p);
            }
        fileList.updateContent();
        fileList.repaint();
    }

    // 現在のプリセットが今の絞り込み結果に含まれていれば、その行が見えるまでスクロールする。
    //  ヘッダーの ◀ ▶ で順送りしたとき、選択行が画面外にあるとハイライトが
    //  見えないままになるため。含まれていない場合 (カテゴリ絞り込みで隠れている等) は
    //  再描画のみ行う。
    void revealCurrent()
    {
        const int fc = factoryRowCount();
        int row = -1;

        if (currentFactoryIndex >= 0)
        {
            for (int i = 0; i < fc; ++i)
                if (factoryCur.getReference(i).index == currentFactoryIndex) { row = i; break; }
        }
        else if (currentFile != juce::File())
        {
            for (int i = 0; i < currentList.size(); ++i)
                if (currentList.getReference(i).file == currentFile) { row = fc + i; break; }
        }

        if (row >= 0)
            fileList.scrollToEnsureRowIsOnscreen(row);
        fileList.repaint();
    }

    void doSave()
    {
        if (!onSave) return;
        juce::String name = nameBox.getText().trim();
        if (name.isEmpty()) name = "Preset";
        onSave(name, subBox.getText().trim());
        refresh();
    }

    // 右クリックメニュー（Delete）
    void showItemMenu(juce::File file)
    {
        juce::PopupMenu m;
        m.addItem(1, "Delete \"" + file.getFileNameWithoutExtension() + "\"");
        m.showMenuAsync(juce::PopupMenu::Options(),
            [this, file](int result)
            {
                if (result != 1) return;
                juce::NativeMessageBox::showYesNoBox(juce::AlertWindow::WarningIcon,
                    "Delete Preset",
                    "Delete \"" + file.getFileNameWithoutExtension() + "\" permanently?",
                    nullptr,
                    juce::ModalCallbackFunction::create([this, file](int yes)
                    {
                        if (yes != 1) return;
                        auto id = file.getFullPathName();
                        if (favorites.contains(id)) { favorites.removeString(id); saveFavorites(); }
                        if (currentFile == file) currentFile = juce::File();
                        file.deleteFile();
                        refresh();
                    }));
            });
    }

    // --- ListBox Models ---
    struct CatM : juce::ListBoxModel {
        PresetBrowser* owner = nullptr;
        int getNumRows() override { return owner ? owner->categories.size() : 0; }
        void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override {
            if (!owner) return; bool a = (r == owner->selCat);
            if (a) g.fillAll(LiftColors::accentMaster.withAlpha(0.18f));
            g.setColour(a ? LiftColors::text : LiftColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
            g.drawText(owner->categories[r], 12, 0, w - 20, h, juce::Justification::centredLeft);
        }
        void listBoxItemClicked(int r, const juce::MouseEvent&) override {
            if (owner) { owner->selCat = r; owner->updateSubCategories(); }
        }
    } catModel;

    struct SubM : juce::ListBoxModel {
        PresetBrowser* owner = nullptr;
        int getNumRows() override { return owner ? owner->subCategories.size() : 0; }
        void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override {
            if (!owner) return; bool a = (r == owner->selSub);
            if (a) g.fillAll(LiftColors::accentMaster.withAlpha(0.14f));
            g.setColour(a ? LiftColors::text : LiftColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(15.0f)));
            g.drawText(owner->subCategories[r], 12, 0, w - 20, h, juce::Justification::centredLeft);
        }
        void listBoxItemClicked(int r, const juce::MouseEvent&) override {
            if (owner) { owner->selSub = r; owner->updateFiles(); }
        }
    } subModel;

    struct FileM : juce::ListBoxModel {
        PresetBrowser* owner = nullptr;
        int getNumRows() override {
            if (!owner) return 0;
            return owner->factoryRowCount() + owner->currentList.size();
        }
        void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override {
            if (!owner) return;
            const int fc = owner->factoryRowCount();
            if (r < fc) {   // Factory 行: ★お気に入りトグル対応
                auto& fi = owner->factoryCur.getReference(r);
                bool sel = (fi.index == owner->currentFactoryIndex);
                if (sel) {
                    g.fillAll(LiftColors::accentMaster.withAlpha(0.16f));
                    g.setColour(LiftColors::accentMaster);
                    g.fillRect(0, 0, 3, h);
                }
                bool fav = owner->isFactoryFavorite(fi.index);
                g.setColour(fav ? LiftColors::peach : LiftColors::textDim.withAlpha(0.45f));
                g.setFont(juce::Font(juce::FontOptions(18.0f)));
                g.drawText(fav ? juce::String::fromUTF8("\xE2\x98\x85") : juce::String::fromUTF8("\xE2\x98\x86"),
                           6, 0, 22, h, juce::Justification::centred);
                g.setColour(sel ? LiftColors::text : LiftColors::textDim);
                g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
                g.drawText(juce::String::fromUTF8("\xE2\x97\x86 ") + fi.name + "    [" + fi.category + "]",
                           32, 0, w - 38, h, juce::Justification::centredLeft);
                return;
            }
            const int ui = r - fc;
            if (ui < 0 || ui >= owner->currentList.size()) return;
            auto& it = owner->currentList.getReference(ui);
            bool sel = (it.file == owner->currentFile);
            if (sel) {   // 選択中プリセットをハイライト（左に縦アクセントバー＋背景）
                g.fillAll(LiftColors::accentMaster.withAlpha(0.16f));
                g.setColour(LiftColors::accentMaster);
                g.fillRect(0, 0, 3, h);
            }
            bool fav = owner->favorites.contains(it.file.getFullPathName());
            g.setColour(fav ? LiftColors::peach : LiftColors::textDim.withAlpha(0.45f));
            g.setFont(juce::Font(juce::FontOptions(18.0f)));
            g.drawText(fav ? juce::String::fromUTF8("\xE2\x98\x85") : juce::String::fromUTF8("\xE2\x98\x86"),
                       6, 0, 22, h, juce::Justification::centred);
            g.setColour(sel ? LiftColors::text : LiftColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(14.0f, sel ? juce::Font::bold : juce::Font::plain)));
            juce::String label = it.name;
            if (it.subCat.isNotEmpty() && it.subCat != "Uncategorized") label += "    [" + it.subCat + "]";
            g.drawText(label, 32, 0, w - 38, h, juce::Justification::centredLeft);
        }
        void listBoxItemClicked(int r, const juce::MouseEvent& e) override {
            if (!owner) return;
            const int fc = owner->factoryRowCount();
            if (r < fc) {   // Factory 行: ★トグル or シングルクリックで index 読込
                auto& fi = owner->factoryCur.getReference(r);
                if (e.x < 30) {  // ★トグル
                    auto id = owner->factoryFavId(fi.index);
                    if (owner->favorites.contains(id)) {
                        owner->favorites.removeString(id);
                        owner->pendingRemovals.add(id);
                    }
                    else owner->favorites.add(id);
                    owner->saveFavorites();
                    if (owner->categories[owner->selCat] == "Favorites") owner->updateFiles();
                    else owner->fileList.repaintRow(r);
                }
                else {  // シングルクリックで即読込
                    owner->currentFactoryIndex = fi.index;
                    owner->currentFile = juce::File();
                    if (owner->onLoadFactory) owner->onLoadFactory(fi.index);
                    owner->fileList.repaint();
                }
                return;
            }
            const int ui = r - fc;
            if (ui < 0 || ui >= owner->currentList.size()) return;
            auto& it = owner->currentList.getReference(ui);
            if (e.mods.isPopupMenu()) { owner->showItemMenu(it.file); return; }  // 右クリック→Deleteメニュー
            if (e.x < 30) { // ★トグル
                auto id = it.file.getFullPathName();
                if (owner->favorites.contains(id)) {
                    owner->favorites.removeString(id);
                    owner->pendingRemovals.add(id);
                }
                else owner->favorites.add(id);
                owner->saveFavorites();
                if (owner->categories[owner->selCat] == "Favorites") owner->updateFiles();
                else owner->fileList.repaintRow(r);
            }
            else { // シングルクリックで即読込
                owner->currentFile = it.file;   // ハイライト更新
                owner->currentFactoryIndex = -1;
                if (owner->onLoad) owner->onLoad(it.file);
                owner->fileList.repaint();
            }
        }
    } fileModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
