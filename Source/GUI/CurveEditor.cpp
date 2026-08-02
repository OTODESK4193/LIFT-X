// ==========================================
// File: CurveEditor.cpp
// ==========================================
#include "CurveEditor.h"
#include "ColorPalette.h"
#include <algorithm>
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

    // ---- カーブプリセット / Snap / グリッド (右上コントロール) ----
    presetBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetBtn);

    snapBtn.setClickingTogglesState(true);
    snapBtn.onClick = [this]
    {
        snapOn = snapBtn.getToggleState();
        snapBtn.setColour(juce::TextButton::buttonColourId,
                          snapOn ? LiftColors::accentMaster.withAlpha(0.35f)
                                 : LiftColors::knobTrack);
        repaint();
    };
    addAndMakeVisible(snapBtn);

    gridBox.addItemList({ "4", "8", "16", "32", "64" }, 1);
    gridBox.setSelectedItemIndex(2, juce::dontSendNotification); // 16
    gridBox.onChange = [this]
    {
        static const int divs[5] = { 4, 8, 16, 32, 64 };
        gridDiv = divs[juce::jlimit(0, 4, gridBox.getSelectedItemIndex())];
        repaint();
    };
    addAndMakeVisible(gridBox);
}

void CurveEditor::resized()
{
    auto top = getLocalBounds().removeFromTop(24).reduced(8, 3);
    gridBox.setBounds(top.removeFromRight(56));
    top.removeFromRight(4);
    snapBtn.setBounds(top.removeFromRight(52));
    top.removeFromRight(4);
    presetBtn.setBounds(top.removeFromRight(66));
}

// ==========================================================
// Snap
// ==========================================================
float CurveEditor::snapX(float x) const
{
    if (!snapOn || gridDiv < 2)
        return x;
    return juce::jlimit(0.0f, 1.0f,
                        std::round(x * (float)gridDiv) / (float)gridDiv);
}

// ==========================================================
// カーブプリセット
// ==========================================================
juce::File CurveEditor::curveDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("LIFT-X").getChildFile("Curves");
    dir.createDirectory();
    return dir;
}

juce::StringArray CurveEditor::factoryCurveNames()
{
    return { "Linear Up", "Linear Down", "Exp Up (Soft)", "Exp Up (Hard)", "Log Up",
             "Exp Down", "Log Down", "S-Curve Up", "S-Curve Down", "Ramp + Hold",
             "Hold + Ramp", "Triangle", "V Shape",
             "Steps 4", "Steps 8", "Steps 16", "Steps 32",
             "Saw 4", "Saw 8", "Saw 16", "Saw 32",
             "Pulse 4", "Pulse 8", "Pulse 16", "Pulse 32",
             "Zigzag Up", "Flat Center", "Flat Max" };
}

