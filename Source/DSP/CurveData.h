// ==========================================
// File: CurveData.h
// マルチポイント・カーブ（本プラグインの肝: マルチENV）のデータモデル
//
//  - CurveSnapshot : 固定サイズPOD。最大32ポイント + セグメント毎のテンション。
//                    evaluate() は DSP と GUI 描画で共有され、表示と音のズレが無い。
//  - CurveStore    : 9系統のカーブを保持。GUI(メッセージスレッド)が publish() し、
//                    オーディオスレッドは atomic インデックス経由で read() するだけの
//                    ロックフリー設計（リングバッファ8面。旧スナップショットは
//                    上書きまで保持されるため解放待ちが発生しない）。
//  - processBlock 内でのアロケーション/ロックは一切無い。
//  - カーブはホストパラメーターにしない（APVTS外）。これにより計画書の
//    「内部カーブターゲットの withAutomatable(false) 隔離」と同じ効果を
//    より強い形で達成し、Ableton Live のオートメーション巻き戻りを防ぐ。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

// ------------------------------------------
// 1本のカーブのスナップショット (POD・固定サイズ)
// ------------------------------------------
struct CurveSnapshot
{
    static constexpr int kMaxPoints = 32;

    struct Point
    {
        float x = 0.0f;      // 0..1 (Progress)
        float y = 0.5f;      // 0..1
        float curve = 0.0f;  // -1..1 このポイントから次ポイントまでのテンション
    };

    int numPoints = 0;
    std::array<Point, kMaxPoints> pts {};

    // セグメント形状: テンション c(-1..1) を指数 2^(3c) にマップ (0.125..8)
    static float shape(float t, float c) noexcept
    {
        t = juce::jlimit(0.0f, 1.0f, t);
        if (std::abs(c) < 0.005f) return t;
        return std::pow(t, std::exp2(c * 3.0f));
    }

    // x(0..1) におけるカーブ値 y(0..1)。RT安全 (線形走査・分岐のみ)。
    float evaluate(float x) const noexcept
    {
        if (numPoints <= 0) return 0.5f;
        if (numPoints == 1) return pts[0].y;

        x = juce::jlimit(0.0f, 1.0f, x);
        if (x <= pts[0].x) return pts[0].y;

        for (int i = 0; i < numPoints - 1; ++i)
        {
            const auto& p0 = pts[(size_t)i];
            const auto& p1 = pts[(size_t)i + 1];
            if (x <= p1.x)
            {
                const float w = p1.x - p0.x;
                const float t = (w > 1.0e-6f) ? (x - p0.x) / w : 1.0f;
                return p0.y + (p1.y - p0.y) * shape(t, p0.curve);
            }
        }
        return pts[(size_t)numPoints - 1].y;
    }

    // デフォルト2点 (start→end の直線)
    static CurveSnapshot makeDefault(float yStart, float yEnd)
    {
        CurveSnapshot s;
        s.numPoints = 2;
        s.pts[0] = { 0.0f, yStart, 0.0f };
        s.pts[1] = { 1.0f, yEnd,   0.0f };
        return s;
    }

    // --- シリアライズ ("x,y,c;x,y,c;...") ---
    juce::String toString() const
    {
        juce::String s;
        for (int i = 0; i < numPoints; ++i)
        {
            if (i > 0) s << ";";
            s << juce::String(pts[(size_t)i].x, 5) << ","
              << juce::String(pts[(size_t)i].y, 5) << ","
              << juce::String(pts[(size_t)i].curve, 4);
        }
        return s;
    }

