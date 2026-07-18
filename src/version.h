//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Version helpers
//-----------------------------------------------------------------------------
#pragma once

#include <string>

namespace nst3 {

// VST3 SDK version string (e.g. "VST 3.8.0 Build 66")
std::string vst3SdkVersion();

// nst3 native addon version
std::string nst3Version();

} // namespace nst3
