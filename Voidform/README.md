# Voidform - audio-reactive particle visualizer (VST3 for FL Studio)

GPU particle cloud (OpenGL 3.2) with ink / glow / metal looks, driven by 4 frequency bands plus a beat "punch".

## Build on Windows
Needs: Visual Studio 2022 (workload "Desktop development with C++"), CMake 3.22+, Git.

    cd Voidform
    cmake -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH=C:/path/to/your/JUCE
    cmake --build build --config Release

Leave out -DJUCE_PATH and CMake downloads JUCE 9.0.6 itself (first build only).
Output: build/Voidform_artefacts/Release/VST3/Voidform.vst3 (and a Standalone .exe for quick testing).
Copy the Voidform.vst3 folder to C:\Program Files\Common Files\VST3 (needs admin).

## No Visual Studio? Build in the cloud
Push this folder to a GitHub repo. The included workflow (.github/workflows/build.yml) builds Windows and macOS;
download the Voidform-windows-latest artifact from the Actions tab (or run it by hand with Run workflow).

## Use in FL Studio
1. Options > Manage plugins > Find installed plugins (rescan).
2. Put Voidform in an effect slot on the mixer track you want to visualize. Audio passes through unchanged.
3. Pick a look, then tweak. Double-click the visual to hide/show the controls.

## Frame / aspect
Frame toggles your monitor frame; Aspect is Free / 16:9 / 9:16 / 1:1. Frame art lives in Assets/ (PNG with a
transparent screen opening). If you replace a frame, update its opening rectangle in Source/FrameInfo.h.

tools/render_preview.py renders the same shaders headlessly (python + moderngl) for look-dev.