CurveSnapshot CurveEditor::makeFactoryCurve(int id)
{
    CurveSnapshot s;
    auto add = [&s](float x, float y, float c = 0.0f)
    {
        if (s.numPoints < CurveSnapshot::kMaxPoints)
            s.pts[(size_t)s.numPoints++] = { x, y, c };
    };

    // 分割系ジェネレーター (n分割, eps=段差の立ち上がり幅)
    auto steps = [&add](int n)
    {
        const float eps = juce::jmin(0.01f, 0.25f / (float)n);
        for (int k = 0; k < n; ++k)
        {
            const float lv = (float)k / (float)(n - 1);
            add((float)k / (float)n, lv);
            add((float)(k + 1) / (float)n - eps, lv);
        }
        add(1, 1);
    };
    auto saw = [&add](int n)
    {
        const float eps = juce::jmin(0.01f, 0.25f / (float)n);
        for (int k = 0; k < n; ++k)
        {
            add((float)k / (float)n, 0);
            add((float)(k + 1) / (float)n - eps, 1);
        }
    };
    auto pulse = [&add](int n)
    {
        const float eps = juce::jmin(0.01f, 0.25f / (float)n);
        for (int k = 0; k < n; ++k)
        {
            const float lv = (k % 2 == 0) ? 1.0f : 0.0f;
            add((float)k / (float)n, lv);
            add((float)(k + 1) / (float)n - eps, lv);
        }
    };

    switch (id)
    {
    case 0:  add(0, 0);        add(1, 1); break;                       // Linear Up
    case 1:  add(0, 1);        add(1, 0); break;                       // Linear Down
    case 2:  add(0, 0, 0.45f); add(1, 1); break;                       // Exp Up Soft
    case 3:  add(0, 0, 0.8f);  add(1, 1); break;                       // Exp Up Hard
    case 4:  add(0, 0, -0.5f); add(1, 1); break;                       // Log Up
    case 5:  add(0, 1, 0.45f); add(1, 0); break;                       // Exp Down
    case 6:  add(0, 1, -0.5f); add(1, 0); break;                       // Log Down
    case 7:  add(0, 0, 0.5f);  add(0.5f, 0.5f, -0.5f); add(1, 1); break; // S Up
    case 8:  add(0, 1, 0.5f);  add(0.5f, 0.5f, -0.5f); add(1, 0); break; // S Down
    case 9:  add(0, 0, 0.3f);  add(0.6f, 1); add(1, 1); break;         // Ramp+Hold
    case 10: add(0, 0);        add(0.4f, 0, 0.5f); add(1, 1); break;   // Hold+Ramp
    case 11: add(0, 0);        add(0.5f, 1); add(1, 0); break;         // Triangle
    case 12: add(0, 1);        add(0.5f, 0); add(1, 1); break;         // V Shape
    case 13: steps(4);  break;
    case 14: steps(8);  break;
    case 15: steps(16); break;
    case 16: steps(32); break;
    case 17: saw(4);  break;
    case 18: saw(8);  break;
    case 19: saw(16); break;
    case 20: saw(32); break;
    case 21: pulse(4);  break;
    case 22: pulse(8);  break;
    case 23: pulse(16); break;
    case 24: pulse(32); break;
    case 25: // Zigzag Up
        add(0, 0); add(0.2f, 0.5f); add(0.4f, 0.25f);
        add(0.6f, 0.75f); add(0.8f, 0.5f); add(1, 1);
        break;
    case 26: add(0, 0.5f); add(1, 0.5f); break;                        // Flat Center
    default: add(0, 1);    add(1, 1); break;                           // Flat Max
    }

    s.pts[0].x = 0.0f;
    s.pts[(size_t)s.numPoints - 1].x = 1.0f;
    return s;
}

void CurveEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const auto names = factoryCurveNames();
    for (int i = 0; i < names.size(); ++i)
        m.addItem(1 + i, names[i]);

    // ユーザー保存カーブ
    auto files = curveDir().findChildFiles(juce::File::findFiles, false, "*.crv");
    std::sort(files.begin(), files.end(),
              [](const juce::File& a, const juce::File& b)
              { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });

    if (!files.isEmpty())
    {
        juce::PopupMenu um;
        for (int i = 0; i < files.size(); ++i)
            um.addItem(200 + i, files[i].getFileNameWithoutExtension());
        m.addSeparator();
        m.addSubMenu("User Curves", um);
    }

    m.addSeparator();
    m.addItem(100, "Save Current...");

    const int numFactory = names.size();
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
        [this, files, numFactory](int result)
        {
            if (result == 0) return;

            if (result >= 1 && result <= numFactory)
            {
                setSnapshot(makeFactoryCurve(result - 1));
                notify();
            }
            else if (result == 100)
            {
                saveCurveDialog();
            }
            else if (result >= 200 && result - 200 < files.size())
            {
                setSnapshot(CurveSnapshot::fromString(
                    files[result - 200].loadFileAsString()));
                notify();
            }
        });
}

