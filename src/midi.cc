//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// MIDI helpers — convert JS MIDI event objects to VST3 Event structs and back.
//
// VST3 only defines dedicated event types for Note On/Off, Poly Pressure,
// and SysEx (DataEvent). Controller, Program Change, Channel Pressure, and
// Pitch Bend are carried via LegacyMIDICCOutEvent using the ControllerNumbers
// enum (kAfterTouch=128, kPitchBend=129, kCtrlProgramChange=130, ...).
//-----------------------------------------------------------------------------
#include "midi.h"

#include <cstring>

namespace nst3 {

namespace {

void zeroEvent(Steinberg::Vst::Event& e, bool isLive = true) {
    std::memset(&e, 0, sizeof(e));
    if (isLive) {
        e.flags = Steinberg::Vst::Event::kIsLive;
    } else {
        e.flags = 0;
    }
}

} // namespace

bool midiBytesToEvent(const uint8_t* bytes, size_t numBytes, int32_t sampleOffset,
                      Steinberg::Vst::Event& outEvent,
                      bool isLive, int32_t noteId) {
    if (!bytes || numBytes == 0) return false;
    uint8_t status = bytes[0];
    if (status < 0x80) return false; // not a status byte

    uint8_t hi = status & 0xF0;
    int channel = status & 0x0F;

    switch (hi) {
        case 0x80: { // Note Off
            if (numBytes < 3) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kNoteOffEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.noteOff.channel = static_cast<int16_t>(channel);
            outEvent.noteOff.pitch = static_cast<int16_t>(bytes[1] & 0x7F);
            outEvent.noteOff.velocity = static_cast<float>(bytes[2] & 0x7F) / 127.f;
            outEvent.noteOff.noteId = noteId;
            return true;
        }
        case 0x90: { // Note On
            if (numBytes < 3) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kNoteOnEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.noteOn.channel = static_cast<int16_t>(channel);
            outEvent.noteOn.pitch = static_cast<int16_t>(bytes[1] & 0x7F);
            outEvent.noteOn.velocity = static_cast<float>(bytes[2] & 0x7F) / 127.f;
            outEvent.noteOn.noteId = noteId;
            return true;
        }
        case 0xA0: { // Poly Aftertouch
            if (numBytes < 3) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kPolyPressureEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.polyPressure.channel = static_cast<int16_t>(channel);
            outEvent.polyPressure.pitch = static_cast<int16_t>(bytes[1] & 0x7F);
            outEvent.polyPressure.pressure = static_cast<float>(bytes[2] & 0x7F) / 127.f;
            outEvent.polyPressure.noteId = noteId;
            return true;
        }
        case 0xB0: { // Controller
            if (numBytes < 3) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(bytes[1] & 0x7F);
            outEvent.midiCCOut.value = static_cast<int8_t>(bytes[2] & 0x7F);
            outEvent.midiCCOut.value2 = 0;
            return true;
        }
        case 0xC0: { // Program Change
            if (numBytes < 2) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(Steinberg::Vst::kCtrlProgramChange);
            outEvent.midiCCOut.value = static_cast<int8_t>(bytes[1] & 0x7F);
            outEvent.midiCCOut.value2 = 0;
            return true;
        }
        case 0xD0: { // Channel Pressure
            if (numBytes < 2) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(Steinberg::Vst::kAfterTouch);
            outEvent.midiCCOut.value = static_cast<int8_t>(bytes[1] & 0x7F);
            outEvent.midiCCOut.value2 = 0;
            return true;
        }
        case 0xE0: { // Pitch Bend
            if (numBytes < 3) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(Steinberg::Vst::kPitchBend);
            outEvent.midiCCOut.value = static_cast<int8_t>(bytes[1] & 0x7F);
            outEvent.midiCCOut.value2 = static_cast<int8_t>(bytes[2] & 0x7F);
            return true;
        }
        case 0xF0: { // System messages — only SysEx (0xF0) supported
            if (status != 0xF0) return false;
            // Find end byte (0xF7); if absent, treat whole input as SysEx
            size_t end = numBytes;
            for (size_t i = 1; i < numBytes; ++i) {
                if (bytes[i] == 0xF7) { end = i; break; }
            }
            size_t sysexLen = (end > 1) ? (end - 1) : 0;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kDataEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.data.bytes = const_cast<uint8_t*>(bytes + 1);
            outEvent.data.size = static_cast<uint32_t>(sysexLen);
            outEvent.data.type = Steinberg::Vst::DataEvent::kMidiSysEx;
            return true;
        }
        default:
            return false;
    }
}

bool structuredMidiToEvent(int type, int channel, int note, int velocity,
                           int controllerNumber, int controllerValue,
                           int programNumber, int pressure, int pitchBend,
                           const uint8_t* sysExData, size_t sysExSize,
                           int32_t sampleOffset,
                           Steinberg::Vst::Event& outEvent,
                           bool isLive, int32_t noteId) {
    auto t = static_cast<MidiEventType>(type);
    switch (t) {
        case MidiEventType::NoteOff:
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kNoteOffEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.noteOff.channel = static_cast<int16_t>(channel & 0x0F);
            outEvent.noteOff.pitch = static_cast<int16_t>(note & 0x7F);
            outEvent.noteOff.velocity = static_cast<float>(velocity & 0x7F) / 127.f;
            outEvent.noteOff.noteId = noteId;
            return true;
        case MidiEventType::NoteOn:
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kNoteOnEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.noteOn.channel = static_cast<int16_t>(channel & 0x0F);
            outEvent.noteOn.pitch = static_cast<int16_t>(note & 0x7F);
            outEvent.noteOn.velocity = static_cast<float>(velocity & 0x7F) / 127.f;
            outEvent.noteOn.noteId = noteId;
            return true;
        case MidiEventType::PolyPressure:
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kPolyPressureEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.polyPressure.channel = static_cast<int16_t>(channel & 0x0F);
            outEvent.polyPressure.pitch = static_cast<int16_t>(note & 0x7F);
            outEvent.polyPressure.pressure = static_cast<float>(pressure & 0x7F) / 127.f;
            outEvent.polyPressure.noteId = noteId;
            return true;
        case MidiEventType::Controller:
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel & 0x0F);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(controllerNumber & 0x7F);
            outEvent.midiCCOut.value = static_cast<int8_t>(controllerValue & 0x7F);
            outEvent.midiCCOut.value2 = 0;
            return true;
        case MidiEventType::ProgramChange:
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel & 0x0F);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(Steinberg::Vst::kCtrlProgramChange);
            outEvent.midiCCOut.value = static_cast<int8_t>(programNumber & 0x7F);
            outEvent.midiCCOut.value2 = 0;
            return true;
        case MidiEventType::ChannelPressure:
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel & 0x0F);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(Steinberg::Vst::kAfterTouch);
            outEvent.midiCCOut.value = static_cast<int8_t>(pressure & 0x7F);
            outEvent.midiCCOut.value2 = 0;
            return true;
        case MidiEventType::PitchBend: {
            // pitchBend is a signed integer in [-8192, 8191]; convert to 14-bit unsigned [0, 16383].
            int32_t pb = pitchBend + 8192;
            uint8_t lsb = static_cast<uint8_t>(pb & 0x7F);
            uint8_t msb = static_cast<uint8_t>((pb >> 7) & 0x7F);
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.midiCCOut.channel = static_cast<int8_t>(channel & 0x0F);
            outEvent.midiCCOut.controlNumber = static_cast<uint8_t>(Steinberg::Vst::kPitchBend);
            outEvent.midiCCOut.value = static_cast<int8_t>(lsb);
            outEvent.midiCCOut.value2 = static_cast<int8_t>(msb);
            return true;
        }
        case MidiEventType::SysEx: {
            if (!sysExData || sysExSize == 0) return false;
            zeroEvent(outEvent, isLive);
            outEvent.type = Steinberg::Vst::Event::kDataEvent;
            outEvent.sampleOffset = sampleOffset;
            outEvent.data.bytes = const_cast<uint8_t*>(sysExData);
            outEvent.data.size = static_cast<uint32_t>(sysExSize);
            outEvent.data.type = Steinberg::Vst::DataEvent::kMidiSysEx;
            return true;
        }
    }
    return false;
}

