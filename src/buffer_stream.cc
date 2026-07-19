//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// BufferStream implementation
//-----------------------------------------------------------------------------
#include "buffer_stream.h"

#include <algorithm>
#include <cstring>

namespace nst3 {

BufferStream::BufferStream() = default;

BufferStream::~BufferStream() noexcept = default;

BufferStream::BufferStream(const uint8_t* data, size_t size) {
    if (data && size > 0) {
        buffer_.assign(data, data + size);
    }
    pos_ = 0;
}

std::vector<uint8_t> BufferStream::takeBuffer() {
    return std::move(buffer_);
}

Steinberg::tresult PLUGIN_API BufferStream::read(void* buffer, Steinberg::int32 numBytes,
                                                   Steinberg::int32* numBytesRead) {
    if (numBytesRead) *numBytesRead = 0;
    if (!buffer || numBytes <= 0) return Steinberg::kInvalidArgument;
    if (pos_ >= buffer_.size()) return Steinberg::kResultFalse; // EOF

    size_t available = buffer_.size() - pos_;
    size_t toRead = std::min<size_t>(static_cast<size_t>(numBytes), available);
    std::memcpy(buffer, buffer_.data() + pos_, toRead);
    pos_ += toRead;
    if (numBytesRead) *numBytesRead = static_cast<Steinberg::int32>(toRead);
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API BufferStream::write(void* buffer, Steinberg::int32 numBytes,
                                                    Steinberg::int32* numBytesWritten) {
    if (numBytesWritten) *numBytesWritten = 0;
    if (!buffer || numBytes <= 0) return Steinberg::kInvalidArgument;
    size_t need = pos_ + static_cast<size_t>(numBytes);
    if (need > buffer_.size()) buffer_.resize(need);
    std::memcpy(buffer_.data() + pos_, buffer, static_cast<size_t>(numBytes));
    pos_ += static_cast<size_t>(numBytes);
    if (numBytesWritten) *numBytesWritten = numBytes;
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API BufferStream::seek(Steinberg::int64 pos, Steinberg::int32 mode,
                                                   Steinberg::int64* result) {
    int64_t newPos = 0;
    switch (mode) {
        case Steinberg::IBStream::kIBSeekSet: newPos = pos; break;
        case Steinberg::IBStream::kIBSeekCur: newPos = static_cast<int64_t>(pos_) + pos; break;
        case Steinberg::IBStream::kIBSeekEnd: newPos = static_cast<int64_t>(buffer_.size()) + pos; break;
        default: return Steinberg::kInvalidArgument;
    }
    if (newPos < 0) newPos = 0;
    pos_ = static_cast<size_t>(newPos);
    if (result) *result = newPos;
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API BufferStream::tell(Steinberg::int64* pos) {
    if (!pos) return Steinberg::kInvalidArgument;
    *pos = static_cast<Steinberg::int64>(pos_);
    return Steinberg::kResultTrue;
}

//------------------------------------------------------------------------
// Versioned state-envelope helpers (Task 21)
//------------------------------------------------------------------------
namespace {
constexpr char kStateEnvelopeMagic[4] = { 0x4E, 0x53, 0x54, 0x33 }; // "NST3"
constexpr uint8_t kStateEnvelopeVersion = 1;

inline void writeU32LE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

inline uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}
} // namespace

std::vector<uint8_t> composeStateEnvelope(const std::vector<uint8_t>& componentState,
                                          const std::vector<uint8_t>& controllerState) {
    // Header: 4 (magic) + 1 (version) + 4 (compLen) = 9 bytes,
    // followed by component bytes, then 4 (ctrlLen) + controller bytes.
    std::vector<uint8_t> out;
    out.reserve(9 + componentState.size() + 4 + controllerState.size());
    out.insert(out.end(), kStateEnvelopeMagic, kStateEnvelopeMagic + 4);
    out.push_back(kStateEnvelopeVersion);
    writeU32LE(out, static_cast<uint32_t>(componentState.size()));
    out.insert(out.end(), componentState.begin(), componentState.end());
    writeU32LE(out, static_cast<uint32_t>(controllerState.size()));
    out.insert(out.end(), controllerState.begin(), controllerState.end());
    return out;
}

bool parseStateEnvelope(const uint8_t* data, size_t size,
                        std::vector<uint8_t>& componentState,
                        std::vector<uint8_t>& controllerState) {
    componentState.clear();
    controllerState.clear();
    // Minimum envelope: 4 (magic) + 1 (version) + 4 (compLen) + 4 (ctrlLen) = 13
    if (size < 13) return false;
    if (std::memcmp(data, kStateEnvelopeMagic, 4) != 0) return false;
    if (data[4] != kStateEnvelopeVersion) return false;

    const uint8_t* p = data + 5;
    uint32_t compLen = readU32LE(p);
    p += 4;
    // Bounds check: component bytes must fit in the buffer (including the
    // 4-byte controller length that follows).
    if (static_cast<size_t>(compLen) > size - 13) return false;
    const uint8_t* compStart = p;
    p += compLen;
    uint32_t ctrlLen = readU32LE(p);
    p += 4;
    // Bounds check: controller bytes must fit exactly to the end of the buffer.
    if (static_cast<size_t>(ctrlLen) != size - 13 - compLen) return false;

    componentState.assign(compStart, compStart + compLen);
    controllerState.assign(p, p + ctrlLen);
    return true;
}

} // namespace nst3
