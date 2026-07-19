//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Plugin discovery — scans directories for .vst3 modules and reads metadata
// without instantiating DSP components.
//-----------------------------------------------------------------------------
#include "discovery.h"

#include <fstream>
#include <set>
#include <sstream>

#include "errors.h"
#include "string_convert.h"

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/utility/uid.h"

// macOS: std::filesystem requires MACOSX_DEPLOYMENT_TARGET >= 10.15, but the
// project intentionally targets 10.13 (High Sierra) for backwards compat.
// Fall back to POSIX dirent + stat for directory walking on Apple platforms.
#if defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

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

bool endsWithVst3(const std::string& s) {
    static const std::string ext = ".vst3";
    return s.size() >= ext.size() &&
           s.compare(s.size() - ext.size(), ext.size(), ext) == 0;
}

#if defined(__APPLE__)
// POSIX recursive walk used on macOS to avoid std::filesystem's 10.15
// requirement. Dedupes via canonical paths (realpath). Skips unreadable
// entries silently, mirroring skip_permission_denied best-effort behavior.
void walkDirPosix(const std::string& dir, std::set<std::string>& seen,
                  std::vector<std::string>& out) {
    DIR* dh = opendir(dir.c_str());
    if (!dh) return;
    struct dirent* entry;
    while ((entry = readdir(dh)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string full = dir;
        if (full.empty() || full.back() != '/') full.push_back('/');
        full += name;

        struct stat st;
        if (lstat(full.c_str(), &st) != 0) continue;

        // Collect .vst3 entries regardless of type: on macOS a .vst3 is a
        // bundle *directory* (Contents/MacOS/...), on Linux it may be a
        // directory or a symlink. Do not recurse into bundles.
        if (endsWithVst3(name)) {
            char resolved[PATH_MAX];
            if (!realpath(full.c_str(), resolved)) {
                if (seen.insert(full).second) out.push_back(full);
                continue;
            }
            if (seen.insert(resolved).second) out.push_back(resolved);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            walkDirPosix(full, seen, out);
            continue;
        }
    }
    closedir(dh);
}
#endif

} // namespace

std::vector<PluginClassInfo> scanDirectory(const std::string& dir) {
    std::vector<PluginClassInfo> result;

#if defined(__APPLE__)
    // macOS: POSIX walk (std::filesystem requires macOS 10.15+).
    struct stat dirSt;
    if (stat(dir.c_str(), &dirSt) != 0 || !S_ISDIR(dirSt.st_mode)) return result;
    std::set<std::string> seen;
    std::vector<std::string> candidates;
    walkDirPosix(dir, seen, candidates);
    for (const auto& path : candidates) {
        std::string errDesc;
        auto module = VST3::Hosting::Module::create(path, errDesc);
        if (!module) continue; // skip unloadable modules silently
        auto infos = collectClassInfos(module);
        for (auto& info : infos) result.push_back(std::move(info));
    }
#else
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
#endif
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
