//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// PluginInstance — napi ObjectWrap that owns a live VST3 plugin
// (component + audio processor + edit controller) and exposes all
// processing, parameter, MIDI, and state methods to JS.
//-----------------------------------------------------------------------------
#pragma once

#include <napi.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "host.h"
#include "host_application.h"
#include "component_handler.h"
#include "buffer_stream.h"
#include "midi.h"

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/connectionproxy.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/eventlist.h"

#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

namespace nst3 {

// One audio bus as far as JS sees it: a sequence of channel Float32Arrays.
// Used at process() time to map user-supplied arrays to per-bus AudioBusBuffers.
struct AudioBusChannelPointers {
    std::vector<float*> channelPtrs;
};

// Info for each declared audio bus, captured at load time.
struct AudioBusInfoEntry {
    int32_t channelCount = 0;
    Steinberg::Vst::SpeakerArrangement arrangement = 0;
    bool isActive = false;
};

// Info returned by getInfo()
struct PluginInstanceInfo {
    std::string name;
    std::string vendor;
    std::string version;
    std::string category;
    std::string subCategories;
    std::string sdkVersion;
    std::string classId;
    int32_t numAudioInputs = 0;
    int32_t numAudioOutputs = 0;
    int32_t numMidiInputs = 0;
    int32_t numMidiOutputs = 0;
    int32_t parameterCount = 0;
    bool hasController = false;
    bool isSingleComponent = false;
};

// PluginInstance is a napi ObjectWrap that owns one live VST3 plugin instance.
class PluginInstance : public Napi::ObjectWrap<PluginInstance>,
                       public IPerformEditSink {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    // Factory method: load a plugin from a path and return a JS wrapper.
    static Napi::Value Create(Napi::Env env, const std::string& path,
                              const HostOptions& opts, NstHostApplication* hostApp);

    PluginInstance(const Napi::CallbackInfo& info);
    ~PluginInstance() override;

    //--- IPerformEditSink (called by ComponentHandler on performEdit) ---
    void onPerformEdit(Steinberg::Vst::ParamID id,
                       Steinberg::Vst::ParamValue valueNormalized) override;

    //--- Lifecycle ------------------------------------------------------
    Napi::Value Dispose(const Napi::CallbackInfo& info);
    void Finalize(Napi::Env env) override;

    //--- Info / options -------------------------------------------------
    Napi::Value GetInfo(const Napi::CallbackInfo& info);
    Napi::Value GetLatency(const Napi::CallbackInfo& info);

    //--- Audio processing -----------------------------------------------
    Napi::Value SetActive(const Napi::CallbackInfo& info);
    Napi::Value SetProcessing(const Napi::CallbackInfo& info);
    Napi::Value Process(const Napi::CallbackInfo& info);

    //--- Parameters -----------------------------------------------------
    Napi::Value GetParameterCount(const Napi::CallbackInfo& info);
    Napi::Value GetParameterInfo(const Napi::CallbackInfo& info);
    Napi::Value GetParameter(const Napi::CallbackInfo& info);
    Napi::Value SetParameter(const Napi::CallbackInfo& info);
    Napi::Value SetParameters(const Napi::CallbackInfo& info);
    Napi::Value FormatParameter(const Napi::CallbackInfo& info);

    //--- MIDI / events --------------------------------------------------
    Napi::Value AddMidiEvent(const Napi::CallbackInfo& info);
    Napi::Value AddMidiBytes(const Napi::CallbackInfo& info);
    Napi::Value TakeOutputEvents(const Napi::CallbackInfo& info);
    Napi::Value ClearEvents(const Napi::CallbackInfo& info);

    //--- State ----------------------------------------------------------
    Napi::Value SaveState(const Napi::CallbackInfo& info);
    Napi::Value LoadState(const Napi::CallbackInfo& info);

    //--- Events ---------------------------------------------------------
    Napi::Value On(const Napi::CallbackInfo& info);

private:
    // Internal setup (called by Create). Returns false on failure.
    bool setup(const std::string& path, const HostOptions& opts, NstHostApplication* hostApp);

    // Tears down all plugin resources (idempotent).
    void teardown();

    // Throws if disposed or faulted.
    void checkAlive() const;

    // Forward restart callback to the JS side via TSFN.
    void emitRestart(int32_t flags);

    // Resolve per-bus AudioBusBuffers from a JS `inputs`/`outputs` value.
    // `value` may be either:
    //   - A flat array of Float32Array channels (single-bus backward compat),
    //   - An array of arrays of Float32Array channels (multi-bus).
    // `outPtrs` is filled with the channel pointer vectors per bus.
    // Returns false (and throws) if buffer lengths are inconsistent.
    bool resolveAudioBuses(Napi::Env env, Napi::Value value,
                           const std::vector<AudioBusInfoEntry>& busInfos,
                           std::vector<std::vector<float*>>& outPtrs,
                           int32_t numSamples, const char* dirName);

    // Try to map a MIDI controller (CC/PC/CP/PB) to a plugin parameter via
    // IMidiMapping. On success, sets the parameter on the controller and queues
    // the change for the next process() call. Returns true if mapped.
    bool tryMapMidiController(int16_t channel,
                              Steinberg::Vst::CtrlNumber ctrlNumber,
                              double normalizedValue,
                              Steinberg::Vst::ParamID* outParamId = nullptr);

    // Negotiate speaker arrangements: query each bus's current arrangement and
    // try to lock in stereo (or mono as fallback) for stereo plugins.
    void negotiateSpeakerArrangements();

    //--- Owned state ----------------------------------------------------
    VST3::Hosting::Module::Ptr module_;
    NstHostApplication* hostApp_ = nullptr; // not owned (Host owns it)
    std::unique_ptr<ComponentHandler> handler_;

    Steinberg::IPtr<Steinberg::Vst::IComponent> component_;
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> audioProcessor_;
    Steinberg::IPtr<Steinberg::Vst::IEditController> controller_;
    Steinberg::IPtr<Steinberg::Vst::IMidiMapping> midiMapping_;
    Steinberg::IPtr<Steinberg::Vst::ConnectionProxy> componentCP_;
    Steinberg::IPtr<Steinberg::Vst::ConnectionProxy> controllerCP_;

    // Class info captured at load time (for getInfo)
    PluginInstanceInfo info_;
    HostOptions opts_;
    bool active_ = false;
    bool processing_ = false;
    bool faulted_ = false;
    std::atomic<bool> disposed_{false};

    // Per-bus info, captured at load time. Indexed by bus direction+index.
    std::vector<AudioBusInfoEntry> inputBusInfos_;
    std::vector<AudioBusInfoEntry> outputBusInfos_;

    // Process data (reused across calls — zero allocation steady state)
    Steinberg::Vst::ProcessData processData_;
    // Per-bus AudioBusBuffers; one entry per declared input/output bus.
    std::vector<Steinberg::Vst::AudioBusBuffers> inputBuffers_;
    std::vector<Steinberg::Vst::AudioBusBuffers> outputBuffers_;
    // Per-bus channel pointer storage (channelBuffers32 arrays). Indexed by bus.
    std::vector<std::vector<float*>> inputChannelPtrsPerBus_;
    std::vector<std::vector<float*>> outputChannelPtrsPerBus_;

    // ProcessContext reused across calls; sample position advances per block.
    Steinberg::Vst::ProcessContext processContext_;

    // Parameter changes (input + output)
    Steinberg::Vst::ParameterChanges inputParams_;
    Steinberg::Vst::ParameterChanges outputParams_;

    // Event lists (input + output)
    Steinberg::Vst::EventList inputEvents_;
    Steinberg::Vst::EventList outputEvents_;

    // Held SysEx payloads (the VST3 Event points to caller-owned memory;
    // we must keep the buffers alive until the next process call clears them).
    std::vector<std::vector<uint8_t>> sysexHeld_;

    // TSFN for emitting 'restart' events to JS listeners.
    Napi::ThreadSafeFunction restartTsfn_;
    std::atomic<bool> restartTsfnValid_{false};
};

} // namespace nst3
