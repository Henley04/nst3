//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Plugin discovery — scans directories for .vst3 modules and reads metadata
// without instantiating DSP components.
//-----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

#include "public.sdk/source/vst/hosting/module.h"

namespace nst3 {

// Metadata for a single VST3 plugin class (one .vst3 module may export several).
struct PluginClassInfo {
    std::string path;             // path to the .vst3 module
    std::string name;             // class name
    std::string vendor;           // vendor (from class info or factory info)
    std::string version;          // version string
    std::string category;         // e.g. "Audio Module Class"
    std::string subCategories;    // pipe-separated, e.g. "Fx|Delay"
    std::string sdkVersion;       // e.g. "VST 3.8.0"
    std::string classId;          // 32-char hex string representation of the TUID
    int32_t cardinality = 0;      // class cardinality (typically 0x7FFFFFFF)
    // Factory info
    std::string factoryVendor;
    std::string factoryUrl;
    std::string factoryEmail;
};

// Recursively scan a directory for .vst3 modules; returns metadata for every
// class in every module found. Errors per-module are logged and skipped.
std::vector<PluginClassInfo> scanDirectory(const std::string& dir);

// Inspect a single .vst3 module; returns one or more PluginClassInfo entries.
std::vector<PluginClassInfo> inspectPlugin(const std::string& path);

// Return the platform-specific default VST3 plugin search paths.
std::vector<std::string> defaultPluginPaths();

} // namespace nst3
