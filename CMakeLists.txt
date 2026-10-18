cmake_minimum_required(VERSION 3.22)

# must be set BEFORE project(), otherwise it is ignored
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(HipHopBeatMaker VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: either a local checkout (-DJUCE_SOURCE_DIR=...) or fetched from GitHub
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional local JUCE checkout")
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

endif()

set(HHBM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HHBM_FORMATS AU)
elseif(UNIX)
    list(APPEND HHBM_FORMATS LV2)
endif()

juce_add_plugin(HipHopBeatMaker
    COMPANY_NAME "JustCreateit"
    COMPANY_WEBSITE "https://justcreateit.ch"
    BUNDLE_ID "ch.justcreateit.hiphopbeatmaker"
    PRODUCT_NAME "Hip-Hop BeatMaker"
    PLUGIN_MANUFACTURER_CODE Jcit
    PLUGIN_CODE HhBm
    FORMATS ${HHBM_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    LV2URI "https://justcreateit.ch/plugins/hiphop-beatmaker"
    VST3_CATEGORIES Instrument Drum Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(HipHopBeatMaker PRIVATE
    Source/Generator.cpp
    Source/SynthEngine.cpp
    Source/MidiExport.cpp
    Source/SampleTools.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/NoteEditor.cpp)

target_compile_definitions(HipHopBeatMaker PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HipHopBeatMaker
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- optional render test
option(HHBM_BUILD_TESTS "Build the offline render test" OFF)
if(HHBM_BUILD_TESTS)
    juce_add_console_app(HHBMRenderTest PRODUCT_NAME "HHBMRenderTest")
    target_sources(HHBMRenderTest PRIVATE
        Tests/RenderTest.cpp
        Source/Generator.cpp
        Source/SynthEngine.cpp
        Source/MidiExport.cpp
        Source/SampleTools.cpp)
    target_include_directories(HHBMRenderTest PRIVATE Source)
    target_compile_definitions(HHBMRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(HHBMRenderTest PRIVATE juce::juce_audio_basics juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags)
    juce_add_console_app(HHBMProcessorTest PRODUCT_NAME "HHBMProcessorTest")
    target_sources(HHBMProcessorTest PRIVATE
        Tests/ProcessorTest.cpp
        Source/Generator.cpp
        Source/SynthEngine.cpp
        Source/MidiExport.cpp
        Source/SampleTools.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    Source/NoteEditor.cpp)
    target_include_directories(HHBMProcessorTest PRIVATE Source)
    target_compile_definitions(HHBMProcessorTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="HHBM")
    target_link_libraries(HHBMProcessorTest PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)

endif()
