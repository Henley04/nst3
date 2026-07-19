//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// PluginInstance — napi ObjectWrap that owns a live VST3 plugin
// (component + audio processor + edit controller) and exposes all
// processing, parameter, MIDI, and state methods to JS.
//-----------------------------------------------------------------------------
#pragma once

#include <napi.h>

#include <atomic>
#include <map>
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
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/ivstnoteexpression.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstinfoattributes.h"

namespace nst3 {

// One audio bus as far as JS sees it: a sequence of channel Float32Arrays
// (kSample32) or Float64Arrays (kSample64). Used at process() time to map
// user-supplied arrays to per-bus AudioBusBuffers. We store channel pointers
// as void* so the same vector holds either float* or double* (binary-compatible
// on all supported platforms).
struct AudioBusChannelPointers {
    std::vector<void*> channelPtrs;
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

    //--- Sample size / process mode queries -----------------------------
    Napi::Value GetSampleSize(const Napi::CallbackInfo& info);
    Napi::Value CanProcessSampleSize(const Napi::CallbackInfo& info);
    Napi::Value GetTailSamples(const Napi::CallbackInfo& info);

    //--- Parameters -----------------------------------------------------
    Napi::Value GetParameterCount(const Napi::CallbackInfo& info);
    Napi::Value GetParameterInfo(const Napi::CallbackInfo& info);
    Napi::Value GetParameter(const Napi::CallbackInfo& info);
    Napi::Value SetParameter(const Napi::CallbackInfo& info);
    Napi::Value SetParameters(const Napi::CallbackInfo& info);
    Napi::Value FormatParameter(const Napi::CallbackInfo& info);
    Napi::Value ParseParameter(const Napi::CallbackInfo& info);
    Napi::Value PlainToNormalized(const Napi::CallbackInfo& info);
    Napi::Value NormalizedToPlain(const Napi::CallbackInfo& info);

    //--- MIDI / events --------------------------------------------------
    Napi::Value AddMidiEvent(const Napi::CallbackInfo& info);
    Napi::Value AddMidiBytes(const Napi::CallbackInfo& info);
    Napi::Value TakeOutputEvents(const Napi::CallbackInfo& info);
    Napi::Value ClearEvents(const Napi::CallbackInfo& info);

    //--- State ----------------------------------------------------------
    Napi::Value SaveState(const Napi::CallbackInfo& info);
    Napi::Value LoadState(const Napi::CallbackInfo& info);

    //--- IUnitInfo (units + programs) -----------------------------------
    Napi::Value GetUnitCount(const Napi::CallbackInfo& info);
    Napi::Value GetUnitInfo(const Napi::CallbackInfo& info);
    Napi::Value GetProgramListCount(const Napi::CallbackInfo& info);
    Napi::Value GetProgramListInfo(const Napi::CallbackInfo& info);
    Napi::Value GetProgramName(const Napi::CallbackInfo& info);
    Napi::Value SelectProgram(const Napi::CallbackInfo& info);
    Napi::Value GetCurrentUnit(const Napi::CallbackInfo& info);
    Napi::Value GetUnitByBusInfo(const Napi::CallbackInfo& info);

    //--- IProgramListData / IUnitData ----------------------------------
    Napi::Value GetProgramData(const Napi::CallbackInfo& info);
    Napi::Value SetProgramData(const Napi::CallbackInfo& info);
    Napi::Value GetUnitData(const Napi::CallbackInfo& info);
    Napi::Value SetUnitData(const Napi::CallbackInfo& info);

    //--- INoteExpressionController -------------------------------------
    Napi::Value GetNoteExpressionCount(const Napi::CallbackInfo& info);
    Napi::Value GetNoteExpressionInfo(const Napi::CallbackInfo& info);
    Napi::Value AddNoteExpressionEvent(const Napi::CallbackInfo& info);

    //--- IKeyswitchController ------------------------------------------
    Napi::Value GetKeyswitchCount(const Napi::CallbackInfo& info);
    Napi::Value GetKeyswitchInfo(const Napi::CallbackInfo& info);

