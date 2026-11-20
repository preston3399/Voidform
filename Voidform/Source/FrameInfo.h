// Generated from the frame PNGs: screen opening as fractions of the image (x0, y0, x1, y1), y measured from the TOP.
#pragma once
namespace vf
{
struct FrameDef { const char* name; float ratio; float ox0, oy0, ox1, oy1; };
inline const FrameDef& frameDef(int i)
{
    static const FrameDef defs[3] = {
        { "16:9", 1.777778f, 0.040625f, 0.068519f, 0.905208f, 0.933333f },
        { "9:16", 0.562500f, 0.066667f, 0.040625f, 0.931481f, 0.905208f },
        { "1:1", 1.000000f, 0.065625f, 0.076042f, 0.932813f, 0.828125f },
    };
    return defs[i < 0 ? 0 : (i > 2 ? 2 : i)];
}

// aspect: 0 = Free, 1 = 16:9, 2 = 9:16, 3 = 1:1.  Returns the frame index 0..2 (16:9, 9:16, 1:1).
inline int pickFrame(int aspect, float stageRatio)
{
    if (aspect == 1) return 0;
    if (aspect == 2) return 1;
    if (aspect == 3) return 2;
    return stageRatio >= 1.4f ? 0 : (stageRatio <= 0.75f ? 1 : 2);
}
} // namespace vf
