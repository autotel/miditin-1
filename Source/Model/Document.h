#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <PresetFile.h>
#include "ParamRegistry.h"
#include "Patch.h"

/** A whole sound as one tree: every host parameter, by id, in natural units, plus the `Patch`.
    It is what a preset file holds and what the host saves, so the two never mean different things.

        <MiditinPreset><Params en_rate="0.12" .../><Patch pulse="wheel" .../></MiditinPreset>
*/
namespace mdt
{
inline const juce::Identifier documentType { "MiditinPreset" };

juce::ValueTree makeDocument (const Patch&, const ParamValues&);

/** Anything missing, out of range or unreadable falls back to its default. An invalid tree gives
    the defaults outright -- what the librarian's New hands over. */
void readDocument (const juce::ValueTree&, Patch&, ParamValues&);

struct FactoryPreset
{
    juce::String name, category, author, description;
    juce::StringArray tags;
    Patch patch;
    ParamValues pv;
};

/** Shipped inside the binary; every pulse generator appears in at least one. */
const std::vector<FactoryPreset>& factoryPresets();

/** The bytes a .miditin file of this preset would hold, metadata included -- what the librarian's
    built-in entries point into. */
juce::MemoryBlock factoryPresetData (const FactoryPreset&, const atl::PresetFormat&);
} // namespace mdt
