#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "ColorPalette.h"

class FilterResponseDisplay : public juce::Component
{
public:
    FilterResponseDisplay()
    {
        setOpaque(false);
    }

    ~FilterResponseDisplay() override = default;

    void setParams(int type, float cutoffHz, float resQ, float liveCutoffHz, bool active)
    {
        if (filterType != type || std::abs(cutoff - cutoffHz) > 0.1f || 
            std::abs(res - resQ) > 0.01f || std::abs(liveCutoff - liveCutoffHz) > 0.1f ||
            isFilterActive != active)
        {
            filterType = type;
            cutoff = cutoffHz;
            res = resQ;
            liveCutoff = liveCutoffHz;
            isFilterActive = active;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(LiftColors::panel);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(LiftColors::panelLine);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

        auto area = bounds.reduced(8.0f, 6.0f);

        g.setColour(LiftColors::grid);
        const float freqGrid[] = { 100.0f, 1000.0f, 10000.0f };
        for (float f : freqGrid)
        {
            float x = freqToX(f, area);
            g.drawVerticalLine((int)x, area.getY(), area.getBottom());
        }
        float y0dB = dbToY(0.0f, area);
        g.setColour(LiftColors::grid.withAlpha(0.15f));
        g.drawHorizontalLine((int)y0dB, area.getX(), area.getRight());

        if (!isFilterActive)
        {
            g.setColour(LiftColors::textDim.withAlpha(0.4f));
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText("FILTER OFF", area, juce::Justification::centred);
            return;
        }

        juce::Path responsePath;
        const int numPoints = (int)area.getWidth();
        if (numPoints > 10)
        {
            bool first = true;
            for (int i = 0; i < numPoints; i += 2)
            {
                float x = area.getX() + (float)i;
                float f = xToFreq(x, area);
                float magDb = calculateMagnitudeDb(f, liveCutoff, res, filterType);
                float y = dbToY(magDb, area);

                if (first)
                {
                    responsePath.startNewSubPath(x, y);
                    first = false;
                }
                else
                {
                    responsePath.lineTo(x, y);
                }
            }

            juce::Path fillPath = responsePath;
            fillPath.lineTo(area.getRight(), area.getBottom());
            fillPath.lineTo(area.getX(), area.getBottom());
            fillPath.closeSubPath();

            g.setColour(LiftColors::accentFilter.withAlpha(0.15f));
            g.fillPath(fillPath);

            g.setColour(LiftColors::accentFilter);
            g.strokePath(responsePath, juce::PathStrokeType(1.8f));
        }

        float xBaseCut = freqToX(cutoff, area);
        g.setColour(LiftColors::accentFilter.withAlpha(0.5f));
        g.drawVerticalLine((int)xBaseCut, area.getY(), area.getBottom());

        if (std::abs(liveCutoff - cutoff) > 1.0f)
        {
            // ライブカットオフ位置の破線。
            //  createDashedStroke は「元パス」と「出力先」に別のオブジェクトを
            //  渡す必要がある (以前は同一の Path を両方に渡していた)。
            //  また出力は既にストローク済みの塗り形状なので fillPath で描く
            //  (strokePath で再度縁取ると線が二重に太る)。
            const float xLiveCut = freqToX(liveCutoff, area);
            juce::Path srcLine, dashedLine;
            srcLine.startNewSubPath(xLiveCut, area.getY());
            srcLine.lineTo(xLiveCut, area.getBottom());

            const juce::PathStrokeType stroke(1.5f);
            const float dashes[] = { 3.0f, 3.0f };
            stroke.createDashedStroke(dashedLine, srcLine, dashes, 2);

            g.setColour(LiftColors::text.withAlpha(0.8f));
            g.fillPath(dashedLine);
        }

        g.setColour(LiftColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText("100", (int)freqToX(100.0f, area) - 10, (int)area.getBottom() - 10, 20, 10, juce::Justification::centred);
        g.drawText("1k", (int)freqToX(1000.0f, area) - 10, (int)area.getBottom() - 10, 20, 10, juce::Justification::centred);
        g.drawText("10k", (int)freqToX(10000.0f, area) - 10, (int)area.getBottom() - 10, 20, 10, juce::Justification::centred);
    }

private:
    float freqToX(float f, const juce::Rectangle<float>& area) const
    {
        const float minF = 20.0f;
        const float maxF = 20000.0f;
        float norm = (std::log2(juce::jlimit(minF, maxF, f)) - std::log2(minF)) / (std::log2(maxF) - std::log2(minF));
        return area.getX() + norm * area.getWidth();
    }

    float xToFreq(float x, const juce::Rectangle<float>& area) const
    {
        const float minF = 20.0f;
        const float maxF = 20000.0f;
        float norm = juce::jlimit(0.0f, 1.0f, (x - area.getX()) / area.getWidth());
        return std::exp2(std::log2(minF) + norm * (std::log2(maxF) - std::log2(minF)));
    }

    float dbToY(float db, const juce::Rectangle<float>& area) const
    {
        const float minDb = -36.0f;
        const float maxDb = +24.0f;
        float norm = (juce::jlimit(minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return area.getBottom() - norm * area.getHeight();
    }

    // 2次フィルターのバンドパス振幅 (Vowel の各フォルマントに使う)
    static float bandPassMag(float f, float fc, float q)
    {
        const float w = f / juce::jmax(1.0f, fc);
        float denom = (1.0f - w * w) * (1.0f - w * w) + (w / q) * (w / q);
        denom = juce::jmax(1.0e-6f, denom);
        return (w / q) / std::sqrt(denom);
    }

    float calculateMagnitudeDb(float f, float fc, float Q, int type) const
    {
        float w = f / juce::jmax(1.0f, fc);
        float q = juce::jmax(0.5f, Q);

        float denom = (1.0f - w * w) * (1.0f - w * w) + (w / q) * (w / q);
        denom = juce::jmax(1.0e-6f, denom);

        float mag = 1.0f;
        switch (type)
        {
            case 0: mag = 1.0f / std::sqrt(denom); break;
            case 1: mag = (w * w) / std::sqrt(denom); break;
            case 2: mag = (w / q) / std::sqrt(denom); break;
            case 3: mag = std::abs(1.0f - w * w) / std::sqrt(denom); break;

            case 4:   // ---- Vowel: 3フォルマントの合成 ----
            {
                // DSP側と同じマッピング: CUTOFF の対数位置 → A..U のモーフ量
                static constexpr float kF[5][3] = {
                    { 700.0f, 1220.0f, 2600.0f }, { 400.0f, 1700.0f, 2600.0f },
                    { 250.0f, 1750.0f, 2600.0f }, { 400.0f,  750.0f, 2400.0f },
                    { 250.0f,  600.0f, 2400.0f } };
                static constexpr float kG[3] = { 1.0f, 0.62f, 0.28f };

                const float pos = juce::jlimit(0.0f, 1.0f,
                    (std::log2(juce::jmax(20.0f, fc)) - std::log2(80.0f))
                    / (std::log2(8000.0f) - std::log2(80.0f)));
                const float fp = pos * 4.0f;
                const int i0 = juce::jlimit(0, 4, (int)fp);
                const int i1 = juce::jmin(4, i0 + 1);
                const float t = fp - (float)i0;
                const float fq = juce::jlimit(1.0f, 14.0f, q * 1.4f);

                mag = 0.0f;
                for (int b = 0; b < 3; ++b)
                {
                    const float ff = std::exp2(std::log2(kF[i0][b]) * (1.0f - t)
                                             + std::log2(kF[i1][b]) * t);
                    mag += bandPassMag(f, ff, fq) * kG[b];
                }
                mag *= 0.8f;
                break;
            }

            case 5:   // ---- Comb: |1 / (1 - fb·z^-D)| ----
            {
                // D = sr/fc なので wD = 2π·f/fc。基音の倍数ごとにピークが立つ。
                const float fbAmt = juce::jlimit(0.0f, 0.97f,
                    (juce::jlimit(0.5f, 12.0f, Q) - 0.5f) / 11.5f * 0.97f);
                const float wd = juce::MathConstants<float>::twoPi * f / juce::jmax(20.0f, fc);
                const float d = 1.0f - 2.0f * fbAmt * std::cos(wd) + fbAmt * fbAmt;
                mag = (1.0f - fbAmt * 0.65f) / std::sqrt(juce::jmax(1.0e-6f, d));
                break;
            }

            default: mag = 1.0f / std::sqrt(denom); break;
        }

        float db = 20.0f * std::log10(juce::jmax(1.0e-4f, mag));
        return juce::jlimit(-36.0f, 24.0f, db);
    }

    int filterType = 0;
    float cutoff = 1000.0f;
    float res = 0.9f;
    float liveCutoff = 1000.0f;
    bool isFilterActive = true;
};
