# File: CMakeLists.txt
# The Drum Master — JUCE plugin, developed with the "away from home" protocol.
# JUCE is pinned via FetchContent so CI, the sandbox and home all build the same version.

cmake_minimum_required(VERSION 3.22)
project(DrumMaster VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

enable_testing()
add_subdirectory(dsp)                      # pure C++ core + tests (no JUCE)

juce_add_plugin(DrumMaster
    COMPANY_NAME             "alchenive12"
    PLUGIN_MANUFACTURER_CODE Alch          # 4 chars, at least one upper-case
    PLUGIN_CODE              Drmr          # 4 chars, exactly one upper-case (first)
    FORMATS                  VST3 AU Standalone   # AU is built on macOS only
    PRODUCT_NAME             "The Drum Master"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE    # MIDI mode (docs/05-features.md F3)
    NEEDS_MIDI_OUTPUT        FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    NEEDS_WEB_BROWSER        FALSE
    NEEDS_CURL               FALSE)

target_sources(DrumMaster
    PRIVATE
        Source/PluginProcessor.cpp)

target_compile_definitions(DrumMaster
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# rendered 3D UI art (assets/ui3d, made by assets3d/scripts/*.py) -> DrumMasterArt.h
juce_add_binary_data(DrumMasterArt
    HEADER_NAME DrumMasterArt.h
    NAMESPACE DrumMasterArt
    SOURCES
        assets/ui3d/stage_bg_lamp100.jpg
        assets/ui3d/stage_bg_lamp050.jpg
        assets/ui3d/stage_bg_lamp015.jpg
        assets/ui3d/knob_filmstrip_64.png
        assets/ui3d/button_off_2x.png
        assets/ui3d/button_hover_2x.png
        assets/ui3d/button_on_2x.png
        assets/ui3d/panel_2x.png)
set_target_properties(DrumMasterArt PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(DrumMaster
    PRIVATE
        DrumMasterArt
        plugin_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
