cmake_minimum_required(VERSION 3.22)
project(Voidform VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a JUCE checkout you already have:   cmake -B build -DJUCE_PATH=C:/dev/JUCE
# Otherwise JUCE is downloaded automatically.
set(JUCE_PATH "" CACHE PATH "Path to an existing JUCE checkout (optional)")
if (JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                              GIT_TAG 9.0.6 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Shaders + presets are embedded in the plugin binary (BinaryData::xxx_frag etc.)
juce_add_binary_data(VoidformData SOURCES
    Shaders/fullscreen.vert Shaders/fade.frag Shaders/blur_down.frag Shaders/blur_up.frag
    Shaders/particles.vert  Shaders/particles.frag Shaders/composite.frag
    Presets/presets.json
    Assets/frame169.png Assets/frame916.png Assets/frame11.png)

juce_add_plugin(Voidform
    COMPANY_NAME "Preston"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Prst
    PLUGIN_CODE Vfrm
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Analyzer
    PRODUCT_NAME "Voidform")

target_sources(Voidform PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Renderer.cpp)

target_compile_definitions(Voidform PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_OPENGL3=1)

target_link_libraries(Voidform
    PRIVATE VoidformData juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
