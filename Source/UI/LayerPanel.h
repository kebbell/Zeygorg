#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace ui
{

// Shows a layer's sample (with draggable start/end and the playhead) or its
// oscillator shape.
class WaveformView : public juce::Component, public juce::SettableTooltipClient
{
public:
    WaveformView (ZeygorgProcessor& p, int layer);

    void update();                 // called from the panel's timer
    void setDropHighlight (bool b) { if (b != dropHighlight) { dropHighlight = b; repaint(); } }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void()> onEmptyClick;   // e.g. open the browser

private:
    juce::Rectangle<float> waveArea() const;
    float xFor (float norm) const;
    float normFor (float x) const;
    int handleAt (juce::Point<float>) const;
    void rebuildPeaks();
    void paintSample (juce::Graphics&, juce::Rectangle<float>);
    void paintOscillator (juce::Graphics&, juce::Rectangle<float>, int wave);

    ZeygorgProcessor& processor;
    int layer;
    rf::Sample::Ptr sample;
    std::vector<std::pair<float, float>> peaks;   // min/max per column
    float start = 0.0f, end = 1.0f, playhead = -1.0f;
    int wave = 6, unison = 1;
    float detune = 0.0f;
    bool loop = false, reverse = false, dropHighlight = false, missing = false;
    int hover = -1, dragging = -1;
};

// One synth layer: power, wave, the waveform view, sample browser and knobs.
// Accepts audio files dropped onto it.
class LayerPanel : public juce::Component,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
{
public:
    LayerPanel (ZeygorgProcessor& p, int layer);

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void stepSample (int delta);
    void selectLibrary (int index);
    void showBrowser();
    void chooseFile();
    void loadFile (const juce::File&);
    juce::String param (LayerParam::Field f) const { return LayerParam::id (layer, f); }

    ZeygorgProcessor& processor;
    APVTS& state;
    const int layer;

    juce::ToggleButton power;
    APVTS::ButtonAttachment powerAttachment;
    juce::ComboBox waveBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> waveAttachment;

    WaveformView view;
    Button prev { juce::CharPointer_UTF8 ("\xe2\x80\xb9"), "Previous sample" };
    Button next { juce::CharPointer_UTF8 ("\xe2\x80\xba"), "Next sample" };
    Button browse { "", "Browse the built-in samples" };
    Button load { "Load", "Load an audio file (or drag one onto the layer)" };

    Knob level, pan, octave, semi, fine, unison, detune, spread, speed, grain;
    Choice warp, sync;
    Toggle loop, reverse;

    std::unique_ptr<juce::FileChooser> chooser;
    bool dragOver = false;
    juce::String shownName;
};

} // namespace ui
