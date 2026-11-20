#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vf
{
enum Target { tSize, tWarp, tSpin, tFilaments, tBloom, tGlitch, tBrightness, tColor, numTargets };

inline juce::StringArray targetNames()
{
    return { "Size (kick)", "Warp / blobbiness", "Spin speed", "Filaments", "Bloom", "Glitch", "Brightness", "Color shift" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using juce::NormalisableRange;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto F = [&](const char* id, const char* name, float lo, float hi, float def, float skew = 1.0f, float step = 0.0f)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id, 1 }, name,
                                                               NormalisableRange<float>(lo, hi, step, skew), def));
    };
    auto B = [&](const char* id, const char* name, bool def)
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, 1 }, name, def));
    };

    // ---- colour ----
    F("hueA", "Color A hue", 0, 1, 0.72f);       F("satA", "Color A saturation", 0, 1, 0.22f);
    F("hueB", "Color B hue", 0, 1, 0.28f);       F("satB", "Color B saturation", 0, 1, 0.35f);
    F("colorMix", "Color A/B mix", 0, 1, 0.6f);
    F("brightness", "Brightness", 0, 4, 0.3f);  F("bg", "Background / paper", 0, 1, 0.0f);
    B("frame", "Retro monitor frame", true);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "aspect", 1 }, "Aspect",
                                                             juce::StringArray { "Free", "16:9", "9:16", "1:1" }, 1));
    B("ink", "Ink-on-paper mode", false);        F("chroma", "Chromatic fringe", 0, 1.5f, 0.8f);

    // ---- material / post ----
    F("metallic", "Metallic", 0, 1, 0.0f);       F("reflect", "Reflectivity", 0, 2, 0.2f);
    F("crt", "CRT effect", 0, 1, 0.6f);
    F("grain", "Grain", 0, 1.5f, 0.7f);          F("bloom", "Bloom", 0, 3, 0.9f);
    F("glitch", "Glitch", 0, 1, 0.05f);          F("vignette", "Vignette", 0, 1, 0.35f);

    // ---- particles ----
    F("count", "Particle count", 10000, 1500000, 400000, 0.45f, 1000);
    F("size", "Particle size", 0.5f, 8, 1.5f, 0.6f);
    F("fuzz", "Fuzz / thickness", 0, 1, 0.08f);  F("filaments", "Filaments", 0, 1, 0.5f);
    F("filLen", "Filament length", 0, 2, 0.45f); F("wind", "Filament wind (up/down)", -1, 1, 0.0f);
    F("mesh", "Wire mesh", 0, 1, 0.0f);          F("trails", "Trails", 0, 0.98f, 0.55f);

    // ---- shape / motion ----
    F("zoom", "Zoom", 0.3f, 2.5f, 1.0f);   F("fit", "Auto-fit to window", 0, 1, 1.0f);        F("warp", "Warp", 0, 2, 0.45f);
    F("detail", "Detail (noise scale)", 0.2f, 4, 1.6f);
    F("speed", "Flow speed", 0, 3, 0.9f);       F("spin", "Spin", -2, 2, 0.3f);
    F("tilt", "Tilt", -1.5f, 1.5f, 0.1f);

    // ---- audio ----
    F("sens", "Sensitivity", 0, 4, 1.0f);
    F("react", "Reactivity", 0, 4, 1.6f);
    F("punch", "Punch (beat explosion)", 0, 3, 1.5f);
    F("shake", "Camera shake", 0, 1, 0.5f);
    F("attack", "Attack (ms)", 1, 200, 8, 0.5f); F("release", "Release (ms)", 20, 1500, 250, 0.5f);

    const char* bandDefaultTarget[4] = { "Size (kick)", "Warp / blobbiness", "Brightness", "Filaments" };
    const int   defTarget[4] = { tSize, tWarp, tBrightness, tFilaments };
    const float defFreq[4]   = { 60, 220, 1800, 9000 };
    const float defWidth[4]  = { 1.2f, 2.0f, 2.5f, 2.0f };
    const float defGain[4]   = { 1.0f, 0.9f, 0.6f, 0.8f };
    juce::ignoreUnused(bandDefaultTarget);

    for (int i = 0; i < 4; ++i)
    {
        auto n = juce::String(i + 1);
        NormalisableRange<float> fr(20.0f, 20000.0f, 1.0f);
        fr.setSkewForCentre(1000.0f);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "b" + n + "f", 1 }, "Band " + n + " frequency", fr, defFreq[i]));
        F(("b" + n + "w").toRawUTF8(), ("Band " + n + " width (oct)").toRawUTF8(), 0.2f, 5, defWidth[i]);
        F(("b" + n + "g").toRawUTF8(), ("Band " + n + " gain").toRawUTF8(), 0, 4, defGain[i]);
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "b" + n + "t", 1 }, "Band " + n + " target",
                                                                 targetNames(), defTarget[i]));
    }
    return layout;
}
} // namespace vf
