cmake_minimum_required(VERSION 3.22)

project(RecklessDJFX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(RDFX_BUILD_TESTS "Build the unit test runner" ON)
option(RDFX_COPY_AFTER_BUILD "Copy plugins to the user plug-in folders after build" OFF)

# JUCE: fetched from GitHub. To use a local checkout, configure with
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RecklessDJFX
    PRODUCT_NAME                "Reckless DJ FX"
    COMPANY_NAME                "Reckless"
    BUNDLE_ID                   "com.reckless.djfx"
    PLUGIN_MANUFACTURER_CODE    Rckl
    PLUGIN_CODE                 Rdfx
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     ${RDFX_COPY_AFTER_BUILD}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Delay Reverb Filter
    DESCRIPTION                 "DJ mixer style Beat FX and Sound Color FX")

set(RDFX_CORE_SOURCES
    Source/PluginProcessor.cpp
    Source/params/Parameters.cpp
    Source/dsp/TempoSync.cpp
    Source/dsp/BeatFx.cpp
    Source/dsp/ColorFx.cpp
    Source/presets/FactoryPresets.cpp
    Source/presets/PresetManager.cpp)

set(RDFX_UI_SOURCES
    Source/PluginEditor.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/Hardware.cpp
    Source/ui/Panels.cpp
    Source/ui/PresetBrowser.cpp)

target_sources(RecklessDJFX PRIVATE ${RDFX_CORE_SOURCES} ${RDFX_UI_SOURCES})
target_include_directories(RecklessDJFX PRIVATE Source)

target_compile_definitions(RecklessDJFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RecklessDJFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(RDFX_BUILD_TESTS)
    juce_add_console_app(RecklessDJFXTests PRODUCT_NAME "RecklessDJFXTests")
    target_sources(RecklessDJFXTests PRIVATE
        tests/TestMain.cpp
        tests/DspTests.cpp
        tests/PresetTests.cpp
        ${RDFX_CORE_SOURCES})
    target_include_directories(RecklessDJFXTests PRIVATE Source)
    target_compile_definitions(RecklessDJFXTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 RDFX_HEADLESS=1 JUCE_UNIT_TESTS=1)
    target_link_libraries(RecklessDJFXTests PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    # Renders the editor to PNG (README screenshots / visual checks)
    juce_add_console_app(RecklessDJFXSnapshot PRODUCT_NAME "RecklessDJFXSnapshot")
    target_sources(RecklessDJFXSnapshot PRIVATE tests/Snapshot.cpp ${RDFX_CORE_SOURCES} ${RDFX_UI_SOURCES})
    target_include_directories(RecklessDJFXSnapshot PRIVATE Source)
    target_compile_definitions(RecklessDJFXSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RecklessDJFXSnapshot PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME RecklessDJFXTests COMMAND RecklessDJFXTests)
endif()
