#include "PluginEditor.h"
#include <BinaryData.h>
#include "FrameInfo.h"

// ============================================================================================
float SpectrumView::xForFreq(float f) const
{
    return (float) getWidth() * std::log(std::max(f, 20.0f) / 20.0f) / std::log(1000.0f);
}

void SpectrumView::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0d0d10));
    g.fillRoundedRectangle(b, 5.0f);

    const juce::Colour bandCol[4] = { juce::Colour(0xffff5d6c), juce::Colour(0xffffb347), juce::Colour(0xff5dd6a2), juce::Colour(0xff5db8ff) };

    // band windows
    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String(i + 1);
        const float f = proc.apvts.getRawParameterValue("b" + n + "f")->load();
        const float w = proc.apvts.getRawParameterValue("b" + n + "w")->load();
        const float half = std::pow(2.0f, w * 0.5f);
        const float x0 = juce::jlimit(0.0f, b.getWidth(), xForFreq(f / half));
        const float x1 = juce::jlimit(0.0f, b.getWidth(), xForFreq(f * half));
        const float lvl = proc.bandEnv[(size_t) i].load();
        g.setColour(bandCol[i].withAlpha(0.12f + 0.28f * juce::jlimit(0.0f, 1.0f, lvl)));
        g.fillRect(x0, 0.0f, std::max(2.0f, x1 - x0), b.getHeight());
        g.setColour(bandCol[i]);
        g.drawLine(xForFreq(f), 0, xForFreq(f), b.getHeight(), 1.0f);
        g.setFont(11.0f);
        g.drawText(n, (int) xForFreq(f) + 3, 2, 14, 12, juce::Justification::left);
    }

    // spectrum bars
    const int nb = vf::Analyzer::numDisplay;
    const float bw = b.getWidth() / (float) nb;
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    for (int i = 0; i < nb; ++i)
    {
        const float v = proc.spectrum[(size_t) i].load();
        const float h = v * (b.getHeight() - 4.0f);
        g.fillRect(bw * (float) i + 0.5f, b.getHeight() - h - 2.0f, std::max(1.0f, bw - 1.0f), h);
    }

    // frequency ticks
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.setFont(10.0f);
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawText(f >= 1000 ? juce::String((int) (f / 1000)) + "k" : juce::String((int) f), (int) xForFreq(f) - 14, getHeight() - 12, 28, 10, juce::Justification::centred);
}

// ============================================================================================
ControlPanel::ControlPanel(juce::AudioProcessorValueTreeState& s) : apvts(s)
{
    section("COLOR");
    slider("hueA", "Color A hue");        slider("satA", "Color A saturation");
    slider("hueB", "Color B hue");        slider("satB", "Color B saturation");
    slider("colorMix", "A / B mix");      slider("brightness", "Brightness");
    slider("bg", "Background / paper");   toggle("ink", "Ink-on-paper mode");

    section("MATERIAL");
    slider("metallic", "Metallic");       slider("reflect", "Reflectivity");
    slider("bloom", "Bloom");             slider("grain", "Grain");
    slider("chroma", "Chromatic fringe"); slider("glitch", "Glitch");
    slider("vignette", "Vignette");

    section("PARTICLES");
    choice("shape", "Shape");             choice("pstyle", "Particle style");
    slider("count", "Particle count");    slider("size", "Particle size", " px");
    slider("fuzz", "Fuzz / thickness");   slider("mesh", "Wire mesh");
    slider("filaments", "Filaments");     slider("filLen", "Filament length");
    slider("wind", "Filament wind");      slider("trails", "Trails");

    section("SHAPE & MOTION");
    slider("zoom", "Zoom");               slider("fit", "Auto-fit to window");               slider("warp", "Warp");
    slider("detail", "Detail");           slider("speed", "Flow speed");
    slider("spin", "Spin");               slider("tilt", "Tilt");

    section("AUDIO RESPONSE");
    slider("sens", "Sensitivity");
    slider("react", "Reactivity");
    slider("punch", "Punch (explosion)");
    slider("spinKick", "Spin kick (on hits)");
    slider("shake", "Camera shake");
    slider("attack", "Attack", " ms");    slider("release", "Release", " ms");
    for (int i = 1; i <= 4; ++i)
    {
        const auto n = juce::String(i);
        section("BAND " + n);
        slider("b" + n + "f", "Frequency", " Hz");
        slider("b" + n + "w", "Width", " oct");
        slider("b" + n + "g", "Gain");
        choice("b" + n + "t", "Affects");
    }
}

