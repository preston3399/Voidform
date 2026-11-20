#include "Renderer.h"
#include <BinaryData.h>
#include "FrameInfo.h"

using namespace juce::gl;

namespace
{
inline void U1(juce::OpenGLShaderProgram& p, const char* n, float v) { auto l = glGetUniformLocation(p.getProgramID(), n); if (l >= 0) glUniform1f(l, v); }
inline void U1i(juce::OpenGLShaderProgram& p, const char* n, int v)  { auto l = glGetUniformLocation(p.getProgramID(), n); if (l >= 0) glUniform1i(l, v); }
inline void U2(juce::OpenGLShaderProgram& p, const char* n, float a, float b) { auto l = glGetUniformLocation(p.getProgramID(), n); if (l >= 0) glUniform2f(l, a, b); }
inline void U3(juce::OpenGLShaderProgram& p, const char* n, float a, float b, float c) { auto l = glGetUniformLocation(p.getProgramID(), n); if (l >= 0) glUniform3f(l, a, b, c); }
inline juce::String str(const char* d, int s) { return juce::String::fromUTF8(d, s); }
inline void rgb(float h, float s, float& r, float& g, float& b)
{
    auto c = juce::Colour::fromHSV(h - std::floor(h), s, 1.0f, 1.0f);
    r = c.getFloatRed(); g = c.getFloatGreen(); b = c.getFloatBlue();
}
}

// ---------------------------------------------------------------------------------------------
void Renderer::Target::create(int width, int height)
{
    destroy();
    w = width; h = height;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint) GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::Target::destroy()
{
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (tex) glDeleteTextures(1, &tex);
    fbo = tex = 0; w = h = 0;
}

// ---------------------------------------------------------------------------------------------
Renderer::Renderer(VoidformProcessor& p, juce::OpenGLContext& c, juce::Component& t)
    : proc(p), context(c), targetComp(t), sampleScratch((size_t) vf::Analyzer::N, 0.0f)
{
    for (auto* prm : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
            params[rp->paramID.toStdString()] = proc.apvts.getRawParameterValue(rp->paramID);
}

float Renderer::P(const char* id) const
{
    auto it = params.find(id);
    return it == params.end() ? 0.0f : it->second->load();
}

std::unique_ptr<juce::OpenGLShaderProgram> Renderer::makeProgram(const char* vs, int vsSize, const char* fs, int fsSize)
{
    auto prog = std::make_unique<juce::OpenGLShaderProgram>(context);
    // addShader (not addVertexShader) so our own "#version 150" header is used verbatim.
    if (prog->addShader(str(vs, vsSize), GL_VERTEX_SHADER) && prog->addShader(str(fs, fsSize), GL_FRAGMENT_SHADER) && prog->link())
        return prog;
    DBG("Voidform shader error: " << prog->getLastError());
    jassertfalse;
    return nullptr;
}

void Renderer::newOpenGLContextCreated()
{
    using namespace BinaryData;
    pFade = makeProgram(fullscreen_vert, fullscreen_vertSize, fade_frag, fade_fragSize);
    pDown = makeProgram(fullscreen_vert, fullscreen_vertSize, blur_down_frag, blur_down_fragSize);
    pUp   = makeProgram(fullscreen_vert, fullscreen_vertSize, blur_up_frag, blur_up_fragSize);
    pComp = makeProgram(fullscreen_vert, fullscreen_vertSize, composite_frag, composite_fragSize);
    pPart = makeProgram(particles_vert, particles_vertSize, particles_frag, particles_fragSize);
    glGenVertexArrays(1, &vao);   // core profile needs a VAO bound even for attribute-less draws
    ready = pFade && pDown && pUp && pComp && pPart;
    lastMs = juce::Time::getMillisecondCounterHiRes();
    W = H = 0;
}

void Renderer::openGLContextClosing()
{
    ready = false;
    for (auto& a : accum) a.destroy();
    for (auto& m : mips) m.destroy();
    for (auto& t : frameTex) if (t) { glDeleteTextures(1, &t); t = 0; }
    pFade.reset(); pDown.reset(); pUp.reset(); pPart.reset(); pComp.reset();
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
}

void Renderer::ensureTargets(int w, int h)
{
    if (w == W && h == H && accum[0].tex) return;
    W = w; H = h;
    accum[0].create(w, h);
    accum[1].create(w, h);
    int mw = w, mh = h;
    for (auto& m : mips) { mw = std::max(1, mw / 2); mh = std::max(1, mh / 2); m.create(mw, mh); }
}

void Renderer::ensureFrameTexture(int i)
{
    if (frameTex[i]) return;
    struct Src { const char* data; int size; };
    const Src src[3] = { { BinaryData::frame169_png, BinaryData::frame169_pngSize },
                         { BinaryData::frame916_png, BinaryData::frame916_pngSize },
                         { BinaryData::frame11_png,  BinaryData::frame11_pngSize } };
    auto img = juce::ImageFileFormat::loadFrom(src[i].data, (size_t) src[i].size).convertedToFormat(juce::Image::ARGB);
    if (img.isNull()) return;
    const int w = img.getWidth(), h = img.getHeight();
    std::vector<uint8_t> px((size_t) w * (size_t) h * 4);
    {
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                auto* p = (const juce::PixelARGB*) bd.getPixelPointer(x, y);   // stored premultiplied
                uint8_t* o = &px[((size_t) y * (size_t) w + (size_t) x) * 4];
                o[0] = p->getRed(); o[1] = p->getGreen(); o[2] = p->getBlue(); o[3] = p->getAlpha();
            }
    }
    glGenTextures(1, &frameTex[i]);
    glBindTexture(GL_TEXTURE_2D, frameTex[i]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint) GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);
}

