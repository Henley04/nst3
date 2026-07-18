//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// String conversion helpers between UTF-8 (JS) and UTF-16 (VST3 String128)
//-----------------------------------------------------------------------------
#pragma once

#include <string>
#include <cstdint>

#include "pluginterfaces/base/fstrdefs.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace nst3 {

// Convert a VST3 String128 (UTF-16, fixed-size) to a UTF-8 std::string.
std::string string128ToUtf8(const Steinberg::Vst::String128 src);

// Convert a UTF-8 std::string to a VST3 String128 (writes into dst, ensures null-termination).
void utf8ToString128(const std::string& src, Steinberg::Vst::String128 dst);

// Convert a VST3 TChar UTF-16 buffer (length-bounded) to UTF-8.
std::string tcharToUtf8(const Steinberg::Vst::TChar* src, size_t maxLen);

} // namespace nst3