void ControlPanel::section(const juce::String& title)
{
    auto r = std::make_unique<Row>();
    r->header = std::make_unique<juce::Label>(juce::String(), title);
    r->header->setFont(juce::FontOptions(12.0f, juce::Font::bold));
    r->header->setColour(juce::Label::textColourId, juce::Colour(0xff8f95ff));
    addAndMakeVisible(*r->header);
    rows.push_back(std::move(r));
}

void ControlPanel::slider(const juce::String& id, const juce::String& text, const juce::String& suffix)
{
    auto r = std::make_unique<Row>();
    r->label = std::make_unique<juce::Label>(juce::String(), text);
    r->label->setFont(juce::FontOptions(13.0f));
    r->slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    r->slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 20);
    r->slider->setTextValueSuffix(suffix);
    r->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, *r->slider);
    {   // the attachment installs its own text formatting, so override it afterwards
        const int dp = (id == "count" || suffix == " Hz" || suffix == " ms") ? 0 : 2;
        r->slider->textFromValueFunction = [dp](double v) { return juce::String(v, dp); };
        r->slider->valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
        r->slider->updateText();
    }
    addAndMakeVisible(*r->label);
    addAndMakeVisible(*r->slider);
    rows.push_back(std::move(r));
}

void ControlPanel::choice(const juce::String& id, const juce::String& text)
{
    auto r = std::make_unique<Row>();
    r->label = std::make_unique<juce::Label>(juce::String(), text);
    r->label->setFont(juce::FontOptions(13.0f));
    r->combo = std::make_unique<juce::ComboBox>();
    r->combo->addItemList(vf::targetNames(), 1);
    r->ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, id, *r->combo);
    addAndMakeVisible(*r->label);
    addAndMakeVisible(*r->combo);
    rows.push_back(std::move(r));
}

void ControlPanel::toggle(const juce::String& id, const juce::String& text)
{
    auto r = std::make_unique<Row>();
    r->toggle = std::make_unique<juce::ToggleButton>(text);
    r->ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, id, *r->toggle);
    addAndMakeVisible(*r->toggle);
    rows.push_back(std::move(r));
}

void ControlPanel::resized()
{
    int y = 6;
    const int w = getWidth();
    for (auto& r : rows)
    {
        if (r->header)      { y += 8; r->header->setBounds(8, y, w - 16, 20); y += 24; }
        else if (r->toggle) { r->toggle->setBounds(8, y, w - 16, 24); y += 28; }
        else
        {
            r->label->setBounds(8, y, 118, 24);
            if (r->slider) r->slider->setBounds(124, y, w - 132, 24);
            if (r->combo)  r->combo->setBounds(124, y + 1, w - 140, 22);
            y += 28;
        }
    }
    contentHeight = y + 10;
}

