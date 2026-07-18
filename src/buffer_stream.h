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

} // namespace nst3