void CurveEditor::saveCurveDialog()
{
    auto* w = new juce::AlertWindow("Save Curve", "Curve name:",
                                    juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", "MyCurve");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, w](int result)
        {
            if (result == 1)
            {
                auto name = w->getTextEditorContents("name").trim();
                if (name.isEmpty()) name = "Curve";
                curveDir().getChildFile(juce::File::createLegalFileName(name) + ".crv")
                          .replaceWithText(snap.toString());
            }
        }), true);
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

        // X: 端点は固定、中間点はSnap適用後に隣接ポイント間へクランプ
        if (dragPoint > 0 && dragPoint < snap.numPoints - 1)
        {
            const float lo = snap.pts[(size_t)dragPoint - 1].x + 0.005f;
            const float hi = snap.pts[(size_t)dragPoint + 1].x - 0.005f;
            p.x = juce::jlimit(lo, juce::jmax(lo, hi), snapX(toModelX(pos.x)));
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

void CurveEditor::mouseUp(const juce::MouseEvent& e)
{
    dragPoint = -1;
    dragSegment = -1;
    updateHover(e.position);   // ドラッグ終了後のホバー状態を即反映
    repaint();
}

// ---- ホバー: カーソル下のポイント/ハンドルを強調 ----
void CurveEditor::updateHover(juce::Point<float> pos)
{
    const int hp = hitPoint(pos);
    const int hs = (hp < 0) ? hitSegmentHandle(pos) : -1;
    if (hp != hoverPoint || hs != hoverSegment)
    {
        hoverPoint = hp;
        hoverSegment = hs;
        setMouseCursor((hp >= 0 || hs >= 0) ? juce::MouseCursor::PointingHandCursor
                                            : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CurveEditor::mouseMove(const juce::MouseEvent& e)
{
    updateHover(e.position);
}

void CurveEditor::mouseExit(const juce::MouseEvent&)
{
    if (hoverPoint >= 0 || hoverSegment >= 0)
    {
        hoverPoint = hoverSegment = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
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

    // 空白 → ポイント追加 (Snap有効時はグリッドへ吸着)
    if (snap.numPoints >= CurveSnapshot::kMaxPoints) return;

    const float nx = snapX(toModelX(pos.x));
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

    // パネル背景 + プロットエリアを一段窪ませる (カーブを主役に見せる)
    LiftColors::paintPanel(g, bounds);
    LiftColors::paintWell(g, a.expanded(4.0f, 3.0f), 6.0f);

    // タイトル
    if (title.isNotEmpty())
    {
        g.setColour(LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(title, (int)a.getX(), 4, (int)a.getWidth(), 14, juce::Justification::centredLeft);
    }

    // ---- グリッド ----
    //  Snap用の細分線を先に薄く、4分割の主線を後から濃く描いて階層を付ける
    //  (旧実装は同じ濃さで重なっており、視線の拠り所が無かった)
    if (gridDiv > 4)
    {
        g.setColour(LiftColors::text.withAlpha(snapOn ? 0.09f : 0.035f));
        for (int i = 1; i < gridDiv; ++i)
        {
            const float gx = a.getX() + a.getWidth() * (float)i / (float)gridDiv;
            g.drawVerticalLine((int)gx, a.getY(), a.getBottom());
        }
    }
    g.setColour(LiftColors::text.withAlpha(0.10f));
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
        g.setColour(LiftColors::textDim.withAlpha(0.55f));
        const float cy = a.getCentreY();
        g.drawLine(a.getX(), cy, a.getRight(), cy, 1.0f);
    }

    // ---- カーブパス (2px刻みでサンプリング) ----
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

    // 塗り: 上ほど濃い縦グラデーション (「エネルギーが立ち上がる」印象を出す)
    {
        juce::ColourGradient fillGrad(accent.withAlpha(0.30f), a.getX(), a.getY(),
                                      accent.withAlpha(0.02f), a.getX(), a.getBottom(), false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);
    }

    // 線: 外側グロー → 本体 の2段描き
    g.setColour(accent.withAlpha(0.13f));
    g.strokePath(curvePath, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    g.setColour(accent);
    g.strokePath(curvePath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // ---- セグメントハンドル (◆) ----
    for (int i = 0; i < snap.numPoints - 1; ++i)
    {
        const auto hpPos = segmentHandlePos(i);
        const bool hot = (hoverSegment == i || dragSegment == i);
        const float s = hot ? 4.5f : 3.0f;
        if (hot)
        {
            g.setColour(accent.withAlpha(0.25f));
            g.fillEllipse(hpPos.x - s - 3.0f, hpPos.y - s - 3.0f, (s + 3.0f) * 2.0f, (s + 3.0f) * 2.0f);
        }
        g.setColour(accent.withAlpha(hot ? 1.0f : 0.55f));
        g.fillRect(hpPos.x - s, hpPos.y - s, s * 2.0f, s * 2.0f);
    }

    // ---- ポイント ----
    for (int i = 0; i < snap.numPoints; ++i)
    {
        const auto sp = toScreen(snap.pts[(size_t)i].x, snap.pts[(size_t)i].y);
        const bool hot = (hoverPoint == i || dragPoint == i);
        const float rad = hot ? 7.0f : 5.0f;

        if (hot)   // ホバー/ドラッグ中はグローを足す
        {
            g.setColour(accent.withAlpha(0.28f));
            g.fillEllipse(sp.x - rad - 4.0f, sp.y - rad - 4.0f,
                          (rad + 4.0f) * 2.0f, (rad + 4.0f) * 2.0f);
        }
        g.setColour(LiftColors::panel);
        g.fillEllipse(sp.x - rad - 0.5f, sp.y - rad - 0.5f, (rad + 0.5f) * 2.0f, (rad + 0.5f) * 2.0f);
        g.setColour(accent);
        g.drawEllipse(sp.x - rad, sp.y - rad, rad * 2.0f, rad * 2.0f, hot ? 2.4f : 2.0f);
    }

    // ---- プレイヘッド (Progress) - VBlank同期で更新 ----
    const float prog = juce::jlimit(0.0f, 1.0f,
        progressProvider != nullptr ? progressProvider() : 0.0f);
    if (prog > 0.0001f)
    {
        const float px = a.getX() + prog * a.getWidth();

        // 進行方向の後ろへ伸びるトレイル (動きを視覚的に強調)
        const float trailW = juce::jmin(56.0f, px - a.getX());
        if (trailW > 2.0f)
        {
            juce::ColourGradient tg(LiftColors::accentMaster.withAlpha(0.0f), px - trailW, 0.0f,
                                    LiftColors::accentMaster.withAlpha(0.16f), px, 0.0f, false);
            g.setGradientFill(tg);
            g.fillRect(juce::Rectangle<float>(px - trailW, a.getY(), trailW, a.getHeight()));
        }

        g.setColour(LiftColors::accentMaster.withAlpha(0.6f));
        g.drawLine(px, a.getY(), px, a.getBottom(), 1.6f);

        const auto dot = toScreen(prog, snap.evaluate(prog));
        g.setColour(LiftColors::accentMaster.withAlpha(0.28f));
        g.fillEllipse(dot.x - 8.0f, dot.y - 8.0f, 16.0f, 16.0f);
        g.setColour(LiftColors::accentMaster);
        g.fillEllipse(dot.x - 3.5f, dot.y - 3.5f, 7.0f, 7.0f);
    }

    // ---- 目盛りラベル ----
    g.setColour(LiftColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.drawText("0", (int)a.getX() - 4, (int)a.getBottom() + 2, 20, 12, juce::Justification::centredLeft);
    g.drawText("Progress", (int)a.getCentreX() - 30, (int)a.getBottom() + 2, 60, 12, juce::Justification::centred);
    g.drawText("1", (int)a.getRight() - 12, (int)a.getBottom() + 2, 20, 12, juce::Justification::centredRight);

    // ---- ドラッグ中の数値バッジ ----
    //  掴んでいる点の座標を追従表示する (細かい調整がしやすくなる)
    if (dragPoint >= 0 && dragPoint < snap.numPoints)
        drawValueBadge(g, toScreen(snap.pts[(size_t)dragPoint].x, snap.pts[(size_t)dragPoint].y),
                       juce::String((int)std::round(snap.pts[(size_t)dragPoint].x * 100.0f)) + "%  "
                     + juce::String(snap.pts[(size_t)dragPoint].y, 2));
    else if (dragSegment >= 0 && dragSegment < snap.numPoints - 1)
        drawValueBadge(g, segmentHandlePos(dragSegment),
                       "CURVE " + juce::String(snap.pts[(size_t)dragSegment].curve, 2));
}

// ドラッグ中の値バッジ (点の少し上に出す。画面外へはみ出す場合は下へ回す)
void CurveEditor::drawValueBadge(juce::Graphics& g, juce::Point<float> at, const juce::String& text)
{
    const auto a = plotArea();
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    const float w = 78.0f, h = 18.0f;

    float bx = juce::jlimit(a.getX(), a.getRight() - w, at.x - w * 0.5f);
    float by = at.y - h - 12.0f;
    if (by < a.getY()) by = at.y + 12.0f;

    const juce::Rectangle<float> box(bx, by, w, h);
    g.setColour(LiftColors::bg.withAlpha(0.92f));
    g.fillRoundedRectangle(box, 4.0f);
    g.setColour(accent.withAlpha(0.75f));
    g.drawRoundedRectangle(box.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(LiftColors::text);
    g.drawText(text, box, juce::Justification::centred);
}