    static CurveSnapshot fromString(const juce::String& str)
    {
        CurveSnapshot s;
        auto segs = juce::StringArray::fromTokens(str, ";", "");
        for (const auto& seg : segs)
        {
            if (s.numPoints >= kMaxPoints) break;
            auto v = juce::StringArray::fromTokens(seg, ",", "");
            if (v.size() < 2) continue;
            Point p;
            p.x = juce::jlimit(0.0f, 1.0f, v[0].getFloatValue());
            p.y = juce::jlimit(0.0f, 1.0f, v[1].getFloatValue());
            p.curve = v.size() > 2 ? juce::jlimit(-1.0f, 1.0f, v[2].getFloatValue()) : 0.0f;
            s.pts[(size_t)s.numPoints++] = p;
        }
        if (s.numPoints < 2)
            s = makeDefault(0.5f, 1.0f);
        // 端点を強制 (x=0 / x=1)
        s.pts[0].x = 0.0f;
        s.pts[(size_t)s.numPoints - 1].x = 1.0f;
        return s;
    }
};

// ------------------------------------------
// 9系統カーブの保管庫 (ロックフリーSPSC)
//   書き手: GUI/メッセージスレッドのみ
//   読み手: オーディオスレッド (readで最新publish済みを取得)
// ------------------------------------------
class CurveStore
{
public:
    enum Index
    {
        PitchOsc1 = 0, PitchOsc2, PitchOsc3, PitchNoise,
        Filter1, Filter2, Filter3, Filter4,
        FxCurve,
        kNumCurves
    };

    static const char* name(int idx)
    {
        static const char* names[kNumCurves] = {
            "PITCH OSC1", "PITCH OSC2", "PITCH OSC3", "PITCH NOISE",
            "FILTER 1", "FILTER 2", "FILTER 3", "FILTER 4", "FX" };
        return names[juce::jlimit(0, kNumCurves - 1, idx)];
    }

    CurveStore()
    {
        // デフォルト: ピッチ/フィルターは中央→上昇、FXは0→1
        for (int i = 0; i < 4; ++i) publish(i, CurveSnapshot::makeDefault(0.5f, 1.0f));
        for (int i = 4; i < 8; ++i) publish(i, CurveSnapshot::makeDefault(0.5f, 1.0f));
        publish(FxCurve, CurveSnapshot::makeDefault(0.0f, 1.0f));
    }

    // --- オーディオスレッド: 最新スナップショット参照 (コピー無し) ---
    const CurveSnapshot& read(int idx) const noexcept
    {
        const auto& s = slots[(size_t)juce::jlimit(0, kNumCurves - 1, idx)];
        return s.ring[(size_t)s.pub.load(std::memory_order_acquire)];
    }

    // --- GUI: コピー取得 (編集開始用) ---
    CurveSnapshot get(int idx) const { return read(idx); }

    // --- GUI: 更新発行 ---
    void publish(int idx, const CurveSnapshot& snap)
    {
        auto& s = slots[(size_t)juce::jlimit(0, kNumCurves - 1, idx)];
        s.cursor = (s.cursor + 1) % kRingSize;
        s.ring[(size_t)s.cursor] = snap;
        s.pub.store(s.cursor, std::memory_order_release);
    }

    // --- ステート保存/復元 (メッセージスレッド) ---
    juce::ValueTree toValueTree() const
    {
        juce::ValueTree vt("CURVES");
        for (int i = 0; i < kNumCurves; ++i)
        {
            juce::ValueTree c("CURVE");
            c.setProperty("idx", i, nullptr);
            c.setProperty("pts", read(i).toString(), nullptr);
            vt.appendChild(c, nullptr);
        }
        return vt;
    }

    void fromValueTree(const juce::ValueTree& vt)
    {
        if (!vt.isValid()) return;
        for (int i = 0; i < vt.getNumChildren(); ++i)
        {
            auto c = vt.getChild(i);
            if (!c.hasType("CURVE")) continue;
            const int idx = (int)c.getProperty("idx", -1);
            if (idx < 0 || idx >= kNumCurves) continue;
            publish(idx, CurveSnapshot::fromString(c.getProperty("pts").toString()));
        }
    }

private:
    static constexpr int kRingSize = 8;

    struct Slot
    {
        std::array<CurveSnapshot, kRingSize> ring {};
        std::atomic<int> pub { 0 };
        int cursor = 0;
    };

    std::array<Slot, kNumCurves> slots;

    JUCE_DECLARE_NON_COPYABLE(CurveStore)
};
