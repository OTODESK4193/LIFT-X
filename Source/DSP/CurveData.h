// ==========================================
// File: CurveData.h
// マルチポイント・カーブ（本プラグインの肝: マルチENV）のデータモデル
//
//  - CurveSnapshot : 固定サイズPOD。最大32ポイント + セグメント毎のテンション。
//                    evaluate() は DSP と GUI 描画で共有され、表示と音のズレが無い。
//  - CurveStore    : 35系統のカーブを保持。GUI(メッセージスレッド)が publish() し、
//                    オーディオスレッドは atomic インデックス経由で read() するだけの
//                    ロックフリー設計（リングバッファ8面）。
//  - processBlock 内でのアロケーション/ロックは一切無い。
//  - カーブはホストパラメーターにしない（APVTS外）。Ableton Live の
//    オートメーション巻き戻り現象から構造的に隔離される。
//
//  カーブの意味 (v0.2):
//   - OscPitch  : 下=StartKey / 上=EndKey (ユニポーラ補間, デフォルト 0→1 上昇)
//   - NoisePitch: 中央=基準 / ±RANGE oct (バイポーラ)
//   - Filter    : 中央=CUTOFF / ±5oct×ENV AMT (バイポーラ)
//   - その他 (Level/Detune/Spread/Res/FXパラメーター):
//       バイポーラ加算式。中央(0.5)=ノブ現在値、上下でパラメーターレンジの
//       半分を±加算 (クランプあり)。デフォルトは中央フラット=変化なし。
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
    // Steps/Saw/Pulse 32分割プリセット (最大65点) に対応するため128点
    static constexpr int kMaxPoints = 128;

    struct Point
    {
        float x = 0.0f;      // 0..1 (Progress)
        float y = 0.5f;      // 0..1
        float curve = 0.0f;  // -1..1 このポイントから次ポイントまでのテンション
    };

    int numPoints = 0;

    // ---- REPEAT (カーブのLFO化) ----
    //  評価位置 x を repeat 回ぶん折り返して読む。1 なら従来どおり1回だけ通る。
    //  これだけで同じカーブが「テンポ同期LFO」として機能する:
    //   ・Level カーブ  x16 → 16分のゲート/トレモロ
    //   ・Pitch カーブ  x8  → ワブル
    //   ・Filter カーブ x32 → 細かい刻み
    //  Bars と連動するため、常に小節に対して正確な分割になる。
    int repeat = 1;   // 1..32

    std::array<Point, kMaxPoints> pts {};

    // セグメント形状: テンション c(-1..1) を指数 2^(3c) にマップ (0.125..8)
    static float shape(float t, float c) noexcept
    {
        t = juce::jlimit(0.0f, 1.0f, t);
        if (std::abs(c) < 0.005f) return t;
        return std::pow(t, std::exp2(c * 3.0f));
    }

    // x(0..1) におけるカーブ値 y(0..1)。RT安全 (線形走査・分岐のみ)。
    //
    //  hint: 前回ヒットしたセグメント番号を渡すと、そこから探索を始める。
    //   評価位置 (Progress / LIFT) は連続的にしか動かないため、ほぼ必ず
    //   同じセグメントか隣接セグメントでヒットし、実質 O(1) になる。
    //   Steps32 等の 65点カーブでは線形走査が平均32回まで伸びるため効果が大きい。
    //   nullptr を渡せば従来どおり先頭から走査する (GUI描画はこちら)。
    float evaluate(float x, int* hint = nullptr) const noexcept
    {
        return evaluateRaw(wrapRepeat(x), hint);
    }

    // REPEAT を適用した評価位置を返す。
    //  終端 (x>=1) だけはカーブの終わりを返す (折り返して先頭に戻さない)。
    float wrapRepeat(float x) const noexcept
    {
        x = juce::jlimit(0.0f, 1.0f, x);
        if (repeat <= 1 || x >= 1.0f) return x;
        const float xr = x * (float)repeat;
        return xr - std::floor(xr);
    }

    // REPEAT を適用しない素の評価 (カーブエディタの編集表示用)
    float evaluateRaw(float x, int* hint = nullptr) const noexcept
    {
        if (numPoints <= 0) return 0.5f;
        if (numPoints == 1) return pts[0].y;

        x = juce::jlimit(0.0f, 1.0f, x);
        if (x <= pts[0].x) return pts[0].y;

        const int nSeg = numPoints - 1;

        // ヒントの妥当性を検証してから使う (カーブ差し替え直後は範囲外になりうる)
        int i = 0;
        if (hint != nullptr)
        {
            i = *hint;
            if (i < 0 || i >= nSeg) i = 0;
            // ヒント位置より手前なら後退、そうでなければヒント位置から前進
            else if (x <= pts[(size_t)i].x) i = 0;
        }

        for (; i < nSeg; ++i)
        {
            const auto& p0 = pts[(size_t)i];
            const auto& p1 = pts[(size_t)i + 1];
            if (x <= p1.x)
            {
                if (hint != nullptr) *hint = i;
                const float w = p1.x - p0.x;
                const float t = (w > 1.0e-6f) ? (x - p0.x) / w : 1.0f;
                return p0.y + (p1.y - p0.y) * shape(t, p0.curve);
            }
        }
        if (hint != nullptr) *hint = nSeg - 1;
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

    // --- シリアライズ ---
    //  基本形は "x,y,c;x,y,c;..."。
    //  REPEAT が 2以上のときだけ先頭に "R<n>|" を付ける。
    //  → 旧プリセット/旧セッションは R が無いので repeat=1 として読まれ、
    //    完全に後方互換が保たれる。
    juce::String toString() const
    {
        juce::String s;
        if (repeat > 1) s << "R" << repeat << "|";
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

        juce::String body = str;
        if (body.startsWithChar('R'))
        {
            const int bar = body.indexOfChar('|');
            if (bar > 1)
            {
                s.repeat = juce::jlimit(1, 32, body.substring(1, bar).getIntValue());
                body = body.substring(bar + 1);
            }
        }

        auto segs = juce::StringArray::fromTokens(body, ";", "");
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
        {
            const int keepRepeat = s.repeat;
            s = makeDefault(0.5f, 0.5f);
            s.repeat = keepRepeat;
        }
        // 端点を強制 (x=0 / x=1)
        s.pts[0].x = 0.0f;
        s.pts[(size_t)s.numPoints - 1].x = 1.0f;
        return s;
    }
};

