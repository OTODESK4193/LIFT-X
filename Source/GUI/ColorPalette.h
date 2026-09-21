// ==========================================
// File: ColorPalette.h
// LIFT-X カラーパレット (ダーク × パステル / テーマ切替対応)
//  Granular の10テーマを移植。テーマはグローバル設定に保存され、
//  createEditor 時に適用される (変更後はウィンドウを開き直すと完全適用)。
//  ※ 表示のみのグローバル設定。オーディオ処理には一切関与しない。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>

// ============================================================================
//  埋め込みフォント
//   ・UI全般   : Inter          (Regular / Bold)
//   ・数値表示 : JetBrains Mono (Regular / Bold)
//  どちらも SIL OFL 1.1。Source/Assets/Fonts/ に実体、ライセンス全文も同梱。
//
//  UI全般の差し替えは ArcDialLookAndFeel::getTypefaceForFont() が行うため、
//  既存の juce::FontOptions(size, bold) という呼び出しは1箇所も変えずに
//  Inter へ切り替わる。等幅にしたい箇所だけ LiftFonts::mono() を使う。
//
//  ※ 全パネルが include する ColorPalette.h に置いてある。
//    (以前 ArcDial.h にあったが、ArcDial.h を include していないパネルから
//     参照できずビルドが通らなかった)
// ============================================================================
namespace LiftFonts
{
    // getTypefaceForFont() がこの名前を見て JetBrains Mono を返す
    inline const char* kMonoName = "LIFTX Mono";

    inline juce::Font mono(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(kMonoName, height,
                                            bold ? juce::Font::bold : juce::Font::plain));
    }
}

namespace LiftColors
{
    // 背景・基本色 (テーマで書き換わる)
    inline juce::Colour bg        { 0xff17141f };
    inline juce::Colour panel     { 0xff201c2b };
    inline juce::Colour panelLine { 0x22ffffff };
    inline juce::Colour grid      { 0x14ffffff };
    inline juce::Colour text      { 0xffe9e3f2 };
    inline juce::Colour textDim   { 0xff8d86a0 };
    inline juce::Colour knobTrack { 0xff2a2536 };

    // パステルパレット (テーマで書き換わる)
    inline juce::Colour mint      { 0xffb5ead7 };
    inline juce::Colour pink      { 0xffffb7c5 };
    inline juce::Colour lavender  { 0xffc7ceea };
    inline juce::Colour peach     { 0xffffdac1 };
    inline juce::Colour babyBlue  { 0xffaed9f7 };
    inline juce::Colour sage      { 0xffe2f0cb };
    inline juce::Colour rose      { 0xffffb7b2 };
    inline juce::Colour lilac     { 0xffe0c3fc };

    // セクションアクセント (テーマ適用時に再計算)
    inline juce::Colour accentOsc    { 0xffb5ead7 };
    inline juce::Colour accentPitch  { 0xffc7ceea };
    inline juce::Colour accentFilter { 0xffaed9f7 };
    inline juce::Colour accentFx     { 0xffffdac1 };
    inline juce::Colour accentMaster { 0xffffb7c5 };

    // --- テーマ定義 (Granular移植) ---
    struct Theme
    {
        juce::uint32 bg, panel, text, textDim, knobTrack;
        juce::uint32 mint, pink, lavender, peach, babyBlue, sage, rose, lilac;
    };

    inline const std::array<Theme, 10>& themes()
    {
        static const std::array<Theme, 10> t = { {
            // 0 Midnight (default)
            { 0xff17141f,0xff201c2b,0xffe9e3f2,0xff8d86a0,0xff2a2536,
              0xffb5ead7,0xffffb7c5,0xffc7ceea,0xffffdac1,0xffaed9f7,0xffe2f0cb,0xffffb7b2,0xffe0c3fc },
            // 1 Sakura (warm pink)
            { 0xff1e1418,0xff2b1c22,0xfff3e6ec,0xffa2888f,0xff36272e,
              0xfff7c9d8,0xffff9db4,0xffe6c8ee,0xffffcdb8,0xfff5b8cf,0xfff0dccb,0xffffb0ad,0xfff0c3e6 },
            // 2 Ocean (teal / blue)
            { 0xff0f1720,0xff17222f,0xffdce9f2,0xff7f93a2,0xff203039,
              0xff9fe8da,0xff88cfe0,0xffa8c7ea,0xffbfe0d2,0xff8fc9f7,0xffbfe8e0,0xff9fd2d8,0xffaed0f0 },
            // 3 Forest (green)
            { 0xff121a14,0xff1b2620,0xffe4f0e6,0xff85988c,0xff26332c,
              0xffb7ead0,0xffd6e8a8,0xffc2e0c7,0xffe0f0bd,0xffaee0c9,0xffd2f0b8,0xffb8e0a0,0xffcde0c3 },
            // 4 Sunset (orange / pink)
            { 0xff1f1512,0xff2b1e18,0xfff3e9e0,0xffa08f82,0xff362a24,
              0xffffd6a8,0xffff9db0,0xffeac7bd,0xffffc4a0,0xfff7c98f,0xfff0d9b8,0xffffb0a0,0xfff0c3c8 },
            // 5 Mono (grayscale)
            { 0xff161616,0xff202020,0xffe6e6e6,0xff8c8c8c,0xff2b2b2b,
              0xffcccccc,0xffdddddd,0xffbdbdbd,0xffd6d6d6,0xffc4c4c4,0xffe0e0e0,0xffb8b8b8,0xffcfcfcf },
            // 6 Neon (dark + vivid)
            { 0xff0d0d14,0xff16161f,0xffe9eeff,0xff7d84a0,0xff20202e,
              0xff4dffd0,0xffff4d9d,0xff8f8fff,0xffffb14d,0xff4dd0ff,0xffb0ff4d,0xffff6b6b,0xffc44dff },
            // 7 Vaporwave (purple / cyan / pink)
            { 0xff15111f,0xff1f1830,0xffefe6f6,0xff938aa8,0xff2b2240,
              0xff7df0e0,0xffff8fd0,0xffb79fff,0xffffb0e0,0xff8fd0ff,0xffd0b0ff,0xffff9fd8,0xffc79fff },
            // 8 Amber (warm dark)
            { 0xff1a1610,0xff261f16,0xfff2ebdd,0xff9c9078,0xff332a1e,
              0xffe8d6a0,0xffe0b088,0xffd6c8a8,0xffe8c890,0xffd0c090,0xffe0d8a0,0xffe0b090,0xffd8c0a0 },
            // 9 Arctic (cool light-pastel)
            { 0xff14181c,0xff1e242a,0xffe8f0f5,0xff8496a0,0xff28313a,
              0xffc0f0e8,0xffbcd8ea,0xffcdd8ea,0xffd8ece8,0xffb8dcf0,0xffd8ecdc,0xffc8dce0,0xffcdd8ec }
        } };
        return t;
    }

