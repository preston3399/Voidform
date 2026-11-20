#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "Renderer.h"

// The GL surface. Double-click toggles the control panel (handy for screen-recording just the visual).
class VisualComponent : public juce::Component
{
public:
    explicit VisualComponent(VoidformProcessor& p) : renderer(p, context, *this)
    {
        setOpaque(true);
#if JUCE_MAJOR_VERSION >= 9
        context.setPreferredVersion(juce::OpenGLVersion(3, 2));
#else
        context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
#endif
        context.setRenderer(&renderer);
        context.setContinuousRepainting(true);
        context.setComponentPaintingEnabled(false);
        context.attachTo(*this);
        context.setSwapInterval(1);
    }
    ~VisualComponent() override { context.detach(); }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }

    std::function<void()> onDoubleClick;

private:
    juce::OpenGLContext context;
    Renderer renderer;
};

// Spectrum + the four band windows, so you can see what each band is listening to.
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView(VoidformProcessor& p) : proc(p) { startTimerHz(30); }
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    float xForFreq(float f) const;
    VoidformProcessor& proc;
};

// Scrollable list of all parameter controls.
class ControlPanel : public juce::Component
{
public:
    explicit ControlPanel(juce::AudioProcessorValueTreeState&);
    void resized() override;
    int getContentHeight() const { return contentHeight; }

private:
    struct Row
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        std::unique_ptr<juce::Label> header;
    };
    void section(const juce::String& title);
    void slider(const juce::String& id, const juce::String& text, const juce::String& suffix = {});
    void choice(const juce::String& id, const juce::String& text);
    void toggle(const juce::String& id, const juce::String& text);

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<Row>> rows;
    int contentHeight = 0;
};

class VoidformEditor : public juce::AudioProcessorEditor,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::AsyncUpdater
{
public:
    explicit VoidformEditor(VoidformProcessor&);
    ~VoidformEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { resized(); repaint(); }
    bool frameOn() const { return proc.apvts.getRawParameterValue("frame")->load() > 0.5f; }
    int aspectMode() const { return (int) std::lround(proc.apvts.getRawParameterValue("aspect")->load()); }
    juce::Rectangle<int> fitToStage(juce::Rectangle<int> stage) const;
    void applyPreset(int index);
    void setUiHidden(bool);

    VoidformProcessor& proc;
    juce::LookAndFeel_V4 laf;
    VisualComponent visual;
    SpectrumView spectrum;
    ControlPanel panel;
    juce::Viewport viewport;
    juce::ComboBox presetBox, aspectBox;
    juce::Label presetLabel, aspectLabel;
    juce::TextButton hideButton { "Hide UI" };
    juce::ToggleButton frameToggle { "Frame" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frameAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aspectAttach;
    juce::Label crtLabel;
    juce::Slider crtSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> crtAttach;
    juce::Rectangle<int> stageArea;
    juce::var presets;
    bool uiHidden = false;
    static constexpr int panelWidth = 360;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoidformEditor)
};
