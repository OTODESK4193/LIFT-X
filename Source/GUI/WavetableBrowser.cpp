// ==========================================
// File: WavetableBrowser.cpp
// ==========================================
#include "WavetableBrowser.h"

WavetableBrowser::WavetableBrowser(LiftXAudioProcessor& p)
    : proc(p)
{
    mTitle.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    mTitle.setColour(juce::Label::textColourId, LiftColors::text);
    addAndMakeVisible(mTitle);

    for (auto* lb : { &mCatList, &mWtList })
    {
        lb->setColour(juce::ListBox::backgroundColourId, LiftColors::bg.brighter(0.04f));
        lb->setColour(juce::ListBox::outlineColourId, LiftColors::panelLine);
        lb->setOutlineThickness(1);
        lb->setRowHeight(22);
        addAndMakeVisible(*lb);
    }

    addAndMakeVisible(mBtnAddDir);
    addAndMakeVisible(mBtnFactory);
    addAndMakeVisible(mBtnRandom);
    addAndMakeVisible(mBtnClose);

    mBtnAddDir.onClick = [this] { chooseFolder(); };
    mBtnClose.onClick = [this] { close(); };
    mBtnRandom.onClick = [this] { loadRandomFor(targetOsc); };
    mBtnFactory.onClick = [this]
    {
        proc.clearCustomWavetable(targetOsc);
        if (onLoaded != nullptr) onLoaded();
        mWtList.deselectAllRows();
        repaint();
    };

    setVisible(false);
}

// ==========================================================
void WavetableBrowser::openFor(int oscIdx)
{
    targetOsc = juce::jlimit(0, RiserEngine::kNumOscs - 1, oscIdx);
    mTitle.setText("WAVETABLE BROWSER - OSC " + juce::String(targetOsc + 1),
                   juce::dontSendNotification);

    const juce::File dir(LiftXAudioProcessor::getGlobalWavetableDir());
    if (!dir.isDirectory())
    {
        chooseFolder();   // フォルダ未登録ならまず登録から
        return;
    }

    rescanFolder();
    setVisible(true);
    toFront(true);
}

void WavetableBrowser::close()
{
    setVisible(false);
}

// ==========================================================
void WavetableBrowser::chooseFolder()
{
    mChooser = std::make_unique<juce::FileChooser>(
        "Wavetableフォルダを選択 (サブフォルダ=カテゴリ)", juce::File());
    mChooser->launchAsync(juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                LiftXAudioProcessor::setGlobalWavetableDir(dir.getFullPathName());
                rescanFolder();
                setVisible(true);
                toFront(true);
            }
        });
}

// ==========================================================
void WavetableBrowser::rescanFolder()
{
    mCategories.clear();

    const juce::File dir(LiftXAudioProcessor::getGlobalWavetableDir());
    if (dir.isDirectory())
    {
        auto catFor = [this](const juce::String& key) -> WtCategory&
        {
            for (auto& c : mCategories)
                if (c.name == key)
                    return c;
            mCategories.push_back({ key, {} });
            return mCategories.back();
        };

        for (auto& f : dir.findChildFiles(juce::File::findFiles, true, "*.wav;*.aif;*.aiff"))
        {
            juce::String rel = f.getParentDirectory().getRelativePathFrom(dir);
            if (rel == ".")
                rel.clear();
            catFor(rel.isEmpty() ? dir.getFileName() : rel.replaceCharacter('\\', '/')).files.push_back(f);
        }

        std::sort(mCategories.begin(), mCategories.end(),
                  [](const WtCategory& a, const WtCategory& b)
                  { return a.name.compareIgnoreCase(b.name) < 0; });
        for (auto& c : mCategories)
            std::sort(c.files.begin(), c.files.end(),
                      [](const juce::File& a, const juce::File& b)
                      { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });
    }

    // ロード中ファイルの属するカテゴリを選択 (なければ先頭)
    mSelectedCat = 0;
    const juce::String cur = proc.getCustomWavetablePath(targetOsc);
    for (int c = 0; c < (int)mCategories.size(); ++c)
        for (auto& f : mCategories[(size_t)c].files)
            if (f.getFullPathName() == cur)
            {
                mSelectedCat = c;
                c = (int)mCategories.size();
                break;
            }

    mCatList.updateContent();
    mWtList.updateContent();
    if (!mCategories.empty())
        mCatList.selectRow(mSelectedCat);

    if (mSelectedCat < (int)mCategories.size())
    {
        const auto& files = mCategories[(size_t)mSelectedCat].files;
        for (int i = 0; i < (int)files.size(); ++i)
            if (files[(size_t)i].getFullPathName() == cur)
            {
                mWtList.selectRow(i);
                break;
            }
    }
}