    inline juce::StringArray getThemeNames()
    {
        return { "Midnight", "Sakura", "Ocean", "Forest", "Sunset",
                 "Mono", "Neon", "Vaporwave", "Amber", "Arctic" };
    }

    inline void setTheme(int idx) noexcept
    {
        const auto& t = themes()[(size_t)juce::jlimit(0, 9, idx)];
        bg        = juce::Colour(t.bg);
        panel     = juce::Colour(t.panel);
        text      = juce::Colour(t.text);
        textDim   = juce::Colour(t.textDim);
        knobTrack = juce::Colour(t.knobTrack);
        mint      = juce::Colour(t.mint);
        pink      = juce::Colour(t.pink);
        lavender  = juce::Colour(t.lavender);
        peach     = juce::Colour(t.peach);
        babyBlue  = juce::Colour(t.babyBlue);
        sage      = juce::Colour(t.sage);
        rose      = juce::Colour(t.rose);
        lilac     = juce::Colour(t.lilac);
        panelLine = text.withAlpha(0.13f);
        grid      = text.withAlpha(0.08f);
        accentOsc    = mint;
        accentPitch  = lavender;
        accentFilter = babyBlue;
        accentFx     = peach;
        accentMaster = pink;
    }

    // アクセントID → 色 (ノブのARC色をテーマへライブ連動させるための解決関数)
    //  ノブは "accentId" プロパティを持ち、ArcDialLookAndFeel が描画時に
    //  ここから現在テーマの色を取得する (テーマ変更が即座に反映される)
    enum AccentId { IdMint = 0, IdPink, IdLavender, IdPeach, IdBabyBlue, IdSage, IdRose, IdLilac };

    inline juce::Colour accentById(int id) noexcept
    {
        switch (id & 7)
        {
        case IdMint:     return mint;
        case IdPink:     return pink;
        case IdLavender: return lavender;
        case IdPeach:    return peach;
        case IdBabyBlue: return babyBlue;
        case IdSage:     return sage;
        case IdRose:     return rose;
        default:         return lilac;
        }
    }

    // ------------------------------------------------------------------
    //  共通描画ヘルパー
    // ------------------------------------------------------------------

    // パネル背景: 微細な縦グラデーション + 上端ハイライト + 枠。
    //  ベタ塗りだと平坦に見えるため、上を +4.5% / 下を -3.5% にして
    //  わずかな奥行きを出す。上端の1px明線が「面」の存在を強調する。
    inline void paintPanel(juce::Graphics& g, juce::Rectangle<float> r, float corner = 8.0f)
    {
        juce::ColourGradient grad(panel.brighter(0.045f), r.getX(), r.getY(),
                                  panel.darker(0.035f),   r.getX(), r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, corner);

        g.setColour(text.withAlpha(0.07f));
        g.drawLine(r.getX() + corner, r.getY() + 1.0f,
                   r.getRight() - corner, r.getY() + 1.0f, 1.0f);

        g.setColour(panelLine);
        g.drawRoundedRectangle(r.reduced(0.5f), corner, 1.0f);
    }

    // 窪んだ表示エリア (波形/カーブ/レスポンス表示の下地)
    inline void paintWell(juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f)
    {
        juce::ColourGradient grad(bg.darker(0.15f),   r.getX(), r.getY(),
                                  bg.brighter(0.05f), r.getX(), r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, corner);
        g.setColour(panelLine);
        g.drawRoundedRectangle(r.reduced(0.5f), corner, 1.0f);
    }

    // カーブ番号 → アクセント色 (CurveStore::Index 順)
    inline juce::Colour curveAccent(int idx) noexcept
    {
        // OSC1-3 (0-11): Pitch系はlavender / モジュレーション系もソース色で統一
        if (idx < 12) return (idx % 4 == 0) ? lavender : mint;
        if (idx < 15) return lilac;                    // ノイズ
        if (idx < 23) return babyBlue;                 // フィルター (Cutoff/Res)
        if (idx < 35) return peach;                    // FX
        if (idx < 39) return rose;                     // PAN
        if (idx < 41) return peach;                    // Stutter
        return mint;                                   // OSC POSITION (mint)
    }
}
