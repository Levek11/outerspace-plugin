cmake_minimum_required(VERSION 3.22)

project(OuterSpace VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    # Universal binary (Apple silicon + Intel) unless the caller chose otherwise.
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()
endif()

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if given (-DOUTERSPACE_JUCE_DIR=...), otherwise fetch.
set(OUTERSPACE_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(OUTERSPACE_JUCE_DIR)
    add_subdirectory(${OUTERSPACE_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
if(APPLE)
    set(OUTERSPACE_FORMATS AU VST3)
else()
    set(OUTERSPACE_FORMATS VST3)   # Linux/Windows builds are for compile checks only
endif()

juce_add_plugin(OuterSpace
    PRODUCT_NAME "OuterSpace"
    COMPANY_NAME "Levek11"
    BUNDLE_ID "com.levek11.outerspace"
    PLUGIN_MANUFACTURER_CODE Levk
    PLUGIN_CODE OSpc
    FORMATS ${OUTERSPACE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx EQ Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(OuterSpaceAssets
    HEADER_NAME OuterSpaceAssets.h
    NAMESPACE OuterSpaceAssets
    SOURCES assets/fonts/Michroma-Regular.ttf assets/fonts/Goldman-Bold.ttf)

set(OUTERSPACE_DSP_SOURCES
    Source/dsp/Spacer.cpp
    Source/link/SourceRegistry.cpp)

target_sources(OuterSpace PRIVATE
    ${OUTERSPACE_DSP_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/OuterLookAndFeel.cpp
    Source/ui/SpectrumDisplay.cpp)

target_include_directories(OuterSpace PRIVATE Source)

target_compile_definitions(OuterSpace PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(OuterSpace
    PRIVATE
        OuterSpaceAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Headless DSP tests (run in CI on every build).
juce_add_console_app(OuterSpaceDspTest PRODUCT_NAME "OuterSpaceDspTest")
target_sources(OuterSpaceDspTest PRIVATE Tests/DspTest.cpp ${OUTERSPACE_DSP_SOURCES})
target_include_directories(OuterSpaceDspTest PRIVATE Source Tests)
target_compile_definitions(OuterSpaceDspTest PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(OuterSpaceDspTest
    PRIVATE juce::juce_audio_formats juce::juce_events
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Optional: render the editor to PNGs (cmake -DOUTERSPACE_SNAPSHOT=ON).
option(OUTERSPACE_SNAPSHOT "Build the UI snapshot tool" OFF)
if(OUTERSPACE_SNAPSHOT)
    juce_add_console_app(OuterSpaceSnapshot PRODUCT_NAME "OuterSpaceSnapshot")
    target_sources(OuterSpaceSnapshot PRIVATE Tests/Snapshot.cpp ${OUTERSPACE_DSP_SOURCES}
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/ui/OuterLookAndFeel.cpp Source/ui/SpectrumDisplay.cpp)
    target_include_directories(OuterSpaceSnapshot PRIVATE Source)
    target_compile_definitions(OuterSpaceSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1 JucePlugin_Name="OuterSpace")
    target_link_libraries(OuterSpaceSnapshot PRIVATE OuterSpaceAssets juce::juce_audio_utils)
endif()