void Renderer::drawFullscreen() { glDrawArrays(GL_TRIANGLES, 0, 3); }

// ---------------------------------------------------------------------------------------------
void Renderer::updateAudio(double dt)
{
    proc.copyLatest(sampleScratch.data(), vf::Analyzer::N);
    analyzer.process(sampleScratch.data(), proc.sampleRate.load());

    const float sens = P("sens");
    const float aA = 1.0f - std::exp(-(float) dt * 1000.0f / std::max(1.0f, P("attack")));
    const float aR = 1.0f - std::exp(-(float) dt * 1000.0f / std::max(1.0f, P("release")));

    std::fill(std::begin(mods), std::end(mods), 0.0f);
    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String(i + 1).toStdString();
        const float level = analyzer.bandLevel(P(("b" + n + "f").c_str()), P(("b" + n + "w").c_str()))
                            * P(("b" + n + "g").c_str()) * sens;
        env[i] += (level > env[i] ? aA : aR) * (level - env[i]);
        const int target = juce::jlimit(0, (int) vf::numTargets - 1, (int) std::lround(P(("b" + n + "t").c_str())));
        mods[target] += env[i];
        proc.bandEnv[(size_t) i].store(env[i]);
    }
    const float react = P("react");
    for (auto& m : mods) m = std::min(m * react, 3.0f);

    // ---- transient detector: fast bass envelope minus a slower follower = "hit" ----
    const float fast = std::max(env[0], env[1] * 0.8f);
    slowEnv += (1.0f - std::exp(-(float) dt / 0.12f)) * (fast - slowEnv);
    const float onset = juce::jlimit(0.0f, 1.5f, (fast - slowEnv) * 5.0f);
    punch = std::min(1.3f, std::max(punch * std::exp(-(float) dt / 0.22f), onset * P("punch")));
    if (onset > 0.3f && !trig) { trig = true; waveT = 0.0f; }
    if (onset < 0.1f) trig = false;
    waveT += (float) dt * 2.2f;
    for (int i = 0; i < vf::Analyzer::numDisplay; ++i) proc.spectrum[(size_t) i].store(analyzer.displayLevel(i));
}

