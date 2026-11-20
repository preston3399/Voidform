#pragma once
#include <juce_opengl/juce_opengl.h>
#include <unordered_map>
#include "PluginProcessor.h"
#include "Analyzer.h"

// Draws the particle visual with OpenGL 3.2 core. Everything GL-side runs on JUCE's render thread.
class Renderer : public juce::OpenGLRenderer
{
public:
    Renderer(VoidformProcessor&, juce::OpenGLContext&, juce::Component& target);

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    struct Target
    {
        unsigned tex = 0, fbo = 0;
        int w = 0, h = 0;
        void create(int width, int height);
        void destroy();
    };

    struct Eff   // parameter values after audio modulation
    {
        float zoom, warp, spinAdd, filaments, filLen, bloom, glitch, bright, cshift, chroma;
    };

    float P(const char* id) const;
    void updateAudio(double dt);
    void ensureTargets(int W, int H);
    std::unique_ptr<juce::OpenGLShaderProgram> makeProgram(const char* vs, int vsSize, const char* fs, int fsSize);
    void drawFullscreen();

    VoidformProcessor& proc;
    juce::OpenGLContext& context;
    juce::Component& targetComp;

    std::unique_ptr<juce::OpenGLShaderProgram> pFade, pDown, pUp, pPart, pComp;
    Target accum[2];
    Target mips[5];
    int cur = 0, W = 0, H = 0;
    unsigned vao = 0;
    unsigned frameTex[3] = { 0, 0, 0 };
    void ensureFrameTexture(int index);
    int fullW = 0, fullH = 0;

    vf::Analyzer analyzer;
    std::vector<float> sampleScratch;
    std::unordered_map<std::string, std::atomic<float>*> params;
    float env[4] {};
    float slowEnv = 0, punch = 0, waveT = 2.0f, fitS = 1.0f;
    bool trig = false;
    float mods[vf::numTargets] {};

    double lastMs = 0.0, time = 0.0, flow = 0.0, rotY = 0.0;
    bool ready = false;
};
