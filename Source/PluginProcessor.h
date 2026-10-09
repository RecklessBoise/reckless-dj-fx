#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/BeatFx.h"
#include "dsp/ColorFx.h"
#include "dsp/TempoSync.h"
#include "params/Parameters.h"
#include "presets/PresetManager.h"

class RecklessDJFXProcessor final : public juce::AudioProcessor
{
public:
    RecklessDJFXProcessor();
    ~RecklessDJFXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override { return "Reckless DJ FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    rdfx::PresetManager& getPresetManager() { return *presetManager; }

    /** Tempo actually used by the DSP (for the BPM display). */
    double getCurrentBpm() const noexcept { return currentBpm.load(); }
    /** Where the tempo in use comes from: Host = the DAW (TAP / MANUAL only without a host tempo). */
    rdfx::BpmMode getTempoSource() const noexcept { return (rdfx::BpmMode) tempoSource.load(); }
    bool isBeatFxEngaged() const noexcept { return beatEngaged.load(); }

    /** Editor size scale, persisted with the plugin state. */
    float editorScale = 1.0f;

    /** Tap tempo (message thread): updates the BPM parameter and switches the source to TAP. */
    void tapTempo();

private:
    rdfx::BeatFxEngine::Settings readBeatSettings() const noexcept;
    rdfx::ColorFxEngine::Settings readColorSettings() const noexcept;

    rdfx::TempoSync tempo;
    rdfx::TempoSync::Tapper tapper;
    rdfx::BeatFxEngine beatFx;
    rdfx::ColorFxEngine colorFx;
    juce::SmoothedValue<float> outGain;
    std::unique_ptr<rdfx::PresetManager> presetManager;
    std::atomic<double> currentBpm { 128.0 };
    std::atomic<int> tempoSource { (int) rdfx::BpmMode::Manual };
    std::atomic<bool> beatEngaged { false };

    struct Raw
    {
        std::atomic<float>* beatOn {}; std::atomic<float>* beatType {}; std::atomic<float>* beatIdx {};
        std::atomic<float>* beatSync {}; std::atomic<float>* timeMs {}; std::atomic<float>* level {};
        std::atomic<float>* fxLow {}; std::atomic<float>* fxMid {}; std::atomic<float>* fxHi {};
        std::atomic<float>* quantize {}; std::atomic<float>* tape {};
        std::atomic<float>* colorOn {}; std::atomic<float>* colorType {}; std::atomic<float>* colorAmt {};
        std::atomic<float>* colorParam {}; std::atomic<float>* bpmMode {}; std::atomic<float>* bpm {};
        std::atomic<float>* outGain {};
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessDJFXProcessor)
};
