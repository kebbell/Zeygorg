cmake_minimum_required(VERSION 3.22)

project(Zeygorg VERSION 1.0.0)

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

option(ZEYGORG_BUILD_TESTS "Build the offline render test" ON)

# The sample library is compiled into the binary (see Samples/).
file(GLOB SampleFiles CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Samples/*/*.wav)
list(SORT SampleFiles)
juce_add_binary_data(ZeygorgSamples SOURCES ${SampleFiles})
set_target_properties(ZeygorgSamples PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

set(PluginSources
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/Presets.h
    Source/FactoryPresets.h
    Source/SampleLibrary.h
    Source/DSP/Engine.h
    Source/DSP/RaveStack.h
    Source/DSP/Dirt.h
    Source/DSP/Modulation.h
    Source/DSP/Delay.h
    Source/DSP/Dynamics.h
    Source/DSP/Sample.h
    Source/DSP/Synth.h
    Source/DSP/Sequencer.h
    Source/DSP/NoteFx.h
    Source/UI/Theme.h
    Source/UI/Widgets.h
    Source/UI/Widgets.cpp
    Source/UI/Displays.h
    Source/UI/Displays.cpp
    Source/UI/LayerPanel.h
    Source/UI/LayerPanel.cpp
    Source/UI/SequencerPanel.h
    Source/UI/SequencerPanel.cpp
    Source/UI/PresetBar.h
    Source/UI/PresetBar.cpp
    Source/UI/Pages.h
    Source/UI/Pages.cpp)

set(EditorSources
    Source/PluginEditor.cpp
    Source/UI/Widgets.cpp
    Source/UI/Displays.cpp
    Source/UI/LayerPanel.cpp
    Source/UI/SequencerPanel.cpp
    Source/UI/PresetBar.cpp
    Source/UI/Pages.cpp)

juce_add_plugin(Zeygorg
    COMPANY_NAME "Kebbell"
    BUNDLE_ID com.kebbell.zeygorg
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Kbbl
    PLUGIN_CODE Zygg
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Zeygorg")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PluginSources})
target_sources(Zeygorg PRIVATE ${PluginSources})

target_compile_definitions(Zeygorg
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Zeygorg
    PRIVATE
        ZeygorgSamples
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# The linker only ad-hoc signs the binary, and JUCE adds files (moduleinfo.json)
# afterwards, which leaves the bundles with invalid signatures that hosts like
# Live refuse to load. Re-sign each whole bundle once it's complete.
if (APPLE)
    foreach (format VST3 AU Standalone)
        add_custom_command(TARGET Zeygorg_${format} POST_BUILD
            COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:Zeygorg_${format}>"
            VERBATIM)
    endforeach ()
endif ()

if (ZEYGORG_BUILD_TESTS)
    juce_add_console_app(ZeygorgTest PRODUCT_NAME "ZeygorgTest")
    target_sources(ZeygorgTest PRIVATE
        Tests/RenderTest.cpp
        Source/PluginProcessor.cpp
        ${EditorSources})
    target_include_directories(ZeygorgTest PRIVATE Source)
    target_compile_definitions(ZeygorgTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        ZEYGORG_TEST_BUILD=1
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ZeygorgTest PRIVATE
        ZeygorgSamples
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    # Loads the built bundles as a host would (see Tests/LoadTest.cpp).
    juce_add_console_app(ZeygorgLoadTest PRODUCT_NAME "ZeygorgLoadTest")
    target_sources(ZeygorgLoadTest PRIVATE Tests/LoadTest.cpp)
    target_compile_definitions(ZeygorgLoadTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1)
    target_link_libraries(ZeygorgLoadTest PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        "$<$<PLATFORM_ID:Darwin>:-framework CoreAudioKit>"
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif ()
