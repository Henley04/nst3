//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Error helpers — implementation of throwNapiError and errorCodeString.
//-----------------------------------------------------------------------------
#include "errors.h"

#include <sstream>

namespace nst3 {

const char* errorCodeString(ErrorCode code) {
    switch (code) {
        case ErrorCode::LoadFailed:              return "VST3_LOAD_FAILED";
        case ErrorCode::FactoryMissing:          return "VST3_FACTORY_MISSING";
        case ErrorCode::ComponentCreationFailed: return "VST3_COMPONENT_CREATION_FAILED";
        case ErrorCode::ControllerMissing:       return "VST3_CONTROLLER_MISSING";
        case ErrorCode::NotActive:               return "VST3_NOT_ACTIVE";
        case ErrorCode::NotProcessing:           return "VST3_NOT_PROCESSING";
        case ErrorCode::Faulted:                 return "VST3_FAULTED";
        case ErrorCode::PlatformUnsupported:     return "VST3_PLATFORM_UNSUPPORTED";
        case ErrorCode::InvalidParameter:        return "VST3_INVALID_PARAMETER";
        case ErrorCode::InvalidBuffer:           return "VST3_INVALID_BUFFER";
        case ErrorCode::ProcessingError:         return "VST3_PROCESSING_ERROR";
        case ErrorCode::StateError:              return "VST3_STATE_ERROR";
        case ErrorCode::MidiError:               return "VST3_MIDI_ERROR";
        case ErrorCode::Unknown:                 return "VST3_UNKNOWN";
    }
    return "VST3_UNKNOWN";
}

void throwNapiError(Napi::Env env, ErrorCode code, const std::string& message) {
    Napi::Error err = Napi::Error::New(env, message);
    err.Set("code", Napi::String::New(env, errorCodeString(code)));
    throw err;
}

} // namespace nst3