// ============================================================================================
VoidformEditor::VoidformEditor(VoidformProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), visual(p), spectrum(p), panel(p.apvts)
{
    laf.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
    laf.setColour(juce::Slider::thumbColourId, juce::Colour(0xff8f95ff));
    laf.setColour(juce::Slider::trackColourId, juce::Colour(0xff5057d9));
    setLookAndFeel(&laf);

    presets = juce::JSON::parse(juce::String::fromUTF8(BinaryData::presets_json, BinaryData::presets_jsonSize));
    feels   = juce::JSON::parse(juce::String::fromUTF8(BinaryData::feels_json, BinaryData::feels_jsonSize));
    loadUserLooks();

    addAndMakeVisible(visual);
    visual.onDoubleClick = [this] { setUiHidden(!uiHidden); };

    presetLabel.setText("Look", juce::dontSendNotification);
    addAndMakeVisible(presetLabel);
    presetBox.setTextWhenNothingSelected("Load a look...");
    rebuildLookBox();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1000) { if (id - 1000 < userLooks.size()) applyValues(userLooks.getReference(id - 1000)["values"]); }
        else applyPreset(id - 1);
    };
    addAndMakeVisible(presetBox);

    feelLabel.setText("Feel", juce::dontSendNotification);
    addAndMakeVisible(feelLabel);
    feelBox.setTextWhenNothingSelected("Pick a track style (spin / punch)...");
    if (auto* arr = feels.getArray())
        for (int i = 0; i < arr->size(); ++i) feelBox.addItem(arr->getReference(i)["name"].toString(), i + 1);
    feelBox.onChange = [this] { applyFeel(feelBox.getSelectedId() - 1); };
    addAndMakeVisible(feelBox);
    saveButton.setTooltip("Save the current sliders as your own look");
    saveButton.onClick = [this] { saveLookDialog(); };
    addAndMakeVisible(saveButton);

    hideButton.setTooltip("Hide the controls (double-click the visual to bring them back)");
    hideButton.onClick = [this] { setUiHidden(true); };
    addAndMakeVisible(hideButton);

    frameAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "frame", frameToggle);
    addAndMakeVisible(frameToggle);
    aspectLabel.setText("Aspect", juce::dontSendNotification);
    addAndMakeVisible(aspectLabel);
    aspectBox.addItemList({ "Free", "16:9", "9:16", "1:1" }, 1);
    aspectAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "aspect", aspectBox);
    addAndMakeVisible(aspectBox);
    crtLabel.setText("CRT", juce::dontSendNotification);
    addAndMakeVisible(crtLabel);
    crtSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    crtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "crt", crtSlider);
    crtSlider.textFromValueFunction = [](double v) { return juce::String(v, 2); };
    crtSlider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
    crtSlider.updateText();
    addAndMakeVisible(crtSlider);
    proc.apvts.addParameterListener("frame", this);
    proc.apvts.addParameterListener("aspect", this);

    addAndMakeVisible(spectrum);
    viewport.setViewedComponent(&panel, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    setResizable(true, true);
    setResizeLimits(700, 460, 3840, 2160);
    setSize(1180, 720);
}

VoidformEditor::~VoidformEditor()
{
    proc.apvts.removeParameterListener("frame", this);
    proc.apvts.removeParameterListener("aspect", this);
    setLookAndFeel(nullptr);
}

void VoidformEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15151a));
    g.setColour(juce::Colour(0xff050506));
    g.fillRect(stageArea);          // backdrop behind the visual / frame (shows as bars when the aspect is locked)
}

void VoidformEditor::setUiHidden(bool hide)
{
    uiHidden = hide;
    for (juce::Component* c : { (juce::Component*) &frameToggle, (juce::Component*) &aspectBox, (juce::Component*) &aspectLabel,
                                (juce::Component*) &crtSlider, (juce::Component*) &crtLabel, (juce::Component*) &presetBox, (juce::Component*) &presetLabel, (juce::Component*) &feelBox, (juce::Component*) &feelLabel, (juce::Component*) &saveButton, (juce::Component*) &hideButton,
                                (juce::Component*) &spectrum, (juce::Component*) &viewport })
        c->setVisible(!hide);
    resized();
}

// The visual (and its frame) keeps its aspect ratio and is centred in the stage.
juce::Rectangle<int> VoidformEditor::fitToStage(juce::Rectangle<int> stage) const
{
    if (stage.isEmpty()) return stage;
    const int mode = aspectMode();
    float ratio = 0.0f;                                  // 0 = fill the stage
    if (frameOn())
        ratio = vf::frameDef(vf::pickFrame(mode, (float) stage.getWidth() / (float) stage.getHeight())).ratio;
    else if (mode == 1) ratio = 16.0f / 9.0f;
    else if (mode == 2) ratio = 9.0f / 16.0f;
    else if (mode == 3) ratio = 1.0f;
    if (ratio <= 0.0f) return stage;

    int w = stage.getWidth(), h = juce::roundToInt((float) w / ratio);
    if (h > stage.getHeight()) { h = stage.getHeight(); w = juce::roundToInt((float) h * ratio); }
    return juce::Rectangle<int>(w, h).withCentre(stage.getCentre());
}

