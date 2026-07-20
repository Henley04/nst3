//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// BufferStream — IBStream + IStreamAttributes implementation backed by a
// std::vector<uint8_t>. Used for saveState / loadState to round-trip plugin
// state through a JS Buffer. Plugins query IStreamAttributes during
// setState/getState to obtain the loading context (project vs preset, file
// path). The default attributes return an empty name and an empty
// attribute list — host code can populate them via setFileName /
// setAttribute before handing the stream to the plugin.
//-----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstattributes.h"   // IStreamAttributes
#include "pluginterfaces/vst/ivstaudioprocessor.h" // IBStream declared here transitively

namespace nst3 {

class BufferStream final
    : public Steinberg::U::Implements<Steinberg::U::Directly<Steinberg::IBStream>,
                                      Steinberg::U::Directly<Steinberg::Vst::IStreamAttributes>> {
public:
    BufferStream();
    ~BufferStream() noexcept override;

    // Construct from existing data (for loadState).
    explicit BufferStream(const uint8_t* data, size_t size);

    // Take ownership of internal buffer (for saveState return value).
    std::vector<uint8_t> takeBuffer();

    //--- IBStream -------------------------------------------------------
    Steinberg::tresult PLUGIN_API read(void* buffer, Steinberg::int32 numBytes,
                                        Steinberg::int32* numBytesRead) override;
    Steinberg::tresult PLUGIN_API write(void* buffer, Steinberg::int32 numBytes,
                                         Steinberg::int32* numBytesWritten) override;
    Steinberg::tresult PLUGIN_API seek(Steinberg::int64 pos, Steinberg::int32 mode,
                                        Steinberg::int64* result) override;
    Steinberg::tresult PLUGIN_API tell(Steinberg::int64* pos) override;

    //--- IStreamAttributes ----------------------------------------------
    // Returns the file name (without extension) for the stream. Empty by
    // default; host code can call setFileName to populate before passing
    // the stream to the plugin (useful when loadState is given a Buffer
    // decoded from a preset file the host knows about).
    Steinberg::tresult PLUGIN_API getFileName(Steinberg::Vst::String128 name) override;
    // Returns the meta-info attribute list. Lazily creates a
    // HostAttributeList-backed IAttributeList on first call; subsequent
    // calls return the same instance. May return nullptr if allocation
    // fails (plugins must handle this gracefully).
    Steinberg::Vst::IAttributeList* PLUGIN_API getAttributes() override;

    //--- Host-side mutators ---------------------------------------------
    // Set the file name that getFileName() will return. The input is UTF-8;
    // internally we convert to String128 (UTF-16) at query time.
    void setFileName(const std::string& utf8Name);

    // Set the PresetAttributes::kStateType attribute (commonly
    // StateType::kProject or StateType::kPreset). Convenience wrapper
    // around getAttributes()->setString.
    bool setStateType(const std::string& utf8StateType);

    // Set the PresetAttributes::kFilePathStringType attribute. Convenience
    // wrapper for the common case where the host knows the full path of
    // the preset file being loaded.
    bool setFilePath(const std::string& utf8Path);

private:
    std::vector<uint8_t> buffer_;
    size_t pos_ = 0;
    std::string utf8FileName_; // Lazily converted to String128 in getFileName
    Steinberg::IPtr<Steinberg::Vst::IAttributeList> attributes_;
};

// Versioned state-envelope helpers (Task 21).
//
// The envelope format is:
//   bytes 0-4:   magic 'NST3' (0x4E, 0x53, 0x54, 0x33)
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
