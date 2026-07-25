// ==========================================
// File: FactoryPresets.h
// コード埋め込みファクトリープリセット (Riser/Downer 70種)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>

class LiftXAudioProcessor;

namespace FactoryPresets
{
    struct Item
    {
        const char* category;   // ジャンル (サブカテゴリとして表示)
        const char* name;
        const char* params;     // "id=value;id=value;..."
        const char* curves;     // "idx:x,y,c;x,y,c|idx:..." (CurveSnapshot::fromString形式)
    };

    const std::vector<Item>& items();
    int count();
    juce::String nameOf(int index);

    // Init後にパラメーター/カーブを適用する (メッセージスレッド専用)
    void apply(LiftXAudioProcessor& proc, int index);
}
