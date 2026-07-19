//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Host — napi ObjectWrap that owns the host context and loads plugins.
//-----------------------------------------------------------------------------
#pragma once

#include <napi.h>

#include <memory>
#include <string>

#include "host_application.h"
#include "component_handler.h"

namespace nst3 {

// HostOptions mirrors the JS HostOptions type.
struct HostOptions {
    double sampleRate = 48000.0;
    int32_t maxBlockSize = 512;
    int32_t audioInputs = 2;
    int32_t audioOutputs = 2;
    // Sample size: 32 (kSample32, default) or 64 (kSample64). If the user
    // requests 64 but the plugin refuses via canProcessSampleSize, the host
    // silently falls back to 32.
    int32_t sampleSize = 32;
    // Process mode: 0 = realtime (default), 1 = offline, 2 = prefetch.
    // Matches Steinberg::Vst::ProcessMode.
    int32_t processMode = 0;
};

// Host is the top-level JS class. It owns the NstHostApplication and provides
// plugin discovery + loading methods.
class Host : public Napi::ObjectWrap<Host> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    Host(const Napi::CallbackInfo& info);
    ~Host() override;

    // Static discovery methods (do not require an instance).
    static Napi::Value ScanDefaultLocations(const Napi::CallbackInfo& info);
    static Napi::Value ScanDirectory(const Napi::CallbackInfo& info);
    static Napi::Value InspectPlugin(const Napi::CallbackInfo& info);

    // Instance methods
    Napi::Value Load(const Napi::CallbackInfo& info);
    Napi::Value GetOptions(const Napi::CallbackInfo& info);

    // Accessors for PluginInstance to use during load.
    NstHostApplication* hostApplication() const { return hostApp_.get(); }
    const HostOptions& options() const { return options_; }

private:
    HostOptions options_;
    std::unique_ptr<NstHostApplication> hostApp_;
};

// Helper: convert a PluginClassInfo C++ struct to a Napi::Object.
Napi::Object pluginInfoToObject(Napi::Env env, const struct PluginClassInfo& info);

} // namespace nst3
