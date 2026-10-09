#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace rdfx
{
/** A factory preset: parameter values are in real units (choice index, 0/1 for bools, ms, dB...). */
struct FactoryPreset
{
    juce::String name;
    juce::String category;
    std::vector<std::pair<juce::String, float>> values;
};

/** All built-in presets (generated once, names are unique). */
const std::vector<FactoryPreset>& getFactoryPresets();
} // namespace rdfx
