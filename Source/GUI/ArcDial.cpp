// ==========================================
// File: ArcDial.cpp
// ==========================================
#include "ArcDial.h"
#include "ColorPalette.h"
#include <cmath>

ArcDialLookAndFeel::ArcDialLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, LiftColors::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, LiftColors::panel);
    setColour(juce::ComboBox::textColourId, LiftColors::text);
    setColour(juce::ComboBox::outlineColourId, LiftColors::panelLine);
    setColour(juce::ComboBox::arrowColourId, LiftColors::textDim);
    setColour(juce::PopupMenu::backgroundColourId, LiftColors::panel);
    setColour(juce::PopupMenu::textColourId, LiftColors::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, LiftColors::lavender.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, LiftColors::text);
    setColour(juce::ToggleButton::textColourId, LiftColors::text);
    setColour(juce::ToggleButton::tickColourId, LiftColors::mint);
    setColour(juce::ToggleButton::tickDisabledColourId, LiftColors::textDim);
    setColour(juce::Label::textColourId, LiftColors::textDim);
    setColour(juce::TextButton::buttonColourId, LiftColors::knobTrack);
    setColour(juce::TextButton::textColourOffId, LiftColors::text);
    setColour(juce::TextButton::textColourOnId, LiftColors::text);
}

void ArcDialLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
    const auto centreX = (float)x + (float)width * 0.5f;
    const auto centreY = (float)y + (float)height * 0.5f;
    const auto rx = centreX - radius;
    const auto ry = centreY - radius;
    const auto rw = radius * 2.0f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto arcThickness = 5.0f;

    // 1. 背景トラック
    g.setColour(LiftColors::knobTrack);
    g.drawEllipse(rx, ry, rw, rw, arcThickness);

    // 2. 値アーク (セクション色のグラデーション)
    juce::Path p;
    p.addArc(rx, ry, rw, rw, rotaryStartAngle, angle, true);

    const auto baseColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const auto lightColour = baseColour.brighter(0.6f);
    const auto darkColour = baseColour.darker(0.35f);

    juce::ColourGradient gradient(darkColour, rx, centreY, lightColour, rx + rw, centreY, false);
    g.setGradientFill(gradient);
    g.strokePath(p, juce::PathStrokeType(arcThickness, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    // 3. ソフトグロー
    g.setColour(baseColour.withAlpha(0.15f));
    g.strokePath(p, juce::PathStrokeType(arcThickness + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 4. ポインター
    juce::Path p2;
    const auto pointerLength = radius * 0.4f;
    p2.addRoundedRectangle(-1.5f, -radius + 1.5f, 3.0f, pointerLength, 1.5f);
    p2.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(LiftColors::text);
    g.fillPath(p2);
}
