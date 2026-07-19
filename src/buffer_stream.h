//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// BufferStream — IBStream implementation backed by a std::vector<uint8_t>.
// Used for saveState / loadState to round-trip plugin state through a JS Buffer.
//-----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <vector>

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h" // IBStream declared here transitively
#include "pluginterfaces/base/ibstream.h"

namespace nst3 {

class BufferStream final
    : public Steinberg::U::Implements<Steinberg::U::Directly<Steinberg::IBStream>> {
public:
    BufferStream();
    ~BufferStream() noexcept override;

    // Construct from existing data (for loadState).
    explicit BufferStream(const uint8_t* data, size_t size);

    // Take ownership of internal buffer (for saveState return value).
    std::vector<uint8_t> takeBuffer();

    // IBStream
    Steinberg::tresult PLUGIN_API read(void* buffer, Steinberg::int32 numBytes,
                                        Steinberg::int32* numBytesRead) override;
    Steinberg::tresult PLUGIN_API write(void* buffer, Steinberg::int32 numBytes,
                                         Steinberg::int32* numBytesWritten) override;
    Steinberg::tresult PLUGIN_API seek(Steinberg::int64 pos, Steinberg::int32 mode,
                                        Steinberg::int64* result) override;
    Steinberg::tresult PLUGIN_API tell(Steinberg::int64* pos) override;

private:
    std::vector<uint8_t> buffer_;
    size_t pos_ = 0;
};

// Versioned state-envelope helpers (Task 21).
//
// The envelope format is:
//   bytes 0-3:   magic 'NST3' (0x4E, 0x53, 0x54, 0x33)
//   byte  4:     version (currently 1)
//   bytes 5-8:   component-state length (uint32 little-endian)
//   bytes 9..:   component-state bytes
//   next 4:      controller-state length (uint32 little-endian)
//   next len2:   controller-state bytes
//
// Controller-state bytes may be empty (length 0). Legacy single-blob state
// buffers (without the NST3 magic) are detected by parseStateEnvelope and
// should be treated by the caller as plain component state.

// Compose a versioned envelope from component-state and controller-state
// bytes. Controller state may be empty.
std::vector<uint8_t> composeStateEnvelope(const std::vector<uint8_t>& componentState,
                                          const std::vector<uint8_t>& controllerState);

// Parse a versioned envelope. Returns true if the buffer matches the envelope
// format (magic 'NST3' + version 1) AND all length fields are consistent with
// the buffer size. On success, fills componentState and controllerState with
// copies of the sliced bytes. Returns false if the buffer is not an envelope
// (caller should treat the whole buffer as legacy component state).
bool parseStateEnvelope(const uint8_t* data, size_t size,
                        std::vector<uint8_t>& componentState,
                        std::vector<uint8_t>& controllerState);

} // namespace nst3
