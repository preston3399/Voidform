#include "PluginProcessor.h"
#include "PluginEditor.h"

VoidformProcessor::VoidformProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", vf::createLayout())
{
    for (auto& s : spectrum) s.store(0.0f);
    for (auto& b : bandEnv) b.store(0.0f);
}

bool VoidformProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void VoidformProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
    if (ch == 0 || n == 0) return;

    // Audio passes through untouched; we just tap a mono copy for the analyser.
    const uint64_t pos = writePos.load(std::memory_order_relaxed);
    const float inv = 1.0f / (float) ch;
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < ch; ++c) s += buffer.getReadPointer(c)[i];
        ring[(size_t) ((pos + (uint64_t) i) & (ringSize - 1))] = s * inv;
    }
    writePos.store(pos + (uint64_t) n, std::memory_order_release);
}

void VoidformProcessor::copyLatest(float* dst, int n) const
{
    const uint64_t pos = writePos.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        dst[i] = ring[(size_t) ((pos - (uint64_t) n + (uint64_t) i) & (ringSize - 1))];
}

void VoidformProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void VoidformProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* VoidformProcessor::createEditor() { return new VoidformEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoidformProcessor(); }
