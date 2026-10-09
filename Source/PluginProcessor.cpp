#include "PluginProcessor.h"

#if ! RDFX_HEADLESS
 #include "PluginEditor.h"
#endif

using namespace rdfx;

RecklessDJFXProcessor::RecklessDJFXProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RecklessDJFX", createParameterLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw.beatOn = get (ParamID::beatOn);       raw.beatType = get (ParamID::beatType);
    raw.beatIdx = get (ParamID::beatIdx);     raw.beatSync = get (ParamID::beatSync);
    raw.timeMs = get (ParamID::timeMs);       raw.level = get (ParamID::level);
    raw.fxLow = get (ParamID::fxLow);         raw.fxMid = get (ParamID::fxMid);
    raw.fxHi = get (ParamID::fxHi);           raw.quantize = get (ParamID::quantize);
    raw.tape = get (ParamID::tape);           raw.colorOn = get (ParamID::colorOn);
    raw.colorType = get (ParamID::colorType); raw.colorAmt = get (ParamID::colorAmt);
    raw.colorParam = get (ParamID::colorParam);
    raw.bpmMode = get (ParamID::bpmMode);     raw.bpm = get (ParamID::bpm);
    raw.outGain = get (ParamID::outGain);

    presetManager = std::make_unique<PresetManager> (apvts);
}

bool RecklessDJFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void RecklessDJFXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    tempo.prepare (sampleRate);
    beatFx.prepare (sampleRate, samplesPerBlock);
    colorFx.prepare (sampleRate, samplesPerBlock);
    outGain.reset (sampleRate, 0.02);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.outGain->load()));
}

BeatFxEngine::Settings RecklessDJFXProcessor::readBeatSettings() const noexcept
{
    BeatFxEngine::Settings s;
    s.on = raw.beatOn->load() > 0.5f;
    s.type = (BeatFxType) juce::jlimit (0, kNumBeatFx - 1, (int) raw.beatType->load());
    s.beatIdx = juce::jlimit (0, kNumBeats - 1, (int) raw.beatIdx->load());
    s.sync = raw.beatSync->load() > 0.5f;
    s.timeMs = raw.timeMs->load();
    s.level = raw.level->load();
    s.low = raw.fxLow->load() > 0.5f;
    s.mid = raw.fxMid->load() > 0.5f;
    s.hi = raw.fxHi->load() > 0.5f;
    s.quantize = raw.quantize->load() > 0.5f;
    s.tape = raw.tape->load() > 0.5f;
    return s;
}

ColorFxEngine::Settings RecklessDJFXProcessor::readColorSettings() const noexcept
{
    ColorFxEngine::Settings s;
    s.on = raw.colorOn->load() > 0.5f;
    s.type = (ColorFxType) juce::jlimit (0, kNumColorFx - 1, (int) raw.colorType->load());
    s.amount = raw.colorAmt->load();
    s.param = raw.colorParam->load();
    return s;
}

void RecklessDJFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numSamples == 0)
        return;

    const auto mode = (BpmMode) juce::jlimit (0, 2, (int) raw.bpmMode->load());
    const auto& t = tempo.update (getPlayHead(), mode, raw.bpm->load(), numSamples);
    currentBpm.store (t.bpm);

    colorFx.process (buffer, readColorSettings(), t);    // Sound Color FX first, like on the mixer channel
    beatFx.process (buffer, readBeatSettings(), t);      // then the Beat FX on the master send
    beatEngaged.store (beatFx.isEngaged());

    outGain.setTargetValue (juce::Decibels::decibelsToGain (raw.outGain->load()));
    if (outGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outGain.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (outGain.getTargetValue());
    }
}

void RecklessDJFXProcessor::tapTempo()
{
    const double bpm = tapper.tap (juce::Time::getMillisecondCounterHiRes());
    if (bpm <= 0.0)
        return;
    if (auto* p = apvts.getParameter (ParamID::bpm))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) bpm));
    if (auto* p = apvts.getParameter (ParamID::bpmMode))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) BpmMode::Tap));
}

//==============================================================================
int RecklessDJFXProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

int RecklessDJFXProcessor::getCurrentProgram()
{
    const auto& presets = getFactoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presetManager->getCurrentKey() == "factory:" + presets[(size_t) i].name)
            return i;
    return 0;
}

void RecklessDJFXProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        presetManager->load ("factory:" + presets[(size_t) index].name);
}

const juce::String RecklessDJFXProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void RecklessDJFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("editorScale", editorScale, nullptr);
    state.setProperty ("presetKey", presetManager->getCurrentKey(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RecklessDJFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            editorScale = (float) state.getProperty ("editorScale", 1.0f);
            presetManager->setCurrentKeySilently (state.getProperty ("presetKey", {}).toString());
            apvts.replaceState (state);
        }
}

//==============================================================================
bool RecklessDJFXProcessor::hasEditor() const
{
   #if RDFX_HEADLESS
    return false;
   #else
    return true;
   #endif
}

juce::AudioProcessorEditor* RecklessDJFXProcessor::createEditor()
{
   #if RDFX_HEADLESS
    return nullptr;
   #else
    return new RecklessDJFXEditor (*this);
   #endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecklessDJFXProcessor();
}
