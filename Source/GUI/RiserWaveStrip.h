// ==========================================
// File: RiserWaveStrip.h
// 作成したライザーの波形表示 + WAVドラッグ&ドロップ書き出し
//
//  - ノートオンで録音開始 (プロセッサー側キャプチャ)、REC表示付きで
//    波形がリアルタイムに伸びていく
//  - 本編(指定小節分)を録り終えると、以降はFXが実際に鳴り止むまでテールを収録。
//    波形上ではテール区間を暗く描き、境界に縦線を引いて区別できるようにする。
//  - 確定後、ストリップをDAWへドラッグすると 32bit float WAV (セッションSR)
//    としてドロップできる。末尾には 8ms のフェードアウトを掛けてブツ切れを防ぐ。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "../PluginProcessor.h"
#include "ColorPalette.h"

class RiserWaveStrip : public juce::Component,
                       private juce::Timer
{
public:
    // ---- 点灯式トグルボタン (LED + グロー枠) ----
    class NormToggle : public juce::ToggleButton
    {
    public:
        NormToggle() : juce::ToggleButton("NORM") {}

        void paintButton(juce::Graphics& g, bool highlighted, bool /*down*/) override
        {
            const auto r = getLocalBounds().toFloat().reduced(0.5f);
            const bool on = getToggleState();

            // 背景
            g.setColour(on ? LiftColors::mint.withAlpha(0.18f)
                           : (highlighted ? LiftColors::knobTrack.brighter(0.12f) : LiftColors::knobTrack));
            g.fillRoundedRectangle(r, 4.0f);

            // 枠
            g.setColour(on ? LiftColors::mint.withAlpha(0.90f) : LiftColors::panelLine);
            g.drawRoundedRectangle(r, 4.0f, on ? 1.2f : 1.0f);

            // LED インジケーター (左側)
            const float ledX = r.getX() + 7.5f;
            const float ledY = r.getCentreY();
            if (on)
            {
                // LED グロー
                g.setColour(LiftColors::mint.withAlpha(0.35f));
                g.fillEllipse(ledX - 5.0f, ledY - 5.0f, 10.0f, 10.0f);
                // LED コア
                g.setColour(LiftColors::mint);
                g.fillEllipse(ledX - 2.5f, ledY - 2.5f, 5.0f, 5.0f);
                // コアハイライト
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.fillEllipse(ledX - 1.0f, ledY - 1.0f, 2.0f, 2.0f);
            }
            else
            {
                // 消灯 LED
                g.setColour(LiftColors::textDim.withAlpha(0.35f));
                g.fillEllipse(ledX - 2.0f, ledY - 2.0f, 4.0f, 4.0f);
            }

            // テキスト "NORM"
            g.setColour(on ? LiftColors::text : LiftColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText("NORM", (int)(ledX + 6.0f), 0,
                       getWidth() - (int)(ledX + 7.0f), getHeight(),
                       juce::Justification::centred);
        }
    };

    explicit RiserWaveStrip(LiftXAudioProcessor& p)
        : proc(p)
    {
        wavePath.preallocateSpace(kCols * 6 + 16);

        // ---- NORM: 書き出し時に -0.3dBFS へノーマライズ ----
        //  ライザーは設定次第でピークがばらつくので、DAWへ貼った直後の
        //  音量が揃っていると即戦力度が上がる。設定はグローバルへ永続化。
        normBtn.setClickingTogglesState(true);
        normBtn.setTooltip("Normalize the exported WAV to -0.3 dBFS");
        normBtn.setToggleState(
            LiftXAudioProcessor::getGlobalSettings().getBoolValue("normalizeExport", false),
            juce::dontSendNotification);
        normBtn.onClick = [this]
        {
            auto& gs = LiftXAudioProcessor::getGlobalSettings();
            gs.setValue("normalizeExport", normBtn.getToggleState());
            gs.saveIfNeeded();
            exportDirty = true;      // 設定が変わったので書き出し直す
        };
        addAndMakeVisible(normBtn);

        startTimerHz(6);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(5, 4);
        normBtn.setBounds(r.removeFromTop(16).removeFromRight(54));
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        LiftColors::paintWell(g, r, 6.0f);

        const auto a = r.reduced(6.0f, 5.0f);

        if (peakLen <= 0)
        {
            g.setColour(LiftColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(11.5f)));
            g.drawText("Play a note to record the riser",
                       getLocalBounds(), juce::Justification::centred);
            return;
        }

        // 波形 (min/maxピーク)
        wavePath.clear();
        const float cy = a.getCentreY();
        const float hh = a.getHeight() * 0.5f;
        const float cw = a.getWidth() / (float)kCols;
        wavePath.startNewSubPath(a.getX(), cy - juce::jlimit(-1.0f, 1.0f, peakMax[0]) * hh);
        for (int c = 1; c < kCols; ++c)
            wavePath.lineTo(a.getX() + cw * (float)c, cy - juce::jlimit(-1.0f, 1.0f, peakMax[(size_t)c]) * hh);
        for (int c = kCols - 1; c >= 0; --c)
            wavePath.lineTo(a.getX() + cw * (float)c, cy - juce::jlimit(-1.0f, 1.0f, peakMin[(size_t)c]) * hh);
        wavePath.closeSubPath();

        const bool rec = proc.isCapturing();
        const auto waveCol = (rec ? LiftColors::rose : LiftColors::accentMaster);

        // テール区間 (本編終端より後ろ) は暗く塗り分ける
        const float bodyT = (peakLen > 0 && peakBodyEnd > 0)
                          ? juce::jlimit(0.0f, 1.0f, (float)peakBodyEnd / (float)peakLen)
                          : 1.0f;
        const float bodyX = a.getX() + a.getWidth() * bodyT;

        if (bodyT < 0.999f)
        {
            // テール側の背景をわずかに沈める
            g.setColour(LiftColors::bg.withAlpha(0.35f));
            g.fillRect(juce::Rectangle<float>(bodyX, a.getY(), a.getRight() - bodyX, a.getHeight()));
        }

        // 本編部分 (フル彩度)
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(juce::Rectangle<float>(a.getX(), r.getY(),
                                                      bodyX - a.getX(), r.getHeight()).toNearestInt());
            g.setColour(waveCol.withAlpha(0.78f));
            g.fillPath(wavePath);
        }
        // テール部分 (減光)
        if (bodyT < 0.999f)
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(juce::Rectangle<float>(bodyX, r.getY(),
                                                      a.getRight() - bodyX, r.getHeight()).toNearestInt());
            g.setColour(waveCol.withAlpha(0.40f));
            g.fillPath(wavePath);

            // 境界線
            g.setColour(LiftColors::text.withAlpha(0.45f));
            g.drawLine(bodyX, a.getY(), bodyX, a.getBottom(), 1.0f);
        }

        // ステータス表示
        g.setFont(LiftFonts::mono(10.5f, true));
        if (rec)
        {
            const bool inTail = (peakBodyEnd > 0);
            g.setColour(inTail ? LiftColors::peach : LiftColors::rose);
            g.fillEllipse(a.getX() + 2.0f, a.getY() + 2.0f, 7.0f, 7.0f);
            g.drawText(inTail ? "REC TAIL" : "REC",
                       (int)a.getX() + 13, (int)a.getY(), 70, 12, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(LiftColors::text.withAlpha(0.85f));
            const double sec = peakLen / juce::jmax(1.0, proc.getPreparedSampleRate());
            g.drawText("DRAG > WAV  " + juce::String(sec, 1) + "s",
                       getLocalBounds().reduced(8, 2).withTrimmedRight(60),
                       juce::Justification::topRight);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragging || peakLen <= 0 || proc.isCapturing() || ownL.empty())
            return;
        if (e.getDistanceFromDragStart() < 8)
            return;

        dragging = true;
        const auto f = writeWavFile();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles(
                { f.getFullPathName() }, false);
    }

    void mouseUp(const juce::MouseEvent&) override { dragging = false; }

