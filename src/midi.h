//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// MIDI helpers — convert JS MIDI event objects to VST3 Event structs and back.
//-----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <vector>

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

namespace nst3 {

// JS-visible MIDI event types (must match index.d.ts).
enum class MidiEventType {
    NoteOff         = 0,
    NoteOn          = 1,
    PolyPressure    = 2,
    Controller      = 3,
    ProgramChange   = 4,
    ChannelPressure = 5,
    PitchBend       = 6,
    SysEx           = 7,
};

// Convert raw MIDI 1.0 bytes (status + data bytes, no running status) to a
// VST3 Event. Returns false if the bytes are malformed or empty.
bool midiBytesToEvent(const uint8_t* bytes, size_t numBytes, int32_t sampleOffset,
                      Steinberg::Vst::Event& outEvent);

// Convert a structured MIDI event (matching JS shape) to a VST3 Event.
// For SysEx, payload is provided separately; for other types, fields map
// directly. Returns false on invalid input.
bool structuredMidiToEvent(int type, int channel, int note, int velocity,
                           int controllerNumber, int controllerValue,
                           int programNumber, int pressure, int pitchBend,
                           const uint8_t* sysExData, size_t sysExSize,
                           int32_t sampleOffset,
                           Steinberg::Vst::Event& outEvent);

// Convert a VST3 Event back to a structured representation suitable for JS.
// Caller is responsible for freeing the SysEx buffer via the returned size.
struct MidiEventOut {
    int type = 0;
    int channel = 0;
    int note = 0;
    int velocity = 0;
    int controllerNumber = 0;
    int controllerValue = 0;
    int programNumber = 0;
    int pressure = 0;
    int pitchBend = 0;
    int32_t sampleOffset = 0;
    std::vector<uint8_t> sysEx; // empty for non-SysEx events
};

bool eventToMidiOut(const Steinberg::Vst::Event& event, MidiEventOut& out);

} // namespace nst3
