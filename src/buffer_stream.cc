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

} // namespace nst3
