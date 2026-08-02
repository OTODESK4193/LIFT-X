// ==========================================
// File: CurveEditor.h
// マルチポイント・カーブエディタ (本プラグインの肝: マルチENV編集UI)
//
//  操作:
//   - ポイントをドラッグ         : 移動 (端点はX固定)
//   - 空白をダブルクリック       : ポイント追加 (最大32)
//   - ポイントをダブルクリック   : 削除 (端点以外)
//   - セグメント中央の◆をドラッグ: カーブ(テンション)調整
//   - ◆をダブルクリック          : テンションリセット
//
//  描画:
//   - juce::VBlankAttachment でモニターリフレッシュに同期した
//     プレイヘッド(Progress)アニメーション (計画書フェーズ4)
//   - "Golden Rule": paint() 内で juce::Path 等を新規確保せず、
//     メンバを clear()+再利用 (preallocateSpace 済み)
//   - VBlankAttachment はメンバとして保持され、デストラクタで
//     自動的にコールバック解除される (UI破棄時のクラッシュ防止)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <functional>

#include "../DSP/CurveData.h"

class CurveEditor : public juce::Component
{
public:
    CurveEditor();
    ~CurveEditor() override = default;

    // モデル
    void setSnapshot(const CurveSnapshot& s);
    const CurveSnapshot& getSnapshot() const noexcept { return snap; }

    // 変更通知 (CurveStore::publish へ接続する)
    std::function<void(const CurveSnapshot&)> onChanged;

    // 「Paste to All」でタブ内の全ターゲットへ貼るためのフック。
    //  設定されている場合のみメニューに項目が出る。
    std::function<void(const CurveSnapshot&)> onPasteToAll;

    // ---- カーブのクリップボード (全エディタで共有) ----
    //  35本のカーブを1本ずつ描くのは大変なので、OSC1で作った形を
    //  OSC2/3 や他のターゲットへそのまま流用できるようにする。
    static CurveSnapshot& clipboard()      { static CurveSnapshot c; return c; }
    static bool& clipboardValid()          { static bool v = false; return v; }
    static bool hasClipboard()             { return clipboardValid(); }

    // 見た目
    void setAccent(juce::Colour c) { accent = c; repaint(); }
    void setBipolar(bool b) { bipolar = b; repaint(); }
    void setTitle(const juce::String& t) { title = t; repaint(); }

    // プレイヘッド (オーディオスレッドのatomicを読む関数を渡す)
    void setProgressProvider(std::function<float()> f) { progressProvider = std::move(f); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    // ---- ホバー / ドラッグ中の視覚フィードバック ----
    void updateHover(juce::Point<float> pos);
    void drawValueBadge(juce::Graphics& g, juce::Point<float> at, const juce::String& text);
    int hoverPoint = -1;
    int hoverSegment = -1;

    // ---- Snap / グリッド補助線 ----
    float snapX(float x) const;

    // ---- カーブプリセット (ファクトリー20種 + ユーザーSave/Load) ----
    void showPresetMenu();
    void saveCurveDialog();
    static juce::File curveDir();
    static juce::StringArray factoryCurveNames();
    static CurveSnapshot makeFactoryCurve(int id);

    juce::TextButton presetBtn { "CURVES" };
    juce::TextButton snapBtn { "SNAP" };
    juce::ComboBox gridBox;
    bool snapOn = false;
    int gridDiv = 16;

    // ---- REPEAT (カーブのLFO化) ----
    //  編集は常に1サイクルを全幅で行い、繰り返した結果はゴーストで重ね描きする。
    //  (1/32 幅で編集させるのは非現実的なため)
    static constexpr int kNumRepeatChoices = 10;
    static constexpr int kRepeatChoices[kNumRepeatChoices] =
        { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
    juce::ComboBox repeatBox;
    void syncRepeatBox();

    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen(float x, float y) const;
    float toModelX(float sx) const;
    float toModelY(float sy) const;
    int hitPoint(juce::Point<float> pos) const;      // ポイントのヒットテスト
    int hitSegmentHandle(juce::Point<float> pos) const; // ◆ハンドルのヒットテスト
    juce::Point<float> segmentHandlePos(int seg) const;
    void notify();

    CurveSnapshot snap = CurveSnapshot::makeDefault(0.5f, 1.0f);
    juce::Colour accent { 0xff7a6cd0 };
    bool bipolar = true;
    juce::String title;

    // ドラッグ状態
    int dragPoint = -1;
    int dragSegment = -1;
    float dragStartCurve = 0.0f;
    float dragStartMouseY = 0.0f;

    // プレイヘッド
    std::function<float()> progressProvider;
    float lastProgress = -1.0f;

    // Golden Rule: 再利用パス (paint内での動的確保を排除)
    juce::Path curvePath, fillPath;

    // VBlank同期アニメーション (デストラクタで自動解除)
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveEditor)
};