// ==========================================================
void WavetableBrowser::setOscWaveToWavetable()
{
    // WAVEコンボを "Wavetable" (最終選択肢) へ
    if (auto* p = proc.apvts.getParameter("osc" + juce::String(targetOsc + 1) + "Wave"))
        p->setValueNotifyingHost(1.0f);
}

void WavetableBrowser::loadFileAt(int catIdx, int fileIdx)
{
    if (catIdx < 0 || catIdx >= (int)mCategories.size())
        return;
    const auto& files = mCategories[(size_t)catIdx].files;
    if (fileIdx < 0 || fileIdx >= (int)files.size())
        return;

    if (proc.loadCustomWavetable(targetOsc, files[(size_t)fileIdx]))
    {
        setOscWaveToWavetable();
        if (onLoaded != nullptr) onLoaded();
        mWtList.repaint();
    }
}

void WavetableBrowser::loadRandomFor(int oscIdx)
{
    targetOsc = juce::jlimit(0, RiserEngine::kNumOscs - 1, oscIdx);

    if (mCategories.empty())
        rescanFolder();

    int total = 0;
    for (auto& c : mCategories)
        total += (int)c.files.size();
    if (total <= 0)
        return;

    int pick = mRng.nextInt(total);
    for (int c = 0; c < (int)mCategories.size(); ++c)
    {
        const int n = (int)mCategories[(size_t)c].files.size();
        if (pick < n)
        {
            mSelectedCat = c;
            if (isVisible())
            {
                mCatList.selectRow(c);
                mWtList.updateContent();
                mWtList.selectRow(pick);
            }
            loadFileAt(c, pick);
            return;
        }
        pick -= n;
    }
}

// ==========================================================
// リストモデル
// ==========================================================
void WavetableBrowser::CatModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int)owner.mCategories.size()) return;
    if (selected)
    {
        g.setColour(LiftColors::mint.withAlpha(0.18f));
        g.fillRect(0, 0, w, h);
    }
    g.setColour(selected ? LiftColors::text : LiftColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(owner.mCategories[(size_t)row].name, 8, 0, w - 12, h,
               juce::Justification::centredLeft);
}

void WavetableBrowser::CatModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    owner.mSelectedCat = row;
    owner.mWtList.updateContent();
    owner.mWtList.deselectAllRows();
    owner.mWtList.repaint();
}

int WavetableBrowser::FileModel::getNumRows()
{
    if (owner.mSelectedCat < 0 || owner.mSelectedCat >= (int)owner.mCategories.size())
        return 0;
    return (int)owner.mCategories[(size_t)owner.mSelectedCat].files.size();
}

void WavetableBrowser::FileModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (owner.mSelectedCat < 0 || owner.mSelectedCat >= (int)owner.mCategories.size()) return;
    const auto& files = owner.mCategories[(size_t)owner.mSelectedCat].files;
    if (row < 0 || row >= (int)files.size()) return;

    const bool isLoaded = files[(size_t)row].getFullPathName()
                        == owner.proc.getCustomWavetablePath(owner.targetOsc);
    if (selected || isLoaded)
    {
        g.setColour((isLoaded ? LiftColors::peach : LiftColors::mint).withAlpha(0.18f));
        g.fillRect(0, 0, w, h);
    }
    g.setColour(selected || isLoaded ? LiftColors::text : LiftColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(files[(size_t)row].getFileNameWithoutExtension(), 8, 0, w - 12, h,
               juce::Justification::centredLeft);
}

void WavetableBrowser::FileModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    owner.loadFileAt(owner.mSelectedCat, row);
}

// ==========================================================
void WavetableBrowser::paint(juce::Graphics& g)
{
    g.setColour(LiftColors::bg.withAlpha(0.96f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(LiftColors::panelLine);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.2f);
}

void WavetableBrowser::resized()
{
    auto r = getLocalBounds().reduced(14, 10);

    auto top = r.removeFromTop(28);
    mBtnClose.setBounds(top.removeFromRight(80));
    top.removeFromRight(6);
    mBtnRandom.setBounds(top.removeFromRight(86));
    top.removeFromRight(6);
    mBtnFactory.setBounds(top.removeFromRight(86));
    top.removeFromRight(6);
    mBtnAddDir.setBounds(top.removeFromRight(86));
    mTitle.setBounds(top);

    r.removeFromTop(8);
    mCatList.setBounds(r.removeFromLeft(r.getWidth() / 3));
    r.removeFromLeft(8);
    mWtList.setBounds(r);
}