// ---------------------------------------------------------------------------------------------
void Renderer::renderOpenGL()
{
    if (!ready) return;

    const double scale = context.getRenderingScale();
    fullW = juce::roundToInt(scale * targetComp.getWidth());
    fullH = juce::roundToInt(scale * targetComp.getHeight());
    if (fullW < 16 || fullH < 16) return;

    // The visual is rendered into the frame's screen opening (or the whole component when the frame is off).
    const bool framed = P("frame") > 0.5f;
    float open[4] = { 0.0f, 0.0f, 1.0f, 1.0f };   // x0, y0, x1, y1 (y up)
    int frameIndex = -1;
    if (framed)
    {
        frameIndex = vf::pickFrame((int) std::lround(P("aspect")), (float) fullW / (float) fullH);
        const auto& d = vf::frameDef(frameIndex);
        open[0] = d.ox0; open[2] = d.ox1; open[1] = 1.0f - d.oy1; open[3] = 1.0f - d.oy0;
        ensureFrameTexture(frameIndex);
    }
    const int w = std::max(16, juce::roundToInt((float) fullW * (open[2] - open[0])));
    const int h = std::max(16, juce::roundToInt((float) fullH * (open[3] - open[1])));

    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit(0.0, 0.1, (now - lastMs) * 0.001);
    lastMs = now;
    time = std::fmod(time + dt, 3600.0);

    ensureTargets(w, h);
    updateAudio(dt);

    // ---- modulated parameters (kept identical to tools/render_preview.py) ----
    const bool ink = P("ink") > 0.5f;
    Eff e;
    e.zoom      = P("zoom") * (1.0f + 0.50f * mods[vf::tSize] + 0.12f * punch);
    e.warp      = P("warp") + 1.1f * mods[vf::tWarp] + 0.25f * punch;
    e.spinAdd   = mods[vf::tSpin] * 4.0f + 1.5f * punch * P("spinKick");
    e.filaments = std::min(1.0f, P("filaments") + 0.5f * mods[vf::tFilaments] + 0.1f * punch);
    e.filLen    = P("filLen") * (1.0f + 1.2f * mods[vf::tFilaments]);
    e.bloom     = P("bloom") * (1.0f + 1.5f * mods[vf::tBloom] + 0.5f * punch) + 0.9f * mods[vf::tBloom];
    e.glitch    = std::min(1.5f, P("glitch") + mods[vf::tGlitch] + 0.35f * punch);
    e.bright    = P("brightness") * (1.0f + 0.7f * mods[vf::tBrightness] + 0.25f * punch);
    e.cshift    = 0.5f * mods[vf::tColor];
    e.chroma    = P("chroma") + 0.6f * punch;

    // ---- auto-fit: estimate how far the cloud reaches this frame and shrink it to stay inside the window ----
    {
        const float ext = vf::shapeExtent((int) std::lround(P("shape"))) + std::max(0.0f, e.warp) * 0.75f + punch * 1.5f + P("fuzz") * 0.6f
                          + (e.filaments > 0.01f ? e.filLen * (0.8f + 0.8f * punch) * std::min(1.0f, e.filaments * 3.0f) : 0.0f);
        float target = juce::jlimit(0.25f, 1.0f, 1.55f / ext) * std::min(1.0f, (float) W / (float) H);
        target = juce::jmap(P("fit"), 1.0f, target);
        if (target < fitS) fitS = target;                                            // shrink instantly
        else fitS += (target - fitS) * (1.0f - std::exp(-(float) dt / 0.25f));       // grow back smoothly
        e.zoom *= fitS;
    }

    flow += dt * P("speed") * (1.0 + 0.8 * mods[vf::tWarp]);
    rotY += dt * (P("spin") + e.spinAdd);
    const float shake = P("shake") * punch;
    const float rotX = P("tilt") + 0.12f * std::sin((float) time * 0.23f) + shake * 0.14f * std::sin((float) time * 53.0f);
    const float rotYj = (float) rotY + shake * 0.10f * std::sin((float) time * 41.0f + 1.3f);

    // ---- GL state ----
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);

    // ---- 1. trails: previous frame * decay into the other accumulation buffer ----
    const int prev = cur, next = cur ^ 1;
    glBindFramebuffer(GL_FRAMEBUFFER, accum[next].fbo);
    glViewport(0, 0, W, H);
    glDisable(GL_BLEND);
    const float trails = P("trails");
    if (trails > 0.001f)
    {
        pFade->use();
        glBindTexture(GL_TEXTURE_2D, accum[prev].tex);
        U1i(*pFade, "uPrev", 0);
        U1(*pFade, "uDecay", trails);
        drawFullscreen();
    }
    else
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    cur = next;

    // ---- 2. particles (additive) ----
    const int count = (int) P("count");
    const float size = P("size");
    const float overlap = (float) count * (juce::MathConstants<float>::pi * 0.25f * size * size * 0.8f)
                          * (float) ((float) H / 1080.0f) * ((float) H / 1080.0f)
                          / (juce::MathConstants<float>::pi * std::pow(0.21f * e.zoom * (float) H, 2.0f));
    const float perParticle = e.bright * 0.55f * std::pow(std::max(overlap, 0.05f), -0.55f) * (ink ? 0.9f : 1.0f);

    float ar, ag, ab, br, bg, bb;
    rgb(P("hueA"), P("satA"), ar, ag, ab);
    rgb(P("hueB"), P("satB"), br, bg, bb);

    pPart->use();
    auto& pp = *pPart;
    U2(pp, "uRes", (float) W, (float) H);
    U1(pp, "uFlow", (float) flow);
    U1(pp, "uRotY", rotYj);
    U1(pp, "uPunch", punch);
    U1(pp, "uWave", waveT);
    U1(pp, "uRotX", rotX);
    U1(pp, "uZoom", e.zoom);
    U1(pp, "uSize", size);
    U1(pp, "uWarp", e.warp);
    U1(pp, "uDetail", P("detail"));
    U1(pp, "uMesh", P("mesh"));
    U1(pp, "uFuzz", P("fuzz"));
    U1(pp, "uFilLen", e.filLen);
    U1(pp, "uWind", P("wind"));
    U1(pp, "uMetal", P("metallic"));
    U1(pp, "uReflect", P("reflect"));
    U1(pp, "uPerParticle", perParticle);
    U1(pp, "uColorMix", P("colorMix"));
    U1(pp, "uColorShift", e.cshift);
    U1(pp, "uInk", ink ? 1.0f : 0.0f);
    U1i(pp, "uShape", (int) std::lround(P("shape")));
    U1i(pp, "uStyle", (int) std::lround(P("pstyle")));
    U3(pp, "uColA", ar, ag, ab);
    U3(pp, "uColB", br, bg, bb);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    U1i(pp, "uMode", 0);
    glDrawArrays(GL_POINTS, 0, count);

    const int nFil = (int) (80000.0f * e.filaments * std::min(1.0f, (float) count / 300000.0f + 0.3f));
    if (nFil > 0)
    {
        U1i(pp, "uMode", 1);
        U1(pp, "uPerParticle", perParticle * 0.8f);
        glDrawArrays(GL_LINES, 0, nFil * 2);
    }

    // ---- 3. bloom: dual-Kawase down, additive up ----
    glDisable(GL_BLEND);
    unsigned srcTex = accum[cur].tex;
    pDown->use();
    U1i(*pDown, "uTex", 0);
    for (auto& m : mips)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m.fbo);
        glViewport(0, 0, m.w, m.h);
        glBindTexture(GL_TEXTURE_2D, srcTex);
        U2(*pDown, "uHalfPixel", 0.5f / (float) m.w, 0.5f / (float) m.h);
        drawFullscreen();
        srcTex = m.tex;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    pUp->use();
    U1i(*pUp, "uTex", 0);
    U1(*pUp, "uWeight", 0.85f);
    for (int i = 4; i > 0; --i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mips[i - 1].fbo);
        glViewport(0, 0, mips[i - 1].w, mips[i - 1].h);
        glBindTexture(GL_TEXTURE_2D, mips[i].tex);
        U2(*pUp, "uHalfPixel", 0.5f / (float) mips[i].w, 0.5f / (float) mips[i].h);
        drawFullscreen();
    }
    glDisable(GL_BLEND);

    // ---- 4. composite to the window ----
    glBindFramebuffer(GL_FRAMEBUFFER, context.getFrameBufferID());
    glViewport(0, 0, fullW, fullH);
    pComp->use();
    auto& pc = *pComp;
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, accum[cur].tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, mips[0].tex);
    U1i(pc, "uScene", 0);
    U1i(pc, "uBloom", 1);
    U1i(pc, "uFrame", 2);
    U1(pc, "uFrameOn", (framed && frameTex[std::max(frameIndex, 0)]) ? 1.0f : 0.0f);
    {
        auto l = glGetUniformLocation(pc.getProgramID(), "uOpen");
        if (l >= 0) glUniform4f(l, open[0], open[1], open[2], open[3]);
    }
    if (framed && frameIndex >= 0) { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, frameTex[frameIndex]); }
    U2(pc, "uRes", (float) W, (float) H);
    U1(pc, "uTime", (float) time);
    U1(pc, "uBloomAmt", e.bloom * 0.35f);
    U1(pc, "uGrain", P("grain"));
    U1(pc, "uChroma", e.chroma);
    U1(pc, "uGlitch", e.glitch);
    U1(pc, "uVignette", P("vignette"));
    U1(pc, "uExposure", 1.0f);
    U1(pc, "uInk", ink ? 1.0f : 0.0f);
    U1(pc, "uCrt", P("crt"));
    U1(pc, "uBg", P("bg"));
    {   // ink absorbs the complement of its hue: saturation 0 = black ink, otherwise coloured ink
        float hr, hg, hb;
        rgb(P("hueA"), 1.0f, hr, hg, hb);
        const float s = P("satA");
        U3(pc, "uAbsorb", (1 - s) + s * (1 - hr), (1 - s) + s * (1 - hg), (1 - s) + s * (1 - hb));
    }
    drawFullscreen();
    glActiveTexture(GL_TEXTURE0);
}