    //--- Runtime bus management ----------------------------------------
    Napi::Value GetBusList(const Napi::CallbackInfo& info);
    Napi::Value GetBusInfo(const Napi::CallbackInfo& info);
    Napi::Value ActivateBus(const Napi::CallbackInfo& info);

    //--- Speaker arrangement -------------------------------------------
    Napi::Value SetBusArrangement(const Napi::CallbackInfo& info);
    Napi::Value GetBusArrangement(const Napi::CallbackInfo& info);

    //--- Routing info --------------------------------------------------
    Napi::Value GetRoutingInfo(const Napi::CallbackInfo& info);

    //--- Process context ----------------------------------------------
    // Allow JS users to drive tempo, time signature, transport state, etc.
    Napi::Value SetProcessContext(const Napi::CallbackInfo& info);
    Napi::Value GetProcessContext(const Napi::CallbackInfo& info);

    //--- IProcessContextRequirements ----------------------------------
    Napi::Value GetProcessContextRequirements(const Napi::CallbackInfo& info);

    //--- IAudioPresentationLatencySamples ----------------------------
    Napi::Value SetAudioPresentationLatency(const Napi::CallbackInfo& info);

    //--- IInfoListener -------------------------------------------------
    Napi::Value SetChannelContextInfo(const Napi::CallbackInfo& info);

    //--- IPrefetchableSupport -----------------------------------------
    Napi::Value IsPrefetchable(const Napi::CallbackInfo& info);

    //--- IEditController2 (host→plugin) ------------------------------
    // setKnobMode forwards to IEditController2::setKnobMode when the plugin's
    // controller implements IEditController2. Returns false (no-op) otherwise.
    Napi::Value SetKnobMode(const Napi::CallbackInfo& info);

    //--- Restart auto-react -------------------------------------------
    // ApplyRestartFlags re-queries affected SDK state for the given restart
    // flag bitmask. Called automatically by ComponentHandler::restartComponent
    // BEFORE the JS 'restart' event fires, so users handling the event see
    // up-to-date cached state. Also exposed as the JS method
    // applyRestartFlags(flags) for cases where the user wants to manually
    // trigger a re-query (e.g. after editing plugin state directly).
    Napi::Value ApplyRestartFlagsJs(const Napi::CallbackInfo& info);
    // Internal: performs the actual re-query work for the given flags. Safe
    // to call from any thread that can call SDK methods (typically the JS /
    // controller thread). Returns void.
    void ApplyRestartFlags(int32_t flags);

    //--- Mutable ProcessSetup ----------------------------------------
    // SetProcessSetup updates the stored ProcessSetup fields (sampleRate,
    // maxBlockSize, processMode, sampleSize) for the next setActive(true).
    // Throws VST3_INVALID_PARAMETER if the plugin is currently active.
    Napi::Value SetProcessSetup(const Napi::CallbackInfo& info);

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

    // Forward a plugin→host event (dirty, beginGesture, endGesture,
    // startGroup, finishGroup) to the JS side via the host-event TSFN. The
    // TSFN dispatches to the right listener (registered via on('dirty', cb)
    // etc.) on the JS thread.
    void emitHostEvent(const HostEvent& ev);