void VoidformEditor::resized()
{
    auto r = getLocalBounds();
    frameToggle.setVisible(!uiHidden);
    if (uiHidden)
    {
        stageArea = r;
        visual.setBounds(fitToStage(r));
        repaint();
        return;
    }

    auto side = r.removeFromRight(panelWidth).reduced(8, 8);
    stageArea = r;
    visual.setBounds(fitToStage(r));

    auto top = side.removeFromTop(28);
    hideButton.setBounds(top.removeFromRight(70));
    top.removeFromRight(6);
    presetLabel.setBounds(top.removeFromLeft(34));
    presetBox.setBounds(top);
    side.removeFromTop(6);
    {
        auto row = side.removeFromTop(26);
        saveButton.setBounds(row.removeFromRight(78));
        row.removeFromRight(6);
        feelLabel.setBounds(row.removeFromLeft(34));
        feelBox.setBounds(row);
        side.removeFromTop(6);
    }
    {
        auto row = side.removeFromTop(26);
        frameToggle.setBounds(row.removeFromLeft(80));
        aspectLabel.setBounds(row.removeFromLeft(50));
        aspectBox.setBounds(row.removeFromLeft(100));
    }
    {
        auto row = side.removeFromTop(26);
        crtLabel.setBounds(row.removeFromLeft(40));
        crtSlider.setBounds(row);
    }
    side.removeFromTop(6);
    spectrum.setBounds(side.removeFromTop(86));
    side.removeFromTop(8);
    viewport.setBounds(side);
    panel.setSize(viewport.getMaximumVisibleWidth(), 100);   // lets the panel lay out
    panel.setSize(viewport.getMaximumVisibleWidth(), panel.getContentHeight());
    repaint();
}

void VoidformEditor::applyValues(const juce::var& values)
{
    if (auto* obj = values.getDynamicObject())
        for (auto& nv : obj->getProperties())
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*>(proc.apvts.getParameter(nv.name.toString())))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost(prm->convertTo0to1((float) nv.value));
                prm->endChangeGesture();
            }
}

void VoidformEditor::applyPreset(int index)
{
    auto* arr = presets.getArray();
    if (arr == nullptr || index < 0 || index >= arr->size()) return;
    applyValues((*arr)[index]["values"]);
}

// "Feel" presets only touch spin / punch / reactivity / band routing, so they layer on top of any look.
void VoidformEditor::applyFeel(int index)
{
    auto* arr = feels.getArray();
    if (arr == nullptr || index < 0 || index >= arr->size()) return;
    applyValues((*arr)[index]["values"]);
}

juce::File VoidformEditor::userLooksFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Preston").getChildFile("Voidform").getChildFile("user_looks.json");
}

void VoidformEditor::loadUserLooks()
{
    userLooks.clear();
    const auto f = userLooksFile();
    if (!f.existsAsFile()) return;
    auto v = juce::JSON::parse(f.loadFileAsString());
    if (auto* arr = v.getArray())
        for (auto& item : *arr) userLooks.add(item);
}

void VoidformEditor::rebuildLookBox(int selectId)
{
    presetBox.clear(juce::dontSendNotification);
    if (auto* arr = presets.getArray())
        for (int i = 0; i < arr->size(); ++i) presetBox.addItem(arr->getReference(i)["name"].toString(), i + 1);
    if (!userLooks.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading("My looks");
        for (int i = 0; i < userLooks.size(); ++i) presetBox.addItem(userLooks.getReference(i)["name"].toString(), 1000 + i);
    }
    if (selectId > 0) presetBox.setSelectedId(selectId, juce::dontSendNotification);
}

void VoidformEditor::saveLookDialog()
{
    auto* aw = new juce::AlertWindow("Save look", "Name for this look:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", "My look");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<VoidformEditor> safe(this);
    aw->enterModalState(true, juce::ModalCallbackFunction::create([safe, aw](int result)
    {
        if (result != 1 || safe == nullptr) return;
        auto name = aw->getTextEditorContents("name").trim();
        if (name.isEmpty()) return;

        auto* vals = new juce::DynamicObject();
        for (auto* prm : safe->proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
            {
                const auto id = rp->paramID;
                if (id == "frame" || id == "aspect" || id == "crt") continue;
                vals->setProperty(id, rp->convertFrom0to1(rp->getValue()));
            }
        auto* entry = new juce::DynamicObject();
        entry->setProperty("name", name);
        entry->setProperty("values", juce::var(vals));

        int replace = -1;
        for (int i = 0; i < safe->userLooks.size(); ++i)
            if (safe->userLooks.getReference(i)["name"].toString() == name) replace = i;
        if (replace >= 0) safe->userLooks.set(replace, juce::var(entry));
        else              safe->userLooks.add(juce::var(entry));

        juce::Array<juce::var> copy(safe->userLooks);
        auto f = userLooksFile();
        f.getParentDirectory().createDirectory();
        f.replaceWithText(juce::JSON::toString(juce::var(copy)));
        safe->rebuildLookBox(1000 + (replace >= 0 ? replace : safe->userLooks.size() - 1));
    }), true);
}