// ------------------------------------------
// 35系統カーブの保管庫 (ロックフリーSPSC)
//   書き手: GUI/メッセージスレッドのみ
//   読み手: オーディオスレッド
//
// ※ Index への追加は必ず「末尾」に行うこと。途中挿入すると
//   FactoryPresets.cpp のカーブ index が全件ズレる (v0.4 で実際に発生した)。
//   FactoryPresets.cpp 側に static_assert の防波堤を置いてある。
// ------------------------------------------
class CurveStore
{
public:
    enum Index
    {
        // OSC1-3: [Pitch, Level, Detune, Spread] × 3
        Osc1Pitch = 0, Osc1Level, Osc1Detune, Osc1Spread,
        Osc2Pitch,     Osc2Level, Osc2Detune, Osc2Spread,
        Osc3Pitch,     Osc3Level, Osc3Detune, Osc3Spread,
        // ノイズ
        NoisePitch = 12, NoiseLevel, NoiseRes,
        // フィルター (CUTOFF & RES 独立)
        Filter1 = 15, Filter2, Filter3, Filter4,
        Filter1Res, Filter2Res, Filter3Res, Filter4Res,
        // FX
        SatAmt = 23, SatDrive,
        ChoAmt, ChoDepth,
        DlyAmt, DlyFb, DlyTime,
        RevAmt, RevShimmer,
        DuckAmt, DuckRate, DuckShape,
        // ---- v0.6 追加: PAN ENV (必ず末尾へ追加すること) ----
        //  中央=センター / 上=右 / 下=左。ソース毎に定位を動かせるため
        //  「左から右へ駆け上がる」立体的なライザーが作れる。
        Osc1Pan = 35, Osc2Pan, Osc3Pan, NoisePan,
        kNumCurves // = 39
    };

    // ソース(0-2=OSC1-3, 3=Noise) → PANカーブ番号
    static int panCurve(int src) noexcept
    {
        return Osc1Pan + juce::jlimit(0, 3, src);
    }

    // OSCソース(0-2) × ターゲット(0=Pitch 1=Level 2=Detune 3=Spread)
    static int oscCurve(int osc, int target) noexcept
    {
        return juce::jlimit(0, 2, osc) * 4 + juce::jlimit(0, 3, target);
    }
    // ノイズターゲット(0=Pitch 1=Level 2=Res)
    static int noiseCurve(int target) noexcept
    {
        return NoisePitch + juce::jlimit(0, 2, target);
    }

    static const char* name(int idx)
    {
        static const char* names[kNumCurves] = {
            "OSC1 PITCH", "OSC1 LEVEL", "OSC1 DETUNE", "OSC1 SPREAD",
            "OSC2 PITCH", "OSC2 LEVEL", "OSC2 DETUNE", "OSC2 SPREAD",
            "OSC3 PITCH", "OSC3 LEVEL", "OSC3 DETUNE", "OSC3 SPREAD",
            "NOISE PITCH", "NOISE LEVEL", "NOISE RES",
            "FILTER 1 CUTOFF", "FILTER 2 CUTOFF", "FILTER 3 CUTOFF", "FILTER 4 CUTOFF",
            "FILTER 1 RES",    "FILTER 2 RES",    "FILTER 3 RES",    "FILTER 4 RES",
            "SAT AMT", "SAT DRIVE",
            "CHORUS AMT", "CHORUS DEPTH",
            "DELAY AMT", "DELAY FB", "DELAY TIME",
            "REVERB AMT", "REVERB SHIMMER",
            "DUCK AMT", "DUCK RATE", "DUCK SHAPE",
            "OSC1 PAN", "OSC2 PAN", "OSC3 PAN", "NOISE PAN" };
        return names[juce::jlimit(0, kNumCurves - 1, idx)];
    }

    CurveStore() { resetToDefaults(); }

    // デフォルト:
    //  OSCピッチ = 0→1 上昇 (Start→Endへのライザー)
    //  ノイズピッチ/フィルター = 中央→上昇
    //  モジュレーション/FX = 中央フラット (変化なし)
    void resetToDefaults()
    {
        for (int i = 0; i < kNumCurves; ++i)
            publish(i, CurveSnapshot::makeDefault(0.5f, 0.5f));

        publish(Osc1Pitch, CurveSnapshot::makeDefault(0.0f, 1.0f));
        publish(Osc2Pitch, CurveSnapshot::makeDefault(0.0f, 1.0f));
        publish(Osc3Pitch, CurveSnapshot::makeDefault(0.0f, 1.0f));
        publish(NoisePitch, CurveSnapshot::makeDefault(0.5f, 1.0f));
        for (int f = Filter1; f <= Filter4; ++f)
            publish(f, CurveSnapshot::makeDefault(0.5f, 1.0f));
        for (int f = Filter1Res; f <= Filter4Res; ++f)
            publish(f, CurveSnapshot::makeDefault(0.5f, 0.5f));
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
        vt.setProperty("version", 2, nullptr);
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
        // v1 (9カーブ) はインデックス互換が無いため読み込まない
        if ((int)vt.getProperty("version", 1) < 2) return;
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
