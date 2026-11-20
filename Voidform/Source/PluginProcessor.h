#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Params.h"
#include "Analyzer.h"

class VoidformProcessor : public juce::AudioProcessor
{
public:
    VoidformProcessor();
    ~VoidformProcessor() override = default;

    void prepareToPlay(double sr, int) override { sampleRate.store(sr); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Latest n mono samples (audio thread writes, render thread reads).
    void copyLatest(float* dst, int n) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<double> sampleRate { 44100.0 };

    // Published by the render thread for the editor's spectrum view.
    std::array<std::atomic<float>, vf::Analyzer::numDisplay> spectrum;
    std::array<std::atomic<float>, 4> bandEnv;

private:
    static constexpr size_t ringSize = 32768;
    std::array<float, ringSize> ring {};
    std::atomic<uint64_t> writePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoidformProcessor)
};
