#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace vf
{
// 4096-point Hann-windowed FFT of the latest mono audio. Called from the render thread only.
class Analyzer
{
public:
    static constexpr int order = 12, N = 1 << order, numBins = N / 2, numDisplay = 96;

    Analyzer() : fft(order), window((size_t) N, juce::dsp::WindowingFunction<float>::hann, false)
    {
        buf.fill(0.0f);
        mag.fill(0.0f);
    }

    void process(const float* samples, double sr)
    {
        sampleRate = sr;
        std::copy(samples, samples + N, buf.begin());
        std::fill(buf.begin() + N, buf.end(), 0.0f);
        window.multiplyWithWindowingTable(buf.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform(buf.data());
        for (int i = 0; i < numBins; ++i) mag[(size_t) i] = buf[(size_t) i] * (4.0f / (float) N);   // ~ sine amplitude
    }

    // 0..1 loudness mapping: -60..-6 dBFS, +3 dB/oct tilt above 1 kHz so highs are comparable to bass.
    static float norm(float amp, float freq)
    {
        float db = 20.0f * std::log10(std::max(amp, 1.0e-6f)) + 3.0f * std::log2(std::max(freq, 20.0f) / 1000.0f);
        float x = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 54.0f);
        return std::pow(x, 1.6f);
    }

    float binHz() const { return (float) sampleRate / (float) N; }

    // Peak level inside [freq / 2^(w/2), freq * 2^(w/2)]
    float bandLevel(float freq, float widthOct) const
    {
        const float half = std::pow(2.0f, widthOct * 0.5f);
        return rangeLevel(freq / half, freq * half);
    }

    float rangeLevel(float lo, float hi) const
    {
        const float bh = binHz();
        int b0 = juce::jlimit(1, numBins - 1, (int) std::floor(lo / bh));
        int b1 = juce::jlimit(b0, numBins - 1, (int) std::ceil(hi / bh));
        float peak = 0.0f;
        for (int b = b0; b <= b1; ++b) peak = std::max(peak, mag[(size_t) b]);
        return norm(peak, std::sqrt(lo * hi));
    }

    float displayLevel(int i) const
    {
        const float f0 = 20.0f * std::pow(1000.0f, (float) i / numDisplay);
        const float f1 = 20.0f * std::pow(1000.0f, (float) (i + 1) / numDisplay);
        return rangeLevel(f0, f1);
    }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, 2 * N> buf;
    std::array<float, numBins> mag;
    double sampleRate = 44100.0;
};
} // namespace vf