    // Resolve per-bus AudioBusBuffers from a JS `inputs`/`outputs` value.
    // `value` may be either:
    //   - A flat array of Float32Array/Float64Array channels (single-bus backward compat),
    //   - An array of arrays of Float32Array/Float64Array channels (multi-bus).
    // The accepted TypedArrayType depends on `activeSampleSize_`: Float32Array
    // for kSample32, Float64Array for kSample64.
    // `outPtrs` is filled with the channel pointer vectors per bus (each
    // element is a float* or double* depending on the active sample size).
    // Returns false (and throws) if buffer lengths are inconsistent.
    bool resolveAudioBuses(Napi::Env env, Napi::Value value,
                           const std::vector<AudioBusInfoEntry>& busInfos,
                           std::vector<std::vector<void*>>& outPtrs,
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

    // Optional controller-side interfaces queried at setup() time. Any of
    // these may be null when the plugin does not implement the corresponding
    // interface; the JS-facing methods return sensible defaults (0 count,
    // null) in that case, or throw VST3_UNKNOWN when the user explicitly
    // invokes a method that requires the interface.
    Steinberg::IPtr<Steinberg::Vst::IUnitInfo> unitInfo_;
    Steinberg::IPtr<Steinberg::Vst::IProgramListData> programListData_;
    Steinberg::IPtr<Steinberg::Vst::IUnitData> unitData_;
    Steinberg::IPtr<Steinberg::Vst::INoteExpressionController> noteExpr_;
    Steinberg::IPtr<Steinberg::Vst::IKeyswitchController> keyswitchCtrl_;
    // Optional audio-processor-side interfaces queried at setup() time. Used
    // to drive the IProcessContextRequirements gating, audio presentation
    // latency notifications, and prefetchable query.
    Steinberg::IPtr<Steinberg::Vst::IProcessContextRequirements> processContextReqs_;
    Steinberg::IPtr<Steinberg::Vst::IAudioPresentationLatencySamples> audioPresLatency_;
    Steinberg::IPtr<Steinberg::Vst::IPrefetchableSupport> prefetchable_;
    // Optional controller-side info listener (channel context info).
    Steinberg::IPtr<Steinberg::Vst::IInfoListener> infoListener_;
    // Optional IEditController2 (host→plugin knob mode + openHelp/openAbout).
    // Only setComponentHandler→setDirty is plugin→host (handled by
    // ComponentHandler). setKnobMode / openHelp / openAboutBox are host→plugin
    // method calls on the plugin's controller.
    Steinberg::IPtr<Steinberg::Vst::IEditController2> editController2_;

    // Class info captured at load time (for getInfo)
    PluginInstanceInfo info_;
    HostOptions opts_;
    bool active_ = false;
    bool processing_ = false;
    bool faulted_ = false;
    std::atomic<bool> disposed_{false};

    // Sample-size capability probes (captured at setup() time). Defaults to
    // true so a plugin that fails to answer canProcessSampleSize is still
    // processable at kSample32 (the VST3 spec mandates 32-bit support).
    bool can32_ = true;
    bool can64_ = false;
    // The active sample size for the current setActive(true) session. Stored
    // as a 32/64 integer for JS exposure. Remains 32 until setActive(true).
    int32_t activeSampleSize_ = 32;
    // The stored VST3 process mode (kRealtime / kOffline / kPrefetch).
    // Defaults to kRealtime. Stored as int32_t to avoid exposing the SDK enum
    // in this header; the SDK enum is constructed in setup()/SetActive().
    int32_t processMode_ = 0; // 0=realtime, 1=offline, 2=prefetch

    // Per-bus info, captured at load time. Indexed by bus direction+index.
    std::vector<AudioBusInfoEntry> inputBusInfos_;
    std::vector<AudioBusInfoEntry> outputBusInfos_;

    // Process data (reused across calls — zero allocation steady state)
    Steinberg::Vst::ProcessData processData_;
    // Per-bus AudioBusBuffers; one entry per declared input/output bus.
    std::vector<Steinberg::Vst::AudioBusBuffers> inputBuffers_;
    std::vector<Steinberg::Vst::AudioBusBuffers> outputBuffers_;
    // Per-bus channel pointer storage. Each element is a float* (kSample32)
    // or double* (kSample64); stored as void* to support both without two
    // parallel vectors. Indexed by bus.
    std::vector<std::vector<void*>> inputChannelPtrsPerBus_;
    std::vector<std::vector<void*>> outputChannelPtrsPerBus_;

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

    // TSFN for emitting plugin→host events ('dirty', 'beginGesture',
    // 'endGesture', 'startGroup', 'finishGroup') to JS listeners. A single
    // TSFN carries a heap-allocated HostEvent and dispatches to the right
    // listener (stored in hostEventListeners_) on the JS thread. This mirrors
    // the restartTsfn_ pattern: the TSFN lives in PluginInstance and
    // ComponentHandler only holds a HostEventCallback.
    Napi::ThreadSafeFunction hostEventTsfn_;
    std::atomic<bool> hostEventTsfnValid_{false};
    // JS listeners keyed by event name. Populated by On() and read by the
    // host-event TSFN callback on the JS thread.
    std::map<std::string, Napi::FunctionReference> hostEventListeners_;
};

} // namespace nst3
