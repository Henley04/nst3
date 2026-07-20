//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// BufferStream implementation
//-----------------------------------------------------------------------------
#include "buffer_stream.h"

#include <algorithm>
#include <cstring>
#include <codecvt>
#include <locale>

#include "string_convert.h"

// HostAttributeList (SDK example IAttributeList impl) for lazy getAttributes().
#include "public.sdk/source/vst/hosting/hostclasses.h"
// PresetAttributes::kStateType / kFilePathStringType keys for the convenience
// mutators setStateType / setFilePath.
#include "pluginterfaces/vst/vstpresetkeys.h"

namespace nst3 {

namespace {
// UTF-8 -> std::u16string (variable-length, used for file paths which can
// exceed String128's 127-character capacity). Mirrors the converter in
// string_convert.cc but returns a dynamic-length u16string.
std::u16string utf8ToU16(const std::string& src) {
    try {
        static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
        return conv.from_bytes(src);
    } catch (...) {
        // Fallback: ASCII-only truncation if codecvt fails.
        std::u16string out;
        out.reserve(src.size());
        for (unsigned char ch : src) {
            out.push_back(static_cast<char16_t>(ch < 128 ? ch : u'?'));
        }
        return out;
    }
}
} // namespace

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
// IStreamAttributes
//------------------------------------------------------------------------
// Plugins obtain IStreamAttributes via FUnknownPtr<IStreamAttributes>(stream)
// inside setState/getState to probe the loading context. We implement both
// methods minimally: getFileName returns the host-supplied UTF-8 name (empty
// by default), getAttributes lazily allocates an SDK HostAttributeList that
// host-side mutators (setStateType, setFilePath, or direct attribute writes)
// can populate before handing the stream to the plugin.

Steinberg::tresult PLUGIN_API BufferStream::getFileName(Steinberg::Vst::String128 name) {
    if (!name) return Steinberg::kInvalidArgument;
    // Convert the cached UTF-8 name to a null-terminated String128 (UTF-16).
    utf8ToString128(utf8FileName_, name);
    return Steinberg::kResultTrue;
}

Steinberg::Vst::IAttributeList* PLUGIN_API BufferStream::getAttributes() {
    if (!attributes_) {
        // Lazy-create on first query. HostAttributeList::make() returns an
        // IPtr<IAttributeList> with refcount=1; we store it as a strong
        // reference so subsequent calls return the same instance.
        if (auto al = Steinberg::Vst::HostAttributeList::make()) {
            attributes_ = al;
        }
        // On allocation failure, leave attributes_ null. Plugins must handle
        // a nullptr return from getAttributes gracefully per the SDK contract.
    }
    return attributes_;
}

//------------------------------------------------------------------------
// Host-side mutators
//------------------------------------------------------------------------
void BufferStream::setFileName(const std::string& utf8Name) {
    utf8FileName_ = utf8Name;
}

bool BufferStream::setStateType(const std::string& utf8StateType) {
    if (auto* al = getAttributes()) {
        auto u16 = utf8ToU16(utf8StateType);
        // StateType strings are short ("Project", "Default", "TrackPreset")
        // — a 256-char fixed buffer is more than enough. setString requires
        // a null-terminated TChar buffer.
        Steinberg::Vst::TChar buf[256];
        size_t n = u16.size();
        if (n > 255) n = 255;
        for (size_t i = 0; i < n; ++i) {
            buf[i] = static_cast<Steinberg::Vst::TChar>(u16[i]);
        }
        buf[n] = 0;
        return al->setString(Steinberg::Vst::PresetAttributes::kStateType, buf) == Steinberg::kResultTrue;
    }
    return false;
}

bool BufferStream::setFilePath(const std::string& utf8Path) {
    if (auto* al = getAttributes()) {
        auto u16 = utf8ToU16(utf8Path);
        // File paths can be long (SDK example uses 1024 chars); allocate
        // a dynamic buffer to avoid the 256-char truncation of setStateType.
        std::vector<Steinberg::Vst::TChar> buf(u16.size() + 1);
        for (size_t i = 0; i < u16.size(); ++i) {
            buf[i] = static_cast<Steinberg::Vst::TChar>(u16[i]);
        }
        buf[u16.size()] = 0;
        return al->setString(Steinberg::Vst::PresetAttributes::kFilePathStringType,
                             buf.data()) == Steinberg::kResultTrue;
    }
    return false;
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
