cmake_minimum_required(VERSION 3.22)

project(RaveForge VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Point -DJUCE_DIR=/path/to/JUCE at an existing checkout to skip the download.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

if (JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else ()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif ()

option(RAVEFORGE_BUILD_TESTS "Build the offline render test" ON)

set(PluginSources
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/RaveLookAndFeel.h
    Source/DSP/Engine.h
    Source/DSP/RaveStack.h
    Source/DSP/Dirt.h
    Source/DSP/Modulation.h
    Source/DSP/Delay.h
    Source/DSP/KeysSynth.h)

juce_add_plugin(RaveForge
    COMPANY_NAME "Kebbell"
    BUNDLE_ID com.kebbell.raveforge
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Kbbl
    PLUGIN_CODE RvFg
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "RaveForge")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PluginSources})
target_sources(RaveForge PRIVATE ${PluginSources})

target_compile_definitions(RaveForge
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RaveForge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (RAVEFORGE_BUILD_TESTS)
    juce_add_console_app(RaveForgeTest PRODUCT_NAME "RaveForgeTest")
    target_sources(RaveForgeTest PRIVATE
        Tests/RenderTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(RaveForgeTest PRIVATE Source)
    target_compile_definitions(RaveForgeTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        RAVEFORGE_TEST_BUILD=1)
    target_link_libraries(RaveForgeTest PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif ()
