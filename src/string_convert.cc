//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// String conversion helpers between UTF-8 (JS) and UTF-16 (VST3 String128)
//-----------------------------------------------------------------------------
#include "string_convert.h"

#include <cstring>
#include <codecvt>
#include <locale>

namespace nst3 {

namespace {

// C++11 codecvt facet. Deprecated in C++17 but still widely available and
// sufficient for our needs (no ICU dependency). We instantiate it once per
// translation unit; it's stateless and thread-safe for conversions.
std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>& converter() {
    static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
    return conv;
}

} // namespace

std::string string128ToUtf8(const Steinberg::Vst::String128 src) {
    if (!src) return {};
    // Find length up to first null (String128 is null-terminated UTF-16).
    size_t len = 0;
    while (len < 127 && src[len] != 0) ++len;
    if (len == 0) return {};
    try {
        auto u16 = reinterpret_cast<const char16_t*>(src);
        return converter().to_bytes(u16, u16 + len);
    } catch (...) {
        // Fallback: ASCII-only truncation if codecvt fails.
        std::string out;
        out.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            Steinberg::Vst::TChar ch = src[i];
            out.push_back(ch < 128 ? static_cast<char>(ch) : '?');
        }
        return out;
    }
}

void utf8ToString128(const std::string& src, Steinberg::Vst::String128 dst) {
    if (!dst) return;
    try {
        auto u16 = converter().from_bytes(src);
        size_t n = u16.size();
        if (n > 126) n = 126;
        for (size_t i = 0; i < n; ++i) {
            dst[i] = static_cast<Steinberg::Vst::TChar>(u16[i]);
        }
        dst[n] = 0;
    } catch (...) {
        // Fallback: ASCII copy
        size_t n = src.size();
        if (n > 126) n = 126;
        for (size_t i = 0; i < n; ++i) {
            dst[i] = static_cast<Steinberg::Vst::TChar>(static_cast<unsigned char>(src[i]));
        }
        dst[n] = 0;
    }
}

std::string tcharToUtf8(const Steinberg::Vst::TChar* src, size_t maxLen) {
    if (!src || maxLen == 0) return {};
    size_t len = 0;
    while (len < maxLen && src[len] != 0) ++len;
    if (len == 0) return {};
    try {
        auto u16 = reinterpret_cast<const char16_t*>(src);
        return converter().to_bytes(u16, u16 + len);
    } catch (...) {
        std::string out;
        out.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            Steinberg::Vst::TChar ch = src[i];
            out.push_back(ch < 128 ? static_cast<char>(ch) : '?');
        }
        return out;
    }
}

} // namespace nst3