private:
    static constexpr int kCols = 220;

    void timerCallback() override
    {
        const int ver = proc.getCaptureVersion();
        const int len = proc.getCaptureLength();
        const bool rec = proc.isCapturing();

        if (rec)
        {
            // 録音中はプロセッサーバッファから直接ピーク更新 (表示専用)
            if (len != lastLiveLen)
            {
                lastLiveLen = len;
                peakBodyEnd = proc.getCaptureBodyEnd();
                rebuildPeaks(proc.getCaptureL(), proc.getCaptureR(), len);
                repaint();
            }
            return;
        }

        if (ver != lastVersion)
        {
            lastVersion = ver;
            lastLiveLen = -1;

            // 確定 → エクスポート用にローカルコピー
            const int n = proc.getCaptureLength();
            ownL.assign((size_t)juce::jmax(0, n), 0.0f);
            ownR.assign((size_t)juce::jmax(0, n), 0.0f);
            if (n > 0)
            {
                std::memcpy(ownL.data(), proc.getCaptureL(), (size_t)n * sizeof(float));
                std::memcpy(ownR.data(), proc.getCaptureR(), (size_t)n * sizeof(float));
            }
            peakBodyEnd = juce::jlimit(0, n, proc.getCaptureBodyEnd());
            exportDirty = true;
            rebuildPeaks(ownL.data(), ownR.data(), n);
            repaint();
        }
    }

    void rebuildPeaks(const float* l, const float* r, int len)
    {
        peakLen = len;
        if (len <= 0) return;

        for (int c = 0; c < kCols; ++c)
        {
            const int i0 = (int)((juce::int64)c * len / kCols);
            const int i1 = juce::jmax(i0 + 1, (int)((juce::int64)(c + 1) * len / kCols));
            float mn = 0.0f, mx = 0.0f;
            for (int i = i0; i < i1 && i < len; ++i)
            {
                const float v = (l[i] + r[i]) * 0.5f;
                mn = juce::jmin(mn, v);
                mx = juce::jmax(mx, v);
            }
            peakMin[(size_t)c] = mn;
            peakMax[(size_t)c] = mx;
        }
    }

    // ファイル名: LIFTX_<プリセット名>_<BPM>bpm_<長さ>s.wav
    //  DAWのプールで見分けが付くように、内容が分かる名前にする。
    juce::String makeFileName() const
    {
        auto nm = proc.getCurrentPresetName().trim();
        if (nm.isEmpty()) nm = "Riser";
        nm = juce::File::createLegalFileName(nm).removeCharacters(" ");

        const double sr = proc.getPreparedSampleRate();
        const double sec = (double)ownL.size() / juce::jmax(1.0, sr);
        const int bpm = (int)std::round(proc.getLastBpm());

        return "LIFTX_" + nm + "_" + juce::String(bpm) + "bpm_"
             + juce::String(sec, 1) + "s"
             + (normBtn.getToggleState() ? "_norm" : "")
             + "_" + juce::String(lastVersion) + ".wav";
    }

    juce::File writeWavFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("LIFT-X");
        dir.createDirectory();
        auto f = dir.getChildFile(makeFileName());

        if (exportDirty || !f.existsAsFile())
        {
            f.deleteFile();
            juce::WavAudioFormat fmt;
            if (auto os = f.createOutputStream())
            {
                if (auto* writer = fmt.createWriterFor(os.get(),
                        proc.getPreparedSampleRate(), 2, 32, {}, 0))
                {
                    os.release(); // 以後 writer がストリームを所有
                    std::unique_ptr<juce::AudioFormatWriter> w(writer);
                    const int n = (int)ownL.size();

                    // 末尾 8ms に線形フェードを掛けてブツ切れを防ぐ。
                    // ownL/ownR はエクスポート専用のローカルコピーなので破壊してよいが、
                    // 二重適用を避けるため作業用バッファへコピーしてから処理する。
                    juce::AudioBuffer<float> buf(2, n);
                    buf.copyFrom(0, 0, ownL.data(), n);
                    buf.copyFrom(1, 0, ownR.data(), n);

                    // ---- ノーマライズ (-0.3 dBFS) ----
                    //  フェードを掛ける「前」に行う。後だと末尾のフェード部分が
                    //  ピーク判定に混ざって正しく揃わない。
                    if (normBtn.getToggleState())
                    {
                        const float peak = juce::jmax(buf.getMagnitude(0, 0, n),
                                                      buf.getMagnitude(1, 0, n));
                        if (peak > 1.0e-6f)
                            buf.applyGain(juce::Decibels::decibelsToGain(-0.3f) / peak);
                    }

                    const int fade = juce::jmin(n / 4,
                        (int)(proc.getPreparedSampleRate() * 0.008));
                    if (fade > 1)
                    {
                        buf.applyGainRamp(0, n - fade, fade, 1.0f, 0.0f);
                        // 先頭にも 1ms のフェードインを入れて DC 段差を消す
                        const int fin = juce::jmin(fade, (int)(proc.getPreparedSampleRate() * 0.001));
                        if (fin > 1) buf.applyGainRamp(0, 0, fin, 0.0f, 1.0f);
                    }

                    w->writeFromAudioSampleBuffer(buf, 0, n);
                    exportDirty = false;
                }
            }
        }
        return f;
    }

    LiftXAudioProcessor& proc;

    std::array<float, kCols> peakMin {}, peakMax {};
    int peakLen = 0;
    int peakBodyEnd = 0;      // 本編終端 (0 = まだ本編中)
    int lastVersion = -1;
    int lastLiveLen = -1;
    bool dragging = false;
    bool exportDirty = false;

    std::vector<float> ownL, ownR;
    juce::Path wavePath;
    NormToggle normBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RiserWaveStrip)
};