bool eventToMidiOut(const Steinberg::Vst::Event& event, MidiEventOut& out) {
    switch (event.type) {
        case Steinberg::Vst::Event::kNoteOnEvent:
            out.type = static_cast<int>(MidiEventType::NoteOn);
            out.channel = event.noteOn.channel;
            out.note = event.noteOn.pitch;
            out.velocity = static_cast<int>(event.noteOn.velocity * 127.f);
            out.sampleOffset = event.sampleOffset;
            return true;
        case Steinberg::Vst::Event::kNoteOffEvent:
            out.type = static_cast<int>(MidiEventType::NoteOff);
            out.channel = event.noteOff.channel;
            out.note = event.noteOff.pitch;
            out.velocity = static_cast<int>(event.noteOff.velocity * 127.f);
            out.sampleOffset = event.sampleOffset;
            return true;
        case Steinberg::Vst::Event::kPolyPressureEvent:
            out.type = static_cast<int>(MidiEventType::PolyPressure);
            out.channel = event.polyPressure.channel;
            out.note = event.polyPressure.pitch;
            out.pressure = static_cast<int>(event.polyPressure.pressure * 127.f);
            out.sampleOffset = event.sampleOffset;
            return true;
        case Steinberg::Vst::Event::kLegacyMIDICCOutEvent: {
            uint8_t cn = event.midiCCOut.controlNumber;
            int8_t ch = event.midiCCOut.channel;
            int8_t v1 = event.midiCCOut.value;
            int8_t v2 = event.midiCCOut.value2;
            out.channel = ch;
            out.sampleOffset = event.sampleOffset;
            if (cn == Steinberg::Vst::kCtrlProgramChange) {
                out.type = static_cast<int>(MidiEventType::ProgramChange);
                out.programNumber = v1;
            } else if (cn == Steinberg::Vst::kAfterTouch) {
                out.type = static_cast<int>(MidiEventType::ChannelPressure);
                out.pressure = v1;
            } else if (cn == Steinberg::Vst::kPitchBend) {
                out.type = static_cast<int>(MidiEventType::PitchBend);
                int32_t pb = (static_cast<int32_t>(v1 & 0x7F)) |
                             (static_cast<int32_t>(v2 & 0x7F) << 7);
                out.pitchBend = pb - 8192;
            } else {
                // Regular CC (0-127) or kCtrlPolyPressure (131)
                out.type = static_cast<int>(MidiEventType::Controller);
                out.controllerNumber = cn;
                out.controllerValue = v1;
            }
            return true;
        }
        case Steinberg::Vst::Event::kDataEvent:
            if (event.data.type == Steinberg::Vst::DataEvent::kMidiSysEx) {
                out.type = static_cast<int>(MidiEventType::SysEx);
                out.sampleOffset = event.sampleOffset;
                out.sysEx.assign(event.data.bytes, event.data.bytes + event.data.size);
                return true;
            }
            return false;
        default:
            return false;
    }
}

} // namespace nst3
