// ==========================================
// File: CurveEditor.cpp
// ==========================================
#include "CurveEditor.h"
#include "ColorPalette.h"
#include <cmath>

CurveEditor::CurveEditor()
    : vblank(this, [this](double)
      {
          // VBlank同期: Progressが動いたフレームだけ再描画 (UIスレッド負荷最小)
          if (progressProvider == nullptr) return;
          const float p = progressProvider();
          if (std::abs(p - lastProgress) > 0.0005f)
          {
              lastProgress = p;
              repaint();
          }
      })
{
    curvePath.preallocateSpace(1024 * 3);
    fillPath.preallocateSpace(1024 * 3 + 16);
    setOpaque(false);
}

// ==========================================================
// 座標変換
// ==========================================================
juce::Rectangle<float> CurveEditor::plotArea() const
{
    return getLocalBounds().toFloat().reduced(14.0f, 22.0f).withTrimmedTop(6.0f);
}

juce::Point<float> CurveEditor::toScreen(float x, float y) const
{
    const auto a = plotArea();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

float CurveEditor::toModelX(float sx) const
{
    const auto a = plotArea();
    return juce::jlimit(0.0f, 1.0f, (sx - a.getX()) / juce::jmax(1.0f, a.getWidth()));
}

float CurveEditor::toModelY(float sy) const
{
    const auto a = plotArea();
    return juce::jlimit(0.0f, 1.0f, (a.getBottom() - sy) / juce::jmax(1.0f, a.getHeight()));
}

// ==========================================================
// モデル
// ==========================================================
void CurveEditor::setSnapshot(const CurveSnapshot& s)
{
    snap = s;
    if (snap.numPoints < 2)
        snap = CurveSnapshot::makeDefault(0.5f, 1.0f);
    dragPoint = dragSegment = -1;
    repaint();
}

void CurveEditor::notify()
{
    if (onChanged != nullptr)
        onChanged(snap);
    repaint();
}

// ==========================================================
// ヒットテスト
// ==========================================================
int CurveEditor::hitPoint(juce::Point<float> pos) const
{
    for (int i = 0; i < snap.numPoints; ++i)
    {
        const auto sp = toScreen(snap.pts[(size_t)i].x, snap.pts[(size_t)i].y);
        if (sp.getDistanceFrom(pos) < 9.0f)
            return i;
    }
    return -1;
}

juce::Point<float> CurveEditor::segmentHandlePos(int seg) const
{
    const auto& p0 = snap.pts[(size_t)seg];
    const auto& p1 = snap.pts[(size_t)seg + 1];
    const float mx = (p0.x + p1.x) * 0.5f;
    const float my = p0.y + (p1.y - p0.y) * CurveSnapshot::shape(0.5f, p0.curve);
    return toScreen(mx, my);
}

int CurveEditor::hitSegmentHandle(juce::Point<float> pos) const
{
    for (int i = 0; i < snap.numPoints - 1; ++i)
        if (segmentHandlePos(i).getDistanceFrom(pos) < 8.0f)
            return i;
    return -1;
}

// ==========================================================
// マウス操作
// ==========================================================
void CurveEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto pos = e.position;
    dragPoint = hitPoint(pos);
    dragSegment = -1;

    if (dragPoint < 0)
    {
        dragSegment = hitSegmentHandle(pos);
        if (dragSegment >= 0)
        {
            dragStartCurve = snap.pts[(size_t)dragSegment].curve;
            dragStartMouseY = pos.y;
        }
    }

    // Alt+クリックで削除 (端点以外)
    if (dragPoint > 0 && dragPoint < snap.numPoints - 1 && e.mods.isAltDown())
    {
        for (int i = dragPoint; i < snap.numPoints - 1; ++i)
            snap.pts[(size_t)i] = snap.pts[(size_t)i + 1];
        --snap.numPoints;
        dragPoint = -1;
        notify();
    }
}

void CurveEditor::mouseDrag(const juce::MouseEvent& e)
{
    const auto pos = e.position;

    if (dragPoint >= 0)
    {
        auto& p = snap.pts[(size_t)dragPoint];
        p.y = toModelY(pos.y);

        // X: 端点は固定、中間点は隣接ポイント間へクランプ
        if (dragPoint > 0 && dragPoint < snap.numPoints - 1)
        {
            const float lo = snap.pts[(size_t)dragPoint - 1].x + 0.005f;
            const float hi = snap.pts[(size_t)dragPoint + 1].x - 0.005f;
            p.x = juce::jlimit(lo, juce::jmax(lo, hi), toModelX(pos.x));
        }
        notify();
    }
    else if (dragSegment >= 0)
    {
        // 縦ドラッグでテンション調整 (セグメントの向きに追従)
        const auto& p0 = snap.pts[(size_t)dragSegment];
        const auto& p1 = snap.pts[(size_t)dragSegment + 1];
        const float dir = (p1.y >= p0.y) ? 1.0f : -1.0f;
        const float delta = (dragStartMouseY - pos.y) * 0.01f * dir;
        snap.pts[(size_t)dragSegment].curve = juce::jlimit(-1.0f, 1.0f, dragStartCurve - delta);
        notify();
    }
}

void CurveEditor::mouseUp(const juce::MouseEvent&)
{
    dragPoint = -1;
    dragSegment = -1;
}

void CurveEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto pos = e.position;

    // ポイント上 → 削除 (端点以外)
    const int hp = hitPoint(pos);
    if (hp > 0 && hp < snap.numPoints - 1)
    {
        for (int i = hp; i < snap.numPoints - 1; ++i)
            snap.pts[(size_t)i] = snap.pts[(size_t)i + 1];
        --snap.numPoints;
        notify();
        return;
    }
    if (hp >= 0) return; // 端点は削除不可

    // ◆ハンドル上 → テンションリセット
    const int hs = hitSegmentHandle(pos);
    if (hs >= 0)
    {
        snap.pts[(size_t)hs].curve = 0.0f;
        notify();
        return;
    }

    // 空白 → ポイント追加
    if (snap.numPoints >= CurveSnapshot::kMaxPoints) return;

    const float nx = toModelX(pos.x);
    const float ny = toModelY(pos.y);

    int insertAt = snap.numPoints - 1;
    for (int i = 0; i < snap.numPoints - 1; ++i)
    {
        if (nx > snap.pts[(size_t)i].x && nx <= snap.pts[(size_t)i + 1].x)
        {
            insertAt = i + 1;
            break;
        }
    }

    for (int i = snap.numPoints; i > insertAt; --i)
        snap.pts[(size_t)i] = snap.pts[(size_t)i - 1];
    snap.pts[(size_t)insertAt] = { nx, ny, 0.0f };
    ++snap.numPoints;
    notify();
}

// ==========================================================
// 描画 (Golden Rule: メンバパス再利用・paint内確保無し)
// ==========================================================
void CurveEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto a = plotArea();

    // パネル背景
    g.setColour(LiftColors::panel);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(LiftColors::panelLine);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // タイトル
    if (title.isNotEmpty())
    {
        g.setColour(LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(title, (int)a.getX(), 4, (int)a.getWidth(), 14, juce::Justification::centredLeft);
    }

    // グリッド (4x4)
    g.setColour(LiftColors::grid);
    for (int i = 1; i < 4; ++i)
    {
        const float gx = a.getX() + a.getWidth() * (float)i / 4.0f;
        const float gy = a.getY() + a.getHeight() * (float)i / 4.0f;
        g.drawVerticalLine((int)gx, a.getY(), a.getBottom());
        g.drawHorizontalLine((int)gy, a.getX(), a.getRight());
    }

    // バイポーラ中央線 (=変化なしライン)
    if (bipolar)
    {
        g.setColour(LiftColors::textDim.withAlpha(0.5f));
        const float cy = a.getCentreY();
        g.drawLine(a.getX(), cy, a.getRight(), cy, 1.0f);
    }

    // カーブパス (2px刻みでサンプリング)
    curvePath.clear();
    fillPath.clear();

    const int steps = juce::jmax(8, (int)(a.getWidth() / 2.0f));
    fillPath.startNewSubPath(a.getX(), a.getBottom());
    for (int i = 0; i <= steps; ++i)
    {
        const float mx = (float)i / (float)steps;
        const auto sp = toScreen(mx, snap.evaluate(mx));
        if (i == 0) curvePath.startNewSubPath(sp);
        else        curvePath.lineTo(sp);
        fillPath.lineTo(sp);
    }
    fillPath.lineTo(a.getRight(), a.getBottom());
    fillPath.closeSubPath();

    g.setColour(accent.withAlpha(0.12f));
    g.fillPath(fillPath);

    g.setColour(accent);
    g.strokePath(curvePath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // セグメントハンドル (◆)
    g.setColour(accent.withAlpha(0.6f));
    for (int i = 0; i < snap.numPoints - 1; ++i)
    {
        const auto hpPos = segmentHandlePos(i);
        g.fillRect(hpPos.x - 3.0f, hpPos.y - 3.0f, 6.0f, 6.0f);
    }

    // ポイント
    for (int i = 0; i < snap.numPoints; ++i)
    {
        const auto sp = toScreen(snap.pts[(size_t)i].x, snap.pts[(size_t)i].y);
        g.setColour(LiftColors::panel);
        g.fillEllipse(sp.x - 5.5f, sp.y - 5.5f, 11.0f, 11.0f);
        g.setColour(accent);
        g.drawEllipse(sp.x - 5.0f, sp.y - 5.0f, 10.0f, 10.0f, 2.0f);
    }

    // プレイヘッド (Progress) - VBlank同期で更新
    const float prog = juce::jlimit(0.0f, 1.0f,
        progressProvider != nullptr ? progressProvider() : 0.0f);
    if (prog > 0.0001f)
    {
        const float px = a.getX() + prog * a.getWidth();
        g.setColour(LiftColors::accentMaster.withAlpha(0.55f));
        g.drawLine(px, a.getY(), px, a.getBottom(), 1.6f);

        const auto dot = toScreen(prog, snap.evaluate(prog));
        g.setColour(LiftColors::accentMaster.withAlpha(0.3f));
        g.fillEllipse(dot.x - 7.0f, dot.y - 7.0f, 14.0f, 14.0f);
        g.setColour(LiftColors::accentMaster);
        g.fillEllipse(dot.x - 3.5f, dot.y - 3.5f, 7.0f, 7.0f);
    }

    // 目盛りラベル
    g.setColour(LiftColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.drawText("0", (int)a.getX() - 4, (int)a.getBottom() + 2, 20, 12, juce::Justification::centredLeft);
    g.drawText("Progress", (int)a.getCentreX() - 30, (int)a.getBottom() + 2, 60, 12, juce::Justification::centred);
    g.drawText("1", (int)a.getRight() - 12, (int)a.getBottom() + 2, 20, 12, juce::Justification::centredRight);
}
