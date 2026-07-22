// ==========================================
// File: ColorPalette.h
// LIFT-X カラーパレット (ダークテーマ × パステルアクセント)
//  Granular の Midnight テーマをベースに調整。
//  ※ 表示のみのグローバル設定。オーディオ処理には一切関与しない。
// ==========================================
#pragma once

#include <JuceHeader.h>

namespace LiftColors
{
    // 背景・基本色 (ダークテーマ)
    inline const juce::Colour bg        { 0xff17141f };
    inline const juce::Colour panel     { 0xff201c2b };
    inline const juce::Colour panelLine { 0x22ffffff };
    inline const juce::Colour grid      { 0x14ffffff };
    inline const juce::Colour text      { 0xffe9e3f2 };
    inline const juce::Colour textDim   { 0xff8d86a0 };
    inline const juce::Colour knobTrack { 0xff2a2536 };

    // パステルアクセント (ダーク背景で映える明るいパステル)
    inline const juce::Colour mint      { 0xffb5ead7 };
    inline const juce::Colour pink      { 0xffffb7c5 };
    inline const juce::Colour lavender  { 0xffc7ceea };
    inline const juce::Colour peach     { 0xffffdac1 };
    inline const juce::Colour babyBlue  { 0xffaed9f7 };
    inline const juce::Colour sage      { 0xffe2f0cb };
    inline const juce::Colour rose      { 0xffffb7b2 };
    inline const juce::Colour lilac     { 0xffe0c3fc };

    // セクションアクセント
    inline const juce::Colour accentOsc    = mint;      // オシレーター
    inline const juce::Colour accentPitch  = lavender;  // ピッチカーブ
    inline const juce::Colour accentFilter = babyBlue;  // フィルターカーブ
    inline const juce::Colour accentFx     = peach;     // FX
    inline const juce::Colour accentMaster = pink;      // LIFT / マスター

    // カーブ番号 → アクセント色 (CurveStore::Index 順)
    inline juce::Colour curveAccent(int idx) noexcept
    {
        switch (idx)
        {
        case 0: case 1: case 2: return lavender;  // Pitch Osc1-3
        case 3:                 return lilac;     // Pitch Noise
        case 4: case 5: case 6: case 7: return babyBlue; // Filter1-4
        default:                return peach;     // FX
        }
    }
}
