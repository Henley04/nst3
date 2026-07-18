//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Error helpers — translate C++ exceptions and VST3 result codes into
// structured Napi::Error instances with a stable `code` field.
//-----------------------------------------------------------------------------
#pragma once

#include <napi.h>
#include <string>
#include <stdexcept>

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/base/funknown.h"

namespace nst3 {

// Stable error codes exposed to JS via `Error.prototype.code`.
enum class ErrorCode {
    LoadFailed,
    FactoryMissing,
    ComponentCreationFailed,
    ControllerMissing,
    NotActive,
    NotProcessing,
    Faulted,
    PlatformUnsupported,
    InvalidParameter,
    InvalidBuffer,
    ProcessingError,
    StateError,
    MidiError,
    Unknown
};

// Maps an ErrorCode to its JS-visible string code (e.g. "VST3_LOAD_FAILED").
const char* errorCodeString(ErrorCode code);

// Throws a Napi::Error with the given code and message. The Napi::Error is
// constructed on the supplied env; this function does not return (it throws
// a C++ exception internally via Napi).
[[noreturn]] void throwNapiError(Napi::Env env, ErrorCode code, const std::string& message);

// NstException carries an ErrorCode alongside a human-readable message.
class NstException : public std::runtime_error {
public:
    NstException(ErrorCode code, const std::string& msg)
        : std::runtime_error(msg), code_(code) {}
    ErrorCode code() const noexcept { return code_; }
private:
    ErrorCode code_;
};

// Convenience: wraps a lambda in try/catch and translates any std::exception
// to a Napi::Error. Useful for SDK calls that may throw. NstException carries
// its own ErrorCode which is preserved; other std::exceptions become Unknown.
template <typename Fn>
auto translateExceptions(Napi::Env env, Fn&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const NstException& e) {
        throwNapiError(env, e.code(), e.what());
    } catch (const std::exception& e) {
        throwNapiError(env, ErrorCode::Unknown, e.what());
    } catch (...) {
        throwNapiError(env, ErrorCode::Unknown, "Unknown native exception");
    }
}

// Throw a NstException (caught by translateExceptions or by Host/PluginInstance
// methods directly).
[[noreturn]] inline void throwNst(ErrorCode code, const std::string& msg) {
    throw NstException(code, msg);
}

// Helper to check a Steinberg tresult and throw NstException on failure.
inline void checkTResult(Steinberg::tresult r, ErrorCode code, const std::string& ctx) {
    if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
        throwNst(code, ctx + " (tresult=" + std::to_string(static_cast<int>(r)) + ")");
    }
}

} // namespace nst3
