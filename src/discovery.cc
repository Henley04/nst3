//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Plugin discovery — scans directories for .vst3 modules and reads metadata
// without instantiating DSP components.
//-----------------------------------------------------------------------------
#include "discovery.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "errors.h"
#include "string_convert.h"

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/utility/uid.h"

namespace fs = std::filesystem;

namespace nst3 {

namespace {

// Convert a UID (16-byte array) to a 32-char lowercase hex string.
std::string uidToHex(const Steinberg::TUID& uid) {
    static const char* hex = "0123456789abcdef";
    std::string out(32, '0');
    for (size_t i = 0; i < 16; ++i) {
        out[i * 2]     = hex[(uid[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[uid[i] & 0xF];
    }
    return out;
}

// Read class info from a loaded Module; returns one entry per class.
std::vector<PluginClassInfo> collectClassInfos(const VST3::Hosting::Module::Ptr& module) {
    std::vector<PluginClassInfo> result;
    if (!module) return result;
    const auto& factory = module->getFactory();
    const auto factoryInfo = factory.info();
    for (const auto& ci : factory.classInfos()) {
        PluginClassInfo info;
        info.path = module->getPath();
        info.name = ci.name();
        info.vendor = ci.vendor();
        info.version = ci.version();
        info.category = ci.category();
        info.subCategories = ci.subCategoriesString();
        info.sdkVersion = ci.sdkVersion();
        info.cardinality = ci.cardinality();
        // UID is exposed via the SDK's UID struct; convert to TUID bytes.
        const auto& uid = ci.ID();
        Steinberg::TUID tuid;
        std::memcpy(tuid, uid.data(), 16);
        info.classId = uidToHex(tuid);
        info.factoryVendor = factoryInfo.vendor();
        info.factoryUrl = factoryInfo.url();
        info.factoryEmail = factoryInfo.email();
        result.push_back(std::move(info));
    }
    return result;
}

} // namespace

std::vector<PluginClassInfo> scanDirectory(const std::string& dir) {
    std::vector<PluginClassInfo> result;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return result;

    // Use a set to dedupe (in case of symlinks / overlapping mounts).
    std::set<fs::path> seen;
    for (auto& entry : fs::recursive_directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) { ec.clear(); continue; }
        const auto& path = entry.path();
        if (path.extension() != ".vst3") continue;
        auto canonical = fs::weakly_canonical(path, ec);
        if (ec) { ec.clear(); continue; }
        if (!seen.insert(canonical).second) continue;

        std::string errDesc;
        auto module = VST3::Hosting::Module::create(canonical.string(), errDesc);
        if (!module) continue; // skip unloadable modules silently
        auto infos = collectClassInfos(module);
        for (auto& info : infos) result.push_back(std::move(info));
    }
    return result;
}

std::vector<PluginClassInfo> inspectPlugin(const std::string& path) {
    std::string errDesc;
    auto module = VST3::Hosting::Module::create(path, errDesc);
    if (!module) {
        throwNst(ErrorCode::LoadFailed,
                 "Failed to load VST3 module: " + path + " (" + errDesc + ")");
    }
    return collectClassInfos(module);
}

std::vector<std::string> defaultPluginPaths() {
    std::vector<std::string> paths;
#if defined(_WIN32)
    const char* commonFiles = std::getenv("COMMONPROGRAMFILES");
    if (commonFiles) paths.emplace_back(std::string(commonFiles) + "\\VST3");
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData) {
        paths.emplace_back(std::string(localAppData) + "\\Programs\\Common\\VST3");
    }
    const char* programFiles = std::getenv("PROGRAMFILES");
    if (programFiles) {
        paths.emplace_back(std::string(programFiles) + "\\Common Files\\VST3");
    }
#elif defined(__APPLE__)
    paths.emplace_back("/Library/Audio/Plug-Ins/VST3");
    const char* home = std::getenv("HOME");
    if (home) paths.emplace_back(std::string(home) + "/Library/Audio/Plug-Ins/VST3");
#else
    paths.emplace_back("/usr/lib/vst3");
    paths.emplace_back("/usr/local/lib/vst3");
    const char* home = std::getenv("HOME");
    if (home) paths.emplace_back(std::string(home) + "/.vst3");
#endif
    return paths;
}

} // namespace nst3
