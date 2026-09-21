// ==========================================
// File: PredicateComboBox.h
// 項目ごとの有効/無効 (非活性化/グレーアウト) を
// 述語関数 (setItemEnabledPredicate) で制御できる ComboBox
// Granular / Spectra8 の設計に準拠
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <functional>
#include "ColorPalette.h"

class PredicateComboBox : public juce::ComboBox
{
public:
    PredicateComboBox() = default;
    ~PredicateComboBox() override = default;

    void setItemEnabledPredicate(std::function<bool(int)> predicate)
    {
        mItemEnabledPredicate = std::move(predicate);
    }

    void showPopup() override
    {
        if (getNumItems() <= 0)
        {
            hidePopup();
            return;
        }

        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());

        const int selectedId = getSelectedId();
        for (int i = 0; i < getNumItems(); ++i)
        {
            const int id = getItemId(i);
            const bool tick = (id == selectedId);
            const bool enabled = (mItemEnabledPredicate == nullptr || mItemEnabledPredicate(id));

            juce::PopupMenu::Item item;
            item.text = getItemText(i);
            item.itemID = id;
            item.isEnabled = enabled;
            item.isTicked = tick;
            item.customComponent = new PredicateItem(*this, getItemText(i), tick, enabled);
            menu.addItem(std::move(item));
        }

        juce::Component::SafePointer<PredicateComboBox> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options()
                             .withTargetComponent(this)
                             .withMinimumWidth(getWidth())
                             .withStandardItemHeight(22),
            [safe](int result) mutable
            {
                if (auto* box = safe.getComponent())
                {
                    box->hidePopup();
                    if (result != 0)
                        box->setSelectedId(result, juce::sendNotificationSync);
                }
            });
    }

private:
    class PredicateItem : public juce::PopupMenu::CustomComponent
    {
    public:
        PredicateItem(juce::Component& ownerBox, juce::String itemText, bool ticked, bool enabled)
            : juce::PopupMenu::CustomComponent(enabled),
              mOwner(&ownerBox),
              mText(std::move(itemText)),
              mTicked(ticked),
              mEnabled(enabled)
        {
            setEnabled(enabled);
        }

        void getIdealSize(int& idealWidth, int& idealHeight) override
        {
            auto* box = mOwner.getComponent();
            const int ownerW = (box != nullptr) ? box->getWidth() : 0;
            idealWidth  = juce::jmax(150, ownerW);
            idealHeight = 22;
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds();

            if (isItemHighlighted() && mEnabled)
            {
                g.setColour(LiftColors::mint.withAlpha(0.22f));
                g.fillRect(r);
            }

            if (mTicked)
            {
                g.setColour(mEnabled ? LiftColors::mint : LiftColors::textDim.withAlpha(0.3f));
                g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
                g.drawText("*", r.removeFromLeft(18), juce::Justification::centred);
            }
            else
            {
                r.removeFromLeft(18);
            }

            if (!mEnabled)
            {
                g.setColour(LiftColors::textDim.withAlpha(0.28f));
                g.setFont(juce::Font(juce::FontOptions(12.5f)));
                g.drawText(mText, r.reduced(4, 0), juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour(isItemHighlighted() ? LiftColors::text : LiftColors::textDim);
                g.setFont(juce::Font(juce::FontOptions(12.5f)));
                g.drawText(mText, r.reduced(4, 0), juce::Justification::centredLeft, true);
            }
        }

    private:
        juce::Component::SafePointer<juce::Component> mOwner;
        juce::String mText;
        bool mTicked = false;
        bool mEnabled = true;
    };

    std::function<bool(int)> mItemEnabledPredicate;
};
