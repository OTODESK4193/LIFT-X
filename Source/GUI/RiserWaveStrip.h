// ==========================================
// File: RiserWaveStrip.h
// 作成したライザーの波形表示 + WAVドラッグ&ドロップ書き出し
//
//  - ノートオンで録音開始 (プロセッサー側キャプチャ)、REC表示付きで
//    波形がリアルタイムに伸びていく
//  - リリース完了+テール1.5秒で確定 → ストリップをDAWへドラッグすると
//    32bit float WAV (セッションSR) としてドロップできる
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstring>
#include <vector>

#include "../PluginProcessor.h"
#include "ColorPalette.h"

class RiserWaveStrip : public juce::Component,
                       private juce::Timer
{
public:
    explicit RiserWaveStrip(LiftXAudioProcessor& p)
        : proc(p)
    {
        wavePath.preallocateSpace(kCols * 6 + 16);
        startTimerHz(6);
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour(LiftColors::bg.brighter(0.05f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(LiftColors::panelLine);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);

        const auto a = r.reduced(6.0f, 5.0f);

        if (peakLen <= 0)
        {
            g.setColour(LiftColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(11.5f)));
            g.drawText(juce::CharPointer_UTF8(
                "\xe3\x83\x8e\xe3\x83\xbc\xe3\x83\x88\xe3\x82\xaa\xe3\x83\xb3\xe3\x81\xa7\xe3\x83\xa9\xe3\x82\xa4\xe3\x82\xb6\xe3\x83\xbc\xe3\x82\x92\xe9\x8c\xb2\xe9\x9f\xb3"),
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
        g.setColour((rec ? LiftColors::rose : LiftColors::accentMaster).withAlpha(0.75f));
        g.fillPath(wavePath);

        // ステータス表示
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        if (rec)
        {
            g.setColour(LiftColors::rose);
            g.fillEllipse(a.getX() + 2.0f, a.getY() + 2.0f, 7.0f, 7.0f);
            g.drawText("REC", (int)a.getX() + 13, (int)a.getY(), 60, 12, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(LiftColors::text.withAlpha(0.85f));
            g.drawText(juce::CharPointer_UTF8("DRAG \xe2\x86\x92 WAV"),
                       getLocalBounds().reduced(8, 2), juce::Justification::topRight);
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

    juce::File writeWavFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("LIFT-X");
        dir.createDirectory();
        auto f = dir.getChildFile("LIFTX_Riser_" + juce::String(lastVersion) + ".wav");

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
                    const float* chans[2] = { ownL.data(), ownR.data() };
                    juce::AudioBuffer<float> buf(const_cast<float**>(chans), 2, n);
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
    int lastVersion = -1;
    int lastLiveLen = -1;
    bool dragging = false;
    bool exportDirty = false;

    std::vector<float> ownL, ownR;
    juce::Path wavePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RiserWaveStrip)
};
