//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Version helpers
//-----------------------------------------------------------------------------
#include "version.h"

#include "pluginterfaces/vst/ivsthostapplication.h"

namespace nst3 {

std::string vst3SdkVersion() {
    // The VST3 SDK exposes its version via kVstVersionString if available;
    // otherwise we compile a static string. Steinberg updates this in
    // pluginterfaces/vst/ivstaudioprocessor.h (kVstVersionString).
    return kVstVersionString;
}

std::string nst3Version() {
    return "0.3.1";
}

} // namespace nst3
