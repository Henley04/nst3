//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// PluginInstance implementation
//-----------------------------------------------------------------------------
#include "plugin_instance.h"

#include <cstring>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <sstream>

#include "errors.h"
#include "string_convert.h"

#include "public.sdk/source/vst/utility/uid.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"

#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/ivstnoteexpression.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/funknownimpl.h"

namespace nst3 {

Napi::FunctionReference PluginInstance::constructor;

//------------------------------------------------------------------------
// Init / factory
//------------------------------------------------------------------------
Napi::Object PluginInstance::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "PluginInstance", {
        InstanceMethod("dispose", &PluginInstance::Dispose),
        InstanceMethod("getInfo", &PluginInstance::GetInfo),
        InstanceMethod("getLatency", &PluginInstance::GetLatency),
        InstanceMethod("setActive", &PluginInstance::SetActive),
        InstanceMethod("setProcessing", &PluginInstance::SetProcessing),
        InstanceMethod("process", &PluginInstance::Process),
        InstanceMethod("getSampleSize", &PluginInstance::GetSampleSize),
        InstanceMethod("canProcessSampleSize", &PluginInstance::CanProcessSampleSize),
        InstanceMethod("getTailSamples", &PluginInstance::GetTailSamples),
        InstanceMethod("getParameterCount", &PluginInstance::GetParameterCount),
        InstanceMethod("getParameterInfo", &PluginInstance::GetParameterInfo),
        InstanceMethod("getParameter", &PluginInstance::GetParameter),
        InstanceMethod("setParameter", &PluginInstance::SetParameter),
        InstanceMethod("setParameters", &PluginInstance::SetParameters),
        InstanceMethod("formatParameter", &PluginInstance::FormatParameter),
        InstanceMethod("parseParameter", &PluginInstance::ParseParameter),
        InstanceMethod("plainToNormalized", &PluginInstance::PlainToNormalized),
        InstanceMethod("normalizedToPlain", &PluginInstance::NormalizedToPlain),
        InstanceMethod("addMidiEvent", &PluginInstance::AddMidiEvent),
        InstanceMethod("addMidiBytes", &PluginInstance::AddMidiBytes),
        InstanceMethod("takeOutputEvents", &PluginInstance::TakeOutputEvents),
        InstanceMethod("clearEvents", &PluginInstance::ClearEvents),
        InstanceMethod("saveState", &PluginInstance::SaveState),
        InstanceMethod("loadState", &PluginInstance::LoadState),
        //--- IUnitInfo (units + programs) --------------------------------
        InstanceMethod("getUnitCount", &PluginInstance::GetUnitCount),
        InstanceMethod("getUnitInfo", &PluginInstance::GetUnitInfo),
        InstanceMethod("getProgramListCount", &PluginInstance::GetProgramListCount),
        InstanceMethod("getProgramListInfo", &PluginInstance::GetProgramListInfo),
        InstanceMethod("getProgramName", &PluginInstance::GetProgramName),
        InstanceMethod("selectProgram", &PluginInstance::SelectProgram),
        InstanceMethod("getCurrentUnit", &PluginInstance::GetCurrentUnit),
        InstanceMethod("getUnitByBusInfo", &PluginInstance::GetUnitByBusInfo),
        //--- IProgramListData / IUnitData -------------------------------
        InstanceMethod("getProgramData", &PluginInstance::GetProgramData),
        InstanceMethod("setProgramData", &PluginInstance::SetProgramData),
        InstanceMethod("getUnitData", &PluginInstance::GetUnitData),
        InstanceMethod("setUnitData", &PluginInstance::SetUnitData),
        //--- INoteExpressionController ----------------------------------
        InstanceMethod("getNoteExpressionCount", &PluginInstance::GetNoteExpressionCount),
        InstanceMethod("getNoteExpressionInfo", &PluginInstance::GetNoteExpressionInfo),
        InstanceMethod("addNoteExpressionEvent", &PluginInstance::AddNoteExpressionEvent),
        //--- IKeyswitchController ---------------------------------------
        InstanceMethod("getKeyswitchCount", &PluginInstance::GetKeyswitchCount),
        InstanceMethod("getKeyswitchInfo", &PluginInstance::GetKeyswitchInfo),
        //--- Runtime bus management -------------------------------------
        InstanceMethod("getBusList", &PluginInstance::GetBusList),
        InstanceMethod("getBusInfo", &PluginInstance::GetBusInfo),
        InstanceMethod("activateBus", &PluginInstance::ActivateBus),
        //--- Speaker arrangement ----------------------------------------
        InstanceMethod("setBusArrangement", &PluginInstance::SetBusArrangement),
        InstanceMethod("getBusArrangement", &PluginInstance::GetBusArrangement),
        //--- Routing info -----------------------------------------------
        InstanceMethod("getRoutingInfo", &PluginInstance::GetRoutingInfo),
        //--- Process context -------------------------------------------
        InstanceMethod("setProcessContext", &PluginInstance::SetProcessContext),
        InstanceMethod("getProcessContext", &PluginInstance::GetProcessContext),
        //--- IProcessContextRequirements --------------------------------
        InstanceMethod("getProcessContextRequirements", &PluginInstance::GetProcessContextRequirements),
        //--- IAudioPresentationLatency ---------------------------------
        InstanceMethod("setAudioPresentationLatency", &PluginInstance::SetAudioPresentationLatency),
        //--- IInfoListener ----------------------------------------------
        InstanceMethod("setChannelContextInfo", &PluginInstance::SetChannelContextInfo),
        //--- IPrefetchableSupport --------------------------------------
        InstanceMethod("isPrefetchable", &PluginInstance::IsPrefetchable),
        //--- IEditController2 (host→plugin) ----------------------------
        InstanceMethod("setKnobMode", &PluginInstance::SetKnobMode),
        //--- Restart auto-react ----------------------------------------
        InstanceMethod("applyRestartFlags", &PluginInstance::ApplyRestartFlagsJs),
        //--- Mutable ProcessSetup --------------------------------------
        InstanceMethod("setProcessSetup", &PluginInstance::SetProcessSetup),
        InstanceMethod("on", &PluginInstance::On),
        // Symbol.dispose for `using` syntax
        InstanceMethod(Napi::Symbol::WellKnown(env, "dispose"), &PluginInstance::Dispose),
    });
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("PluginInstance", func);
    return exports;
}

Napi::Value PluginInstance::Create(Napi::Env env, const std::string& path,
                                   const HostOptions& opts, NstHostApplication* hostApp) {
    if (!hostApp) {
        throwNst(ErrorCode::Unknown, "Host application context is null");
    }
    // Load module
    std::string errDesc;
    auto module = VST3::Hosting::Module::create(path, errDesc);
    if (!module) {
        throwNst(ErrorCode::LoadFailed, "Failed to load VST3 module: " + path + " (" + errDesc + ")");
    }

    // Find first Audio Module Class
    const auto& factory = module->getFactory();
    VST3::Hosting::ClassInfo chosen;
    bool found = false;
    for (const auto& ci : factory.classInfos()) {
        if (ci.category() == kVstAudioEffectClass) {
            chosen = ci;
            found = true;
            break;
        }
    }
    if (!found) {
        throwNst(ErrorCode::ComponentCreationFailed,
                 "No VST3 audio effect class found in module: " + path);
    }

    // Construct the JS wrapper via the constructor function.
    Napi::EscapableHandleScope scope(env);
    auto obj = constructor.New({});
    auto* wrap = PluginInstance::Unwrap(obj);
    if (!wrap->setup(path, opts, hostApp)) {
        // setup() throws NstException on failure; this is unreachable
        throwNst(ErrorCode::Unknown, "Plugin setup failed");
    }
    // Save the module reference so the plugin keeps the .vst3 loaded.
    wrap->module_ = module;
    return scope.Escape(obj);
}

PluginInstance::PluginInstance(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<PluginInstance>(info) {}

PluginInstance::~PluginInstance() {
    teardown();
}

void PluginInstance::Finalize(Napi::Env env) {
    teardown();
    if (restartTsfnValid_.exchange(false)) {
        restartTsfn_.Release();
    }
    if (hostEventTsfnValid_.exchange(false)) {
        hostEventTsfn_.Release();
    }
    // Clear listener references (they hold JS functions). Safe now that the
    // TSFN has been released (all pending callbacks have completed).
    hostEventListeners_.clear();
}

//------------------------------------------------------------------------
// setup — performs the full VST3 plugin initialization dance.
//------------------------------------------------------------------------
bool PluginInstance::setup(const std::string& path, const HostOptions& opts,
                            NstHostApplication* hostApp) {
    hostApp_ = hostApp;
    opts_ = opts;

    if (!module_) {
        // When setup() is called from Create(), the module is already loaded
        // and stored on the wrapper before invoking setup. In other paths,
        // reload here.
        std::string errDesc;
        module_ = VST3::Hosting::Module::create(path, errDesc);
        if (!module_) {
            throwNst(ErrorCode::LoadFailed, "Failed to load module: " + errDesc);
        }
    }

    const auto& factory = module_->getFactory();

    // Find first Audio Module Class
    VST3::Hosting::ClassInfo chosen;
    bool found = false;
    for (const auto& ci : factory.classInfos()) {
        if (ci.category() == kVstAudioEffectClass) {
            chosen = ci;
            found = true;
            break;
        }
    }
    if (!found) {
        throwNst(ErrorCode::ComponentCreationFailed, "No audio effect class in module");
    }

    // Create component
    component_ = factory.createInstance<Steinberg::Vst::IComponent>(chosen.ID());
    if (!component_) {
        throwNst(ErrorCode::ComponentCreationFailed, "Failed to create IComponent");
    }

    // Initialize with host context
    if (auto plugBase = Steinberg::U::cast<Steinberg::IPluginBase>(component_)) {
        Steinberg::tresult r = plugBase->initialize(hostApp_);
        if (r != Steinberg::kResultOk && r != Steinberg::kResultTrue) {
            throwNst(ErrorCode::ComponentCreationFailed,
                     "IComponent::initialize failed (tresult=" + std::to_string(static_cast<int>(r)) + ")");
        }
    }

    // Try to get IEditController from the component (single-component effect)
    bool isSingle = false;
    if (component_->queryInterface(Steinberg::Vst::IEditController::iid,
                                   reinterpret_cast<void**>(&controller_)) == Steinberg::kResultTrue) {
        isSingle = true;
        info_.isSingleComponent = true;
    } else {
        // Get controller class ID and create controller via factory
        Steinberg::TUID controllerCID;
        if (component_->getControllerClassId(controllerCID) == Steinberg::kResultTrue) {
            controller_ = factory.createInstance<Steinberg::Vst::IEditController>(VST3::UID(controllerCID));
            if (controller_) {
                if (auto plugCtrlBase = Steinberg::U::cast<Steinberg::IPluginBase>(controller_)) {
                    Steinberg::tresult r = plugCtrlBase->initialize(hostApp_);
                    if (r != Steinberg::kResultOk && r != Steinberg::kResultTrue) {
                        throwNst(ErrorCode::ControllerMissing,
                                 "IEditController::initialize failed");
                    }
                }
            }
        }
    }

    // Connect component <-> controller via IConnectionPoint
    if (controller_ && !isSingle) {
        auto compICP = Steinberg::U::cast<Steinberg::Vst::IConnectionPoint>(component_);
        auto contrICP = Steinberg::U::cast<Steinberg::Vst::IConnectionPoint>(controller_);
        if (compICP && contrICP) {
            componentCP_ = Steinberg::owned(new Steinberg::Vst::ConnectionProxy(compICP));
            controllerCP_ = Steinberg::owned(new Steinberg::Vst::ConnectionProxy(contrICP));
            componentCP_->connect(contrICP);
            controllerCP_->connect(compICP);
        }
    }

    // Query IAudioProcessor
    audioProcessor_ = Steinberg::U::cast<Steinberg::Vst::IAudioProcessor>(component_);
    if (!audioProcessor_) {
        throwNst(ErrorCode::ComponentCreationFailed,
                 "Component does not implement IAudioProcessor");
    }

    // Probe sample-size capabilities. The VST3 spec mandates that every
    // IAudioProcessor support kSample32; kSample64 is optional. If the plugin
    // refuses to answer (returns kResultFalse / kNotImplemented), we conservatively
    // treat the corresponding capability as false. can32_ defaults to true so
    // a non-answering plugin still works at kSample32.
    can32_ = (audioProcessor_->canProcessSampleSize(Steinberg::Vst::kSample32) == Steinberg::kResultTrue);
    can64_ = (audioProcessor_->canProcessSampleSize(Steinberg::Vst::kSample64) == Steinberg::kResultTrue);

    // Query optional audio-processor-side interfaces:
    //   - IProcessContextRequirements: lets us know which ProcessContext
    //     fields the plugin actually consumes, so we can skip recompute work
    //     and always set the requested state bits.
    //   - IAudioPresentationLatency: lets the host inform the plugin
    //     of the output-presentation latency per bus (for monitoring PDC).
    //   - IPrefetchableSupport: lets the host query whether the plugin can
    //     run in a prefetch (look-ahead) rendering mode.
    processContextReqs_ =
        Steinberg::U::cast<Steinberg::Vst::IProcessContextRequirements>(audioProcessor_);
    audioPresLatency_ =
        Steinberg::U::cast<Steinberg::Vst::IAudioPresentationLatency>(audioProcessor_);
    prefetchable_ =
        Steinberg::U::cast<Steinberg::Vst::IPrefetchableSupport>(audioProcessor_);

    // Resolve the active sample size. If the user requested 64 but the plugin
    // refuses (can64_ == false), fall back to 32 (documented behavior).
    processMode_ = opts_.processMode; // 0=realtime, 1=offline, 2=prefetch
    if (opts_.sampleSize == 64 && can64_) {
        activeSampleSize_ = 64;
    } else {
        activeSampleSize_ = 32;
        // Note: silent fallback when 64 was requested but refused. Documented
        // in index.d.ts JSDoc; users can probe via canProcessSampleSize(64).
    }

    // Setup ComponentHandler (so plugin's controller can call back into the host)
    handler_ = std::make_unique<ComponentHandler>();
    handler_->setPluginInstance(this);
    handler_->setPerformEditSink(this);  // route performEdit → inputParams_
    // Wire the apply-restart callback so the host refreshes its cached SDK
    // state (latency, bus info, etc.) BEFORE the JS 'restart' event fires.
    // This is independent of whether the user registers a 'restart' JS
    // listener — the host's internal caches must stay consistent with the
    // plugin's reported state regardless. The JS event is emitted separately
    // via restartCallback_ (wired lazily in On('restart', cb)).
    handler_->setApplyRestartCallback(
        [this](int32_t flags) { this->ApplyRestartFlags(flags); });
    if (controller_) {
        controller_->setComponentHandler(handler_.get());
        hostApp_->setComponentHandler(handler_.get());
        info_.hasController = true;
        // Query optional IMidiMapping for proper MIDI CC/PB/PC routing.
        midiMapping_ = Steinberg::U::cast<Steinberg::Vst::IMidiMapping>(controller_);
        // Query optional IUnitInfo for units/programs enumeration and switching.
        unitInfo_ = Steinberg::U::cast<Steinberg::Vst::IUnitInfo>(controller_);
        // Query optional IProgramListData / IUnitData for bulk-data persistence.
        programListData_ = Steinberg::U::cast<Steinberg::Vst::IProgramListData>(controller_);
        unitData_ = Steinberg::U::cast<Steinberg::Vst::IUnitData>(controller_);
        // Query optional INoteExpressionController for per-note expression events.
        noteExpr_ = Steinberg::U::cast<Steinberg::Vst::INoteExpressionController>(controller_);
        // Query optional IKeyswitchController for static keyswitch enumeration.
        keyswitchCtrl_ = Steinberg::U::cast<Steinberg::Vst::IKeyswitchController>(controller_);
        // Query optional IInfoListener for channel-context-info notifications
        // (host tells the plugin which track / channel it is loaded on).
        infoListener_ = Steinberg::U::cast<Steinberg::Vst::ChannelContext::IInfoListener>(controller_);
        // Query optional IEditController2 for host→plugin knob-mode selection.
        // (openHelp / openAboutBox are also on IEditController2 but are not
        // exposed as JS methods in this version — they relate to GUI windows.)
        editController2_ = Steinberg::U::cast<Steinberg::Vst::IEditController2>(controller_);
    }

    // Read per-bus audio info (per-bus, not just summed channel counts).
    // We capture this once at load time; later speaker-arrangement
    // negotiation may rewrite `arrangement` after setBusArrangements.
    Steinberg::int32 inAudioBuses =
        component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
    Steinberg::int32 outAudioBuses =
        component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);

    inputBusInfos_.clear();
    inputBusInfos_.reserve(static_cast<size_t>(inAudioBuses));
    for (Steinberg::int32 i = 0; i < inAudioBuses; ++i) {
        AudioBusInfoEntry e;
        Steinberg::Vst::BusInfo busInfo;
        if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                    Steinberg::Vst::kInput, i, busInfo)
            == Steinberg::kResultTrue) {
            e.channelCount = busInfo.channelCount;
            e.isActive = (busInfo.flags & Steinberg::Vst::BusInfo::kDefaultActive) != 0;
        }
        inputBusInfos_.push_back(e);
    }
    outputBusInfos_.clear();
    outputBusInfos_.reserve(static_cast<size_t>(outAudioBuses));
    for (Steinberg::int32 i = 0; i < outAudioBuses; ++i) {
        AudioBusInfoEntry e;
        Steinberg::Vst::BusInfo busInfo;
        if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                    Steinberg::Vst::kOutput, i, busInfo)
            == Steinberg::kResultTrue) {
            e.channelCount = busInfo.channelCount;
            e.isActive = (busInfo.flags & Steinberg::Vst::BusInfo::kDefaultActive) != 0;
        }
        outputBusInfos_.push_back(e);
    }

    // MIDI (event) bus counts
    Steinberg::int32 inMidi =
        component_->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
    Steinberg::int32 outMidi =
        component_->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);

    // Activate all default-active audio buses (and at minimum the first
    // input/output so single-bus plugins work even if they omit the flag).
    for (Steinberg::int32 i = 0; i < inAudioBuses; ++i) {
        bool on = inputBusInfos_[static_cast<size_t>(i)].isActive || (i == 0);
        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, i, on);
        inputBusInfos_[static_cast<size_t>(i)].isActive = on;
    }
    for (Steinberg::int32 i = 0; i < outAudioBuses; ++i) {
        bool on = outputBusInfos_[static_cast<size_t>(i)].isActive || (i == 0);
        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, i, on);
        outputBusInfos_[static_cast<size_t>(i)].isActive = on;
    }
    if (inMidi > 0) component_->activateBus(Steinberg::Vst::kEvent, Steinberg::Vst::kInput, 0, true);
    if (outMidi > 0) component_->activateBus(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput, 0, true);

    // Negotiate speaker arrangements: try stereo for all buses, fall back to
    // the plugin's current arrangement. Must run before the first
    // getBusInfo() / process() so the plugin sees consistent state.
    negotiateSpeakerArrangements();

    // Setup ProcessData struct (reused across calls). The processMode and
    // symbolicSampleSize are derived from the user's HostOptions / LoadOptions
    // and the plugin's canProcessSampleSize response.
    std::memset(&processData_, 0, sizeof(processData_));
    processData_.processMode = static_cast<Steinberg::Vst::ProcessModes>(processMode_);
    processData_.symbolicSampleSize = (activeSampleSize_ == 64)
        ? Steinberg::Vst::kSample64 : Steinberg::Vst::kSample32;
    processData_.inputParameterChanges = &inputParams_;
    processData_.inputEvents = &inputEvents_;
    processData_.outputParameterChanges = &outputParams_;
    processData_.outputEvents = &outputEvents_;

    // Allocate per-bus AudioBusBuffers and per-bus channel pointer vectors.
    // Sizes match the declared bus counts; this is zero-alloc on the
    // steady-state process() path because we reuse these members.
    inputBuffers_.assign(static_cast<size_t>(inAudioBuses), Steinberg::Vst::AudioBusBuffers{});
    outputBuffers_.assign(static_cast<size_t>(outAudioBuses), Steinberg::Vst::AudioBusBuffers{});
    inputChannelPtrsPerBus_.assign(static_cast<size_t>(inAudioBuses), {});
    outputChannelPtrsPerBus_.assign(static_cast<size_t>(outAudioBuses), {});
    // Helper: assign the appropriate union member (channelBuffers32 or
    // channelBuffers64) based on the active sample size. Both fields overlap
    // in the AudioBusBuffers union; we set the typed one for clarity.
    auto assignChannelBuffers = [this](Steinberg::Vst::AudioBusBuffers& buf, void** ptrs) {
        if (activeSampleSize_ == 64) {
            buf.channelBuffers64 = reinterpret_cast<double**>(ptrs);
        } else {
            buf.channelBuffers32 = reinterpret_cast<float**>(ptrs);
        }
    };
    for (Steinberg::int32 i = 0; i < inAudioBuses; ++i) {
        auto& bufs = inputChannelPtrsPerBus_[static_cast<size_t>(i)];
        bufs.assign(static_cast<size_t>(inputBusInfos_[static_cast<size_t>(i)].channelCount), nullptr);
        inputBuffers_[static_cast<size_t>(i)].numChannels = inputBusInfos_[static_cast<size_t>(i)].channelCount;
        inputBuffers_[static_cast<size_t>(i)].silenceFlags = 0;
        assignChannelBuffers(inputBuffers_[static_cast<size_t>(i)], bufs.data());
    }
    for (Steinberg::int32 i = 0; i < outAudioBuses; ++i) {
        auto& bufs = outputChannelPtrsPerBus_[static_cast<size_t>(i)];
        bufs.assign(static_cast<size_t>(outputBusInfos_[static_cast<size_t>(i)].channelCount), nullptr);
        outputBuffers_[static_cast<size_t>(i)].numChannels = outputBusInfos_[static_cast<size_t>(i)].channelCount;
        outputBuffers_[static_cast<size_t>(i)].silenceFlags = 0;
        assignChannelBuffers(outputBuffers_[static_cast<size_t>(i)], bufs.data());
    }
    processData_.numInputs = inAudioBuses;
    processData_.numOutputs = outAudioBuses;
    processData_.inputs = inputBuffers_.data();
    processData_.outputs = outputBuffers_.data();

    // Initialize ProcessContext with sensible defaults. Plugins query this
    // every process() call to align LFOs/sequences to tempo & transport.
    std::memset(&processContext_, 0, sizeof(processContext_));
    processContext_.sampleRate = opts_.sampleRate;
    processContext_.projectTimeSamples = 0;
    processContext_.tempo = 120.0;
    processContext_.timeSigNumerator = 4;
    processContext_.timeSigDenominator = 4;
    processContext_.state =
        Steinberg::Vst::ProcessContext::kPlaying |
        Steinberg::Vst::ProcessContext::kTempoValid |
        Steinberg::Vst::ProcessContext::kTimeSigValid |
        Steinberg::Vst::ProcessContext::kProjectTimeMusicValid |
        Steinberg::Vst::ProcessContext::kBarPositionValid;
    processContext_.projectTimeMusic = 0.0;
    processContext_.barPositionMusic = 0.0;
    processContext_.samplesToNextClock = 0;
    processData_.processContext = &processContext_;

    // Populate info struct
    info_.name = chosen.name();
    info_.vendor = chosen.vendor();
    info_.version = chosen.version();
    info_.category = chosen.category();
    info_.subCategories = chosen.subCategoriesString();
    info_.sdkVersion = chosen.sdkVersion();
    // Class ID
    {
        Steinberg::TUID tuid;
        std::memcpy(tuid, chosen.ID().data(), 16);
        static const char* hex = "0123456789abcdef";
        std::string s(32, '0');
        for (size_t i = 0; i < 16; ++i) {
            s[i * 2] = hex[(tuid[i] >> 4) & 0xF];
            s[i * 2 + 1] = hex[tuid[i] & 0xF];
        }
        info_.classId = s;
    }
    // Sum total audio channels across all buses (kept for backward compat
    // with the JS-visible PluginInstanceInfo.numAudioInputs/Outputs fields).
    Steinberg::int32 inAudio = 0, outAudio = 0;
    for (const auto& b : inputBusInfos_) inAudio += b.channelCount;
    for (const auto& b : outputBusInfos_) outAudio += b.channelCount;
    info_.numAudioInputs = inAudio;
    info_.numAudioOutputs = outAudio;
    info_.numMidiInputs = inMidi;
    info_.numMidiOutputs = outMidi;
    info_.parameterCount = controller_ ? controller_->getParameterCount() : 0;

    return true;
}

// Negotiate speaker arrangements per the VST3 spec:
//   1. Query each bus's current arrangement via getBusArrangement.
//   2. Try to set all input/output buses to stereo (kStereo).
//   3. If that fails, fall back to mono (kMono).
//   4. If that fails too, keep the plugin's reported arrangement as-is.
// We never block load on a failed negotiation — the plugin's defaults remain.
void PluginInstance::negotiateSpeakerArrangements() {
    if (!audioProcessor_ || inputBusInfos_.empty() || outputBusInfos_.empty()) return;

    auto readCurrent = [&](Steinberg::Vst::BusDirection dir,
                            std::vector<AudioBusInfoEntry>& infos) {
        for (size_t i = 0; i < infos.size(); ++i) {
            Steinberg::Vst::SpeakerArrangement arr = 0;
            if (audioProcessor_->getBusArrangement(dir, static_cast<Steinberg::int32>(i), arr)
                == Steinberg::kResultTrue) {
                infos[i].arrangement = arr;
            }
        }
    };
    readCurrent(Steinberg::Vst::kInput, inputBusInfos_);
    readCurrent(Steinberg::Vst::kOutput, outputBusInfos_);

    // Try stereo first (the common case for music plugins).
    std::vector<Steinberg::Vst::SpeakerArrangement> inArr(inputBusInfos_.size(),
                                                            Steinberg::Vst::SpeakerArr::kStereo);
    std::vector<Steinberg::Vst::SpeakerArrangement> outArr(outputBusInfos_.size(),
                                                             Steinberg::Vst::SpeakerArr::kStereo);
    Steinberg::tresult r = audioProcessor_->setBusArrangements(
        inArr.data(), static_cast<Steinberg::int32>(inArr.size()),
        outArr.data(), static_cast<Steinberg::int32>(outArr.size()));
    if (r == Steinberg::kResultTrue) {
        for (auto& b : inputBusInfos_) b.arrangement = Steinberg::Vst::SpeakerArr::kStereo;
        for (auto& b : outputBusInfos_) b.arrangement = Steinberg::Vst::SpeakerArr::kStereo;
        // Re-read channel counts (a plugin may have switched bus layouts).
        for (size_t i = 0; i < inputBusInfos_.size(); ++i) {
            Steinberg::Vst::BusInfo busInfo;
            if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                        Steinberg::Vst::kInput,
                                        static_cast<Steinberg::int32>(i), busInfo)
                == Steinberg::kResultTrue) {
                inputBusInfos_[i].channelCount = busInfo.channelCount;
            }
        }
        for (size_t i = 0; i < outputBusInfos_.size(); ++i) {
            Steinberg::Vst::BusInfo busInfo;
            if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                        Steinberg::Vst::kOutput,
                                        static_cast<Steinberg::int32>(i), busInfo)
                == Steinberg::kResultTrue) {
                outputBusInfos_[i].channelCount = busInfo.channelCount;
            }
        }
        return;
    }

    // Try mono fallback (single-channel plugins).
    std::fill(inArr.begin(), inArr.end(), Steinberg::Vst::SpeakerArr::kMono);
    std::fill(outArr.begin(), outArr.end(), Steinberg::Vst::SpeakerArr::kMono);
    r = audioProcessor_->setBusArrangements(
        inArr.data(), static_cast<Steinberg::int32>(inArr.size()),
        outArr.data(), static_cast<Steinberg::int32>(outArr.size()));
    if (r == Steinberg::kResultTrue) {
        for (auto& b : inputBusInfos_) b.arrangement = Steinberg::Vst::SpeakerArr::kMono;
        for (auto& b : outputBusInfos_) b.arrangement = Steinberg::Vst::SpeakerArr::kMono;
        for (size_t i = 0; i < inputBusInfos_.size(); ++i) {
            Steinberg::Vst::BusInfo busInfo;
            if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                        Steinberg::Vst::kInput,
                                        static_cast<Steinberg::int32>(i), busInfo)
                == Steinberg::kResultTrue) {
                inputBusInfos_[i].channelCount = busInfo.channelCount;
            }
        }
        for (size_t i = 0; i < outputBusInfos_.size(); ++i) {
            Steinberg::Vst::BusInfo busInfo;
            if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                        Steinberg::Vst::kOutput,
                                        static_cast<Steinberg::int32>(i), busInfo)
                == Steinberg::kResultTrue) {
                outputBusInfos_[i].channelCount = busInfo.channelCount;
            }
        }
        return;
    }
    // Otherwise leave the plugin's defaults in place.
}

void PluginInstance::teardown() {
    if (disposed_.exchange(true)) return;

    if (audioProcessor_ && processing_) {
        audioProcessor_->setProcessing(false);
        processing_ = false;
    }
    if (component_ && active_) {
        component_->setActive(false);
        active_ = false;
    }

    // Disconnect connection points
    if (controllerCP_) { controllerCP_->disconnect(); controllerCP_.reset(); }
    if (componentCP_) { componentCP_->disconnect(); componentCP_.reset(); }

    // Terminate controller (only if separate from component)
    if (controller_ && !info_.isSingleComponent) {
        if (auto plugCtrlBase = Steinberg::U::cast<Steinberg::IPluginBase>(controller_)) {
            plugCtrlBase->terminate();
        }
    }
    controller_.reset();

    // Terminate component
    if (component_) {
        if (auto plugBase = Steinberg::U::cast<Steinberg::IPluginBase>(component_)) {
            plugBase->terminate();
        }
    }
    component_.reset();
    audioProcessor_.reset();

    handler_.reset();
    hostApp_ = nullptr;
    module_.reset();
}

void PluginInstance::checkAlive() const {
    if (disposed_) throwNst(ErrorCode::Faulted, "PluginInstance has been disposed");
    if (faulted_) throwNst(ErrorCode::Faulted, "PluginInstance is faulted");
}

void PluginInstance::emitRestart(int32_t flags) {
    if (!restartTsfnValid_.load(std::memory_order_acquire)) return;
    auto* flagsHeap = new int32_t(flags);
    restartTsfn_.NonBlockingCall(flagsHeap, [](Napi::Env env, Napi::Function cb, int32_t* data) {
        cb.Call({Napi::Number::New(env, *data)});
        delete data;
    });
}

// Forward a plugin→host event to JS via the host-event TSFN. The TSFN callback
// runs on the JS thread, looks up the listener registered for `ev.name` in
// hostEventListeners_, and invokes it with the appropriate JS value (boolean
// for 'dirty', number for 'beginGesture'/'endGesture', undefined for
// 'startGroup'/'finishGroup'). Safe to call from any thread.
void PluginInstance::emitHostEvent(const HostEvent& ev) {
    if (!hostEventTsfnValid_.load(std::memory_order_acquire)) return;
    // Heap-allocate a copy of the event for the TSFN callback to own.
    // `ev.name` is a string literal with static storage duration, so the
    // pointer copy is safe.
    auto* heap = new HostEvent{ ev.name, ev.value, ev.isBool, ev.hasPayload };
    hostEventTsfn_.NonBlockingCall(heap, [this](Napi::Env env, Napi::Function,
                                                  HostEvent* data) {
        auto it = this->hostEventListeners_.find(data->name);
        if (it != this->hostEventListeners_.end() && !it->second.IsEmpty()) {
            Napi::Value arg;
            if (!data->hasPayload) {
                arg = env.Undefined();
            } else if (data->isBool) {
                arg = Napi::Boolean::New(env, data->value != 0.0);
            } else {
                arg = Napi::Number::New(env, data->value);
            }
            it->second.Call({ arg });
        }
        delete data;
    });
}

// IPerformEditSink: controller-driven parameter automation point.
// Called by ComponentHandler::performEdit (typically when a plugin's GUI
// turns a knob, or when an automated synth internally drives a parameter).
// We update the controller's normalized value and queue a point at sample
// offset 0 for the next process() call, matching the spec for performEdit.
void PluginInstance::onPerformEdit(Steinberg::Vst::ParamID id,
                                    Steinberg::Vst::ParamValue valueNormalized) {
    if (!controller_) return;
    controller_->setParamNormalized(id, valueNormalized);
    Steinberg::int32 idx = 0;
    auto* queue = inputParams_.addParameterData(id, idx);
    if (queue) {
        Steinberg::int32 ptIdx = 0;
        queue->addPoint(0, valueNormalized, ptIdx);
    }
}

// Try to map a MIDI controller (CC/PB/PC/CP) to a plugin parameter via the
// optional IMidiMapping interface. On success, sets the parameter on the
// controller and queues the change for the next process() call. On failure
// (no IMidiMapping, or plugin doesn't map this controller) returns false —
// caller should fall back to a kLegacyMIDICCOutEvent.
bool PluginInstance::tryMapMidiController(int16_t channel,
                                          Steinberg::Vst::CtrlNumber ctrlNumber,
                                          double normalizedValue,
                                          Steinberg::Vst::ParamID* outParamId) {
    if (!midiMapping_ || !controller_) return false;
    Steinberg::Vst::ParamID id = Steinberg::Vst::kNoParamId;
    Steinberg::tresult r = midiMapping_->getMidiControllerAssignment(
        /*busIndex*/ 0, channel, ctrlNumber, id);
    if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) return false;
    if (id == Steinberg::Vst::kNoParamId) return false;
    controller_->setParamNormalized(id, normalizedValue);
    Steinberg::int32 idx = 0;
    auto* queue = inputParams_.addParameterData(id, idx);
    if (queue) {
        Steinberg::int32 ptIdx = 0;
        queue->addPoint(0, normalizedValue, ptIdx);
    }
    if (outParamId) *outParamId = id;
    return true;
}

//------------------------------------------------------------------------
// Public methods
//------------------------------------------------------------------------
Napi::Value PluginInstance::Dispose(const Napi::CallbackInfo& info) {
    teardown();
    if (restartTsfnValid_.exchange(false)) {
        restartTsfn_.Release();
    }
    if (hostEventTsfnValid_.exchange(false)) {
        hostEventTsfn_.Release();
    }
    hostEventListeners_.clear();
    return info.Env().Undefined();
}

Napi::Value PluginInstance::GetInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    Napi::Object o = Napi::Object::New(env);
    o.Set("name", Napi::String::New(env, info_.name));
    o.Set("vendor", Napi::String::New(env, info_.vendor));
    o.Set("version", Napi::String::New(env, info_.version));
    o.Set("category", Napi::String::New(env, info_.category));
    o.Set("subCategories", Napi::String::New(env, info_.subCategories));
    o.Set("sdkVersion", Napi::String::New(env, info_.sdkVersion));
    o.Set("classId", Napi::String::New(env, info_.classId));
    o.Set("numAudioInputs", Napi::Number::New(env, info_.numAudioInputs));
    o.Set("numAudioOutputs", Napi::Number::New(env, info_.numAudioOutputs));
    o.Set("numMidiInputs", Napi::Number::New(env, info_.numMidiInputs));
    o.Set("numMidiOutputs", Napi::Number::New(env, info_.numMidiOutputs));
    o.Set("parameterCount", Napi::Number::New(env, info_.parameterCount));
    o.Set("hasController", Napi::Boolean::New(env, info_.hasController));
    o.Set("isSingleComponent", Napi::Boolean::New(env, info_.isSingleComponent));
    return o;
}

Napi::Value PluginInstance::GetLatency(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!audioProcessor_) throwNst(ErrorCode::Unknown, "No audio processor");
    Steinberg::uint32 latency = audioProcessor_->getLatencySamples();
    return Napi::Number::New(env, static_cast<double>(latency));
}

Napi::Value PluginInstance::GetSampleSize(const Napi::CallbackInfo& info) {
    checkAlive();
    return Napi::Number::New(info.Env(), static_cast<double>(activeSampleSize_));
}

Napi::Value PluginInstance::CanProcessSampleSize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!audioProcessor_) throwNst(ErrorCode::Unknown, "No audio processor");
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "canProcessSampleSize(size) requires a number (32 or 64)");
    }
    int32_t requested = info[0].As<Napi::Number>().Int32Value();
    // Map JS-visible 32/64 to the SDK symbolicSampleSize enum and probe.
    int32_t symbolic = (requested == 64)
        ? Steinberg::Vst::kSample64
        : Steinberg::Vst::kSample32;
    Steinberg::tresult r = audioProcessor_->canProcessSampleSize(symbolic);
    return Napi::Boolean::New(env, r == Steinberg::kResultTrue);
}

Napi::Value PluginInstance::GetTailSamples(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!audioProcessor_) throwNst(ErrorCode::Unknown, "No audio processor");
    Steinberg::uint32 tail = audioProcessor_->getTailSamples();
    // The VST3 SDK defines kInfiniteTail == 0xFFFFFFFF to indicate the plugin
    // never stops producing output (e.g. an infinite reverb). We surface that
    // to JS as Number.POSITIVE_INFINITY; otherwise we return the integer
    // sample count.
    if (tail == 0xFFFFFFFFu /* Steinberg::Vst::kInfiniteTail */) {
        return Napi::Number::New(env, std::numeric_limits<double>::infinity());
    }
    return Napi::Number::New(env, static_cast<double>(tail));
}

Napi::Value PluginInstance::SetActive(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 1 || !info[0].IsBoolean()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "setActive(bool) requires a boolean");
    }
    bool active = info[0].As<Napi::Boolean>().Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        if (active == active_) return env.Undefined();
        if (active) {
            // setupProcessing must be called before setActive. Use the user-
            // chosen process mode and the (possibly fallback-corrected)
            // sample size stored in activeSampleSize_.
            Steinberg::Vst::ProcessSetup setup{
                static_cast<Steinberg::Vst::ProcessModes>(processMode_),
                (activeSampleSize_ == 64)
                    ? Steinberg::Vst::kSample64 : Steinberg::Vst::kSample32,
                opts_.maxBlockSize,
                opts_.sampleRate
            };
            Steinberg::tresult r = audioProcessor_->setupProcessing(setup);
            if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
                throwNst(ErrorCode::ProcessingError, "setupProcessing failed");
            }
            r = component_->setActive(true);
            if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
                throwNst(ErrorCode::ProcessingError, "IComponent::setActive(true) failed");
            }
            // The chosen sample size is now "active" — record it so Process()
            // and getSampleSize() report the right value.
            active_ = true;
        } else {
            if (processing_) {
                audioProcessor_->setProcessing(false);
                processing_ = false;
            }
            component_->setActive(false);
            active_ = false;
        }
        return env.Undefined();
    });
}

Napi::Value PluginInstance::SetProcessing(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 1 || !info[0].IsBoolean()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "setProcessing(bool) requires a boolean");
    }
    bool proc = info[0].As<Napi::Boolean>().Value();
    if (!active_ && proc) {
        throwNapiError(env, ErrorCode::NotActive,
                       "setActive(true) must be called before setProcessing(true)");
    }
    return translateExceptions(env, [&]() -> Napi::Value {
        if (proc == processing_) return env.Undefined();
        Steinberg::tresult r = audioProcessor_->setProcessing(proc);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::ProcessingError,
                     "IAudioProcessor::setProcessing failed");
        }
        processing_ = proc;
        return env.Undefined();
    });
}

Napi::Value PluginInstance::Process(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!active_) throwNapiError(env, ErrorCode::NotActive, "Plugin is not active. Call setActive(true) first.");
    if (!processing_) throwNapiError(env, ErrorCode::NotProcessing, "Plugin is not processing. Call setProcessing(true) first.");
    if (info.Length() < 1 || !info[0].IsObject()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "process(block) requires an object");
    }
    Napi::Object block = info[0].As<Napi::Object>();
    if (!block.Has("numSamples") || !block.Get("numSamples").IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "process: block.numSamples (number) is required");
    }
    int32_t numSamples = block.Get("numSamples").As<Napi::Number>().Int32Value();
    // numSamples === 0 is a parameter-flush block per the VST3 spec: the host
    // calls IAudioProcessor::process with numSamples=0 and no audio buffers so
    // the plugin can flush pending parameter changes. We therefore accept
    // numSamples in [0, maxBlockSize].
    if (numSamples < 0 || numSamples > opts_.maxBlockSize) {
        throwNapiError(env, ErrorCode::InvalidBuffer,
                       "block.numSamples must be in [0, " + std::to_string(opts_.maxBlockSize) + "]");
    }

    // Parse optional input silence flags (one bitmask per input bus). Missing
    // entries default to 0 (no silence hint).
    std::vector<uint32_t> inputSilenceFlags;
    if (block.Has("inputSilenceFlags") && block.Get("inputSilenceFlags").IsArray()) {
        Napi::Array arr = block.Get("inputSilenceFlags").As<Napi::Array>();
        inputSilenceFlags.reserve(arr.Length());
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            Napi::Value v = arr[i];
            inputSilenceFlags.push_back(v.IsNumber()
                ? static_cast<uint32_t>(v.As<Napi::Number>().Uint32Value())
                : 0u);
        }
    }

    // Parameter-flush block: skip audio buffer resolution entirely. The plugin
    // still receives numSamples=0 and may read parameter changes / events.
    const bool flushBlock = (numSamples == 0);

    if (!flushBlock) {
        // Resolve input/output buses from the JS `inputs`/`outputs` values.
        // Supported shapes (per direction):
        //   - undefined / null: all buses silent
        //   - Float32Array/Float64Array[ch0, ch1, ...]: single-bus (backward compat)
        //   - Array[TypedArray, TypedArray, ...]: single-bus, explicit channels
        //   - Array[Array[TypedArray,...], Array[TypedArray,...]]: multi-bus
        // The accepted TypedArrayType depends on activeSampleSize_.
        if (block.Has("inputs")) {
            if (!resolveAudioBuses(env, block.Get("inputs"), inputBusInfos_,
                                    inputChannelPtrsPerBus_, numSamples, "inputs")) {
                return env.Undefined();
            }
        } else {
            for (auto& v : inputChannelPtrsPerBus_) std::fill(v.begin(), v.end(), nullptr);
        }
        if (block.Has("outputs")) {
            if (!resolveAudioBuses(env, block.Get("outputs"), outputBusInfos_,
                                    outputChannelPtrsPerBus_, numSamples, "outputs")) {
                return env.Undefined();
            }
        } else {
            for (auto& v : outputChannelPtrsPerBus_) std::fill(v.begin(), v.end(), nullptr);
        }
    }

    return translateExceptions(env, [&]() -> Napi::Value {
        if (flushBlock) {
            // No audio buffers; the plugin must not touch channelBuffers*.
            for (size_t i = 0; i < inputBuffers_.size(); ++i) {
                inputBuffers_[i].numChannels = 0;
                inputBuffers_[i].silenceFlags = 0;
            }
            for (size_t i = 0; i < outputBuffers_.size(); ++i) {
                outputBuffers_[i].numChannels = 0;
                outputBuffers_[i].silenceFlags = 0;
            }
        } else {
            // Wire per-bus AudioBusBuffers to the freshly resolved channel ptrs.
            // The union member set depends on activeSampleSize_.
            for (size_t i = 0; i < inputBuffers_.size(); ++i) {
                inputBuffers_[i].numChannels =
                    static_cast<Steinberg::int32>(inputChannelPtrsPerBus_[i].size());
                if (activeSampleSize_ == 64) {
                    inputBuffers_[i].channelBuffers64 =
                        reinterpret_cast<double**>(inputChannelPtrsPerBus_[i].data());
                } else {
                    inputBuffers_[i].channelBuffers32 =
                        reinterpret_cast<float**>(inputChannelPtrsPerBus_[i].data());
                }
                // Apply user-supplied input silence hint (default 0).
                inputBuffers_[i].silenceFlags =
                    (i < inputSilenceFlags.size()) ? inputSilenceFlags[i] : 0u;
            }
            for (size_t i = 0; i < outputBuffers_.size(); ++i) {
                outputBuffers_[i].numChannels =
                    static_cast<Steinberg::int32>(outputChannelPtrsPerBus_[i].size());
                if (activeSampleSize_ == 64) {
                    outputBuffers_[i].channelBuffers64 =
                        reinterpret_cast<double**>(outputChannelPtrsPerBus_[i].data());
                } else {
                    outputBuffers_[i].channelBuffers32 =
                        reinterpret_cast<float**>(outputChannelPtrsPerBus_[i].data());
                }
                outputBuffers_[i].silenceFlags = 0;
            }
        }

        processData_.numSamples = numSamples;
        // numInputs / numOutputs / inputs / outputs already wired in setup().

        // Reset output containers (clear stale state from prior process)
        outputParams_.clearQueue();
        outputEvents_.clear();

        // Determine which ProcessContext fields the plugin actually needs.
        // When IProcessContextRequirements is implemented, we honor its
        // bitmask so we skip unnecessary recompute work on the steady-state
        // path; when it isn't implemented (the common case) we compute
        // everything (preserving prior behavior).
        const uint32_t ctxReqs = processContextReqs_
            ? processContextReqs_->getProcessContextRequirements()
            : 0xFFFFFFFFu;
        // When the plugin explicitly requests fields, make sure the
        // corresponding state bits are set so the plugin can rely on them
        // (use sensible defaults when the user hasn't supplied values).
        if (processContextReqs_) {
            if (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedTempo) {
                processContext_.state |= Steinberg::Vst::ProcessContext::kTempoValid;
                processContext_.state |= Steinberg::Vst::ProcessContext::kProjectTimeMusicValid;
            }
            if (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedTimeSignature) {
                processContext_.state |= Steinberg::Vst::ProcessContext::kTimeSigValid;
            }
            if (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedTransportState) {
                // kPlaying / kCycleActive / kRecording are user-driven; leave
                // them as the user configured (default kPlaying set).
            }
            if (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedSystemTime) {
                processContext_.state |= Steinberg::Vst::ProcessContext::kSystemTimeValid;
            }
            if (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedContinousTimeSamples) {
                processContext_.state |= Steinberg::Vst::ProcessContext::kContTimeValid;
            }
        }

        // Always refresh systemTime to the current wall-clock time when the
        // plugin asked for it (kSystemTimeValid set). This matches typical
        // host behavior — the host supplies a fresh nanosecond timestamp
        // every block. If the user has not opted in (state bit clear), leave
        // systemTime at 0.
        if (processContext_.state & Steinberg::Vst::ProcessContext::kSystemTimeValid) {
            auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            processContext_.systemTime = static_cast<int64_t>(nowNs);
        }

        // Advance the persistent ProcessContext. Transport advances only
        // when kPlaying is set in the state bitmask (the VST3 convention).
        // samplesToNextClock is derived from samplePosition and is updated
        // regardless of transport state so the next MIDI clock tick is
        // always reported correctly.
        const bool playing =
            (processContext_.state & Steinberg::Vst::ProcessContext::kPlaying) != 0;
        if (playing) {
            processContext_.projectTimeSamples += numSamples;
            // VST3 convention: projectTimeMusic is measured in quarter notes
            // (a.k.a. "PPQ" position). The bar position is the start of the
            // current bar, computed by snapping projectTimeMusic down to the
            // nearest bar boundary. One bar = 4 * numerator / denominator
            // quarter notes for any simple or compound meter (e.g. 4/4 → 4
            // quarters/bar, 6/8 → 3 quarters/bar, 7/8 → 3.5 quarters/bar).
            // The previous formula coincidentally gave the right answer but
            // is now spelled explicitly as (4 * numerator) / denominator to
            // document the convention.
            if (processContext_.state & Steinberg::Vst::ProcessContext::kTempoValid) {
                double quartersPerSample =
                    processContext_.tempo / (60.0 * opts_.sampleRate);
                processContext_.projectTimeMusic +=
                    static_cast<double>(numSamples) * quartersPerSample;
            }
            // Recompute bar position only if the plugin asked for bars or
            // cycle position (or when no requirements were declared, in which
            // case we compute it for backward compatibility).
            const bool needBars = (ctxReqs == 0xFFFFFFFFu) ||
                (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedBarPositionMusic) ||
                (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedCycleMusic);
            if (needBars &&
                (processContext_.state & Steinberg::Vst::ProcessContext::kTempoValid)) {
                double quartersPerBar = static_cast<double>(
                    processContext_.timeSigNumerator * 4) /
                    static_cast<double>(
                        processContext_.timeSigDenominator > 0
                            ? processContext_.timeSigDenominator : 4);
                double bars =
                    std::floor(processContext_.projectTimeMusic / quartersPerBar);
                processContext_.barPositionMusic = bars * quartersPerBar;
                processContext_.state |= Steinberg::Vst::ProcessContext::kBarPositionValid;
            }
            if (processContext_.state & Steinberg::Vst::ProcessContext::kContTimeValid) {
                processContext_.continousTimeSamples += numSamples;
            }
        }
        // samplesToNextClock: 24 PPQ per quarter note. Compute the distance
        // from the current sample position to the next MIDI clock tick.
        // Recomputed on every block (regardless of transport state) when no
        // requirements were declared, or only when the plugin needs it.
        const bool needClock = (ctxReqs == 0xFFFFFFFFu) ||
            (ctxReqs & Steinberg::Vst::IProcessContextRequirements::kNeedSamplesToNextClock);
        if (needClock &&
            (processContext_.state & Steinberg::Vst::ProcessContext::kTempoValid)) {
            double samplesPerQuarter =
                (60.0 * opts_.sampleRate) / processContext_.tempo;
            double samplesPerClock = samplesPerQuarter / 24.0;
            double frac = std::fmod(static_cast<double>(processContext_.projectTimeSamples),
                                     samplesPerClock);
            processContext_.samplesToNextClock =
                static_cast<int32_t>(samplesPerClock - frac);
            if (processContext_.samplesToNextClock < 0)
                processContext_.samplesToNextClock = 0;
        }
        processData_.processContext = &processContext_;

        Steinberg::tresult r = audioProcessor_->process(processData_);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            faulted_ = true;
            throwNst(ErrorCode::ProcessingError,
                     "IAudioProcessor::process returned tresult=" + std::to_string(static_cast<int>(r)));
        }

        // Collect output silence flags reported by the plugin into a JS array
        // (one entry per output bus). Existing callers that ignore the return
        // value are unaffected — the engine doesn't care.
        Napi::Object result = Napi::Object::New(env);
        Napi::Array outSilence = Napi::Array::New(env, outputBuffers_.size());
        for (size_t i = 0; i < outputBuffers_.size(); ++i) {
            outSilence[static_cast<uint32_t>(i)] =
                Napi::Number::New(env, static_cast<double>(outputBuffers_[i].silenceFlags));
        }
        result.Set("outputSilenceFlags", outSilence);

        // After process, the SDK convention is that the host clears the input
        // parameter changes and event list. We clear them now so the next call
        // starts fresh.
        inputParams_.clearQueue();
        inputEvents_.clear();
        sysexHeld_.clear();

        return result;
    });
}

// Resolve JS `inputs`/`outputs` value to per-bus channel pointer vectors.
// Returns false (and throws) on shape/length errors.
bool PluginInstance::resolveAudioBuses(
    Napi::Env env, Napi::Value value,
    const std::vector<AudioBusInfoEntry>& busInfos,
    std::vector<std::vector<void*>>& outPtrs,
    int32_t numSamples, const char* dirName) {
    // Reset all channel pointers to nullptr (silence).
    for (auto& v : outPtrs) std::fill(v.begin(), v.end(), nullptr);

    if (value.IsNull() || value.IsUndefined()) return true;
    if (!value.IsArray()) {
        throwNapiError(env, ErrorCode::InvalidBuffer,
                       std::string(dirName) + " must be a TypedArray[] or TypedArray[][]");
        return false;
    }
    Napi::Array arr = value.As<Napi::Array>();

    // Helper: extract a typed-array channel with length check. The accepted
    // TypedArrayType depends on activeSampleSize_: Float32Array for kSample32,
    // Float64Array for kSample64. All channels in a block must match the
    // expected type (mixing 32/64 in one call is rejected).
    auto extractChannel = [&](Napi::Value v, void*& out) -> bool {
        if (!v.IsTypedArray()) {
            throwNapiError(env, ErrorCode::InvalidBuffer,
                           std::string(dirName) + " entries must be a Float32Array (sampleSize=32) "
                           "or Float64Array (sampleSize=64)");
            return false;
        }
        Napi::TypedArray ta = v.As<Napi::TypedArray>();
        napi_typedarray_type tt = ta.TypedArrayType();
        if (activeSampleSize_ == 64) {
            if (tt != napi_float64_array) {
                throwNapiError(env, ErrorCode::InvalidBuffer,
                               std::string(dirName) + " entries must be Float64Array when sampleSize=64");
                return false;
            }
            Napi::Float64Array fa = v.As<Napi::Float64Array>();
            if (static_cast<int32_t>(fa.ElementLength()) < numSamples) {
                throwNapiError(env, ErrorCode::InvalidBuffer,
                               std::string(dirName) + " buffer length < numSamples");
                return false;
            }
            out = fa.Data();
        } else {
            if (tt != napi_float32_array) {
                throwNapiError(env, ErrorCode::InvalidBuffer,
                               std::string(dirName) + " entries must be Float32Array when sampleSize=32");
                return false;
            }
            Napi::Float32Array fa = v.As<Napi::Float32Array>();
            if (static_cast<int32_t>(fa.ElementLength()) < numSamples) {
                throwNapiError(env, ErrorCode::InvalidBuffer,
                               std::string(dirName) + " buffer length < numSamples");
                return false;
            }
            out = fa.Data();
        }
        return true;
    };

    // Detect multi-bus form: an array whose first element is itself an array
    // of typed arrays. Single-bus form: a flat array of typed arrays.
    bool multiBus = false;
    if (arr.Length() > 0) {
        Napi::Value first = arr[(uint32_t)0];
        if (first.IsArray()) multiBus = true;
    }

    if (!multiBus) {
        // Flat channel list, applied to bus 0 only. If bus 0 doesn't exist,
        // we silently ignore — the plugin has no audio inputs/outputs.
        if (busInfos.empty()) return true;
        auto& bus0 = outPtrs[0];
        for (uint32_t i = 0; i < arr.Length() && i < bus0.size(); ++i) {
            if (!extractChannel(arr[i], bus0[i])) return false;
        }
        return true;
    }

    // Multi-bus form: each outer element is an array of channels for that bus.
    if (arr.Length() > busInfos.size()) {
        throwNapiError(env, ErrorCode::InvalidBuffer,
                       std::string(dirName) + ": more buses supplied than declared");
        return false;
    }
    for (uint32_t b = 0; b < arr.Length(); ++b) {
        Napi::Value busVal = arr[b];
        if (!busVal.IsArray()) {
            throwNapiError(env, ErrorCode::InvalidBuffer,
                           std::string(dirName) + "[" + std::to_string(b) + "] must be an array");
            return false;
        }
        Napi::Array busArr = busVal.As<Napi::Array>();
        auto& busPtrs = outPtrs[b];
        for (uint32_t c = 0; c < busArr.Length() && c < busPtrs.size(); ++c) {
            if (!extractChannel(busArr[c], busPtrs[c])) return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------
// Parameters
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetParameterCount(const Napi::CallbackInfo& info) {
    checkAlive();
    if (!controller_) return Napi::Number::New(info.Env(), 0);
    return Napi::Number::New(info.Env(), controller_->getParameterCount());
}

Napi::Value PluginInstance::GetParameterInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "getParameterInfo(index) requires a number");
    }
    int32_t index = info[0].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::ParameterInfo pi;
        Steinberg::tresult r = controller_->getParameterInfo(index, pi);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "getParameterInfo failed");
        }
        Napi::Object o = Napi::Object::New(env);
        o.Set("id", Napi::Number::New(env, static_cast<double>(pi.id)));
        o.Set("title", Napi::String::New(env, string128ToUtf8(pi.title)));
        o.Set("shortTitle", Napi::String::New(env, string128ToUtf8(pi.shortTitle)));
        o.Set("units", Napi::String::New(env, string128ToUtf8(pi.units)));
        o.Set("stepCount", Napi::Number::New(env, pi.stepCount));
        o.Set("defaultNormalizedValue", Napi::Number::New(env, pi.defaultNormalizedValue));
        o.Set("unitId", Napi::Number::New(env, static_cast<double>(pi.unitId)));
        o.Set("flags", Napi::Number::New(env, static_cast<double>(pi.flags)));
        return o;
    });
}

Napi::Value PluginInstance::GetParameter(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "getParameter(id) requires a number");
    }
    Steinberg::Vst::ParamID id = static_cast<Steinberg::Vst::ParamID>(info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::ParamValue v = controller_->getParamNormalized(id);
    return Napi::Number::New(env, v);
}

Napi::Value PluginInstance::SetParameter(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "setParameter(id, value) requires two numbers");
    }
    Steinberg::Vst::ParamID id = static_cast<Steinberg::Vst::ParamID>(info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::ParamValue value = info[1].As<Napi::Number>().DoubleValue();
    return translateExceptions(env, [&]() -> Napi::Value {
        controller_->setParamNormalized(id, value);
        // Queue change for next process
        Steinberg::int32 idx = 0;
        auto* queue = inputParams_.addParameterData(id, idx);
        if (queue) {
            Steinberg::int32 ptIdx = 0;
            queue->addPoint(0, value, ptIdx);
        }
        return env.Undefined();
    });
}

Napi::Value PluginInstance::SetParameters(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 1 || !info[0].IsArray()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "setParameters(changes[]) requires an array");
    }
    Napi::Array arr = info[0].As<Napi::Array>();
    return translateExceptions(env, [&]() -> Napi::Value {
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            Napi::Value v = arr[i];
            if (!v.IsObject()) continue;
            Napi::Object o = v.As<Napi::Object>();
            if (!o.Has("id") || !o.Has("value")) continue;
            Steinberg::Vst::ParamID id = static_cast<Steinberg::Vst::ParamID>(
                o.Get("id").As<Napi::Number>().Int32Value());
            Steinberg::Vst::ParamValue value = o.Get("value").As<Napi::Number>().DoubleValue();
            controller_->setParamNormalized(id, value);
            Steinberg::int32 idx = 0;
            auto* queue = inputParams_.addParameterData(id, idx);
            if (queue) {
                Steinberg::int32 ptIdx = 0;
                queue->addPoint(0, value, ptIdx);
            }
        }
        return env.Undefined();
    });
}

Napi::Value PluginInstance::FormatParameter(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "formatParameter(id, value) requires two numbers");
    }
    Steinberg::Vst::ParamID id = static_cast<Steinberg::Vst::ParamID>(info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::ParamValue value = info[1].As<Napi::Number>().DoubleValue();
    Steinberg::Vst::String128 out;
    Steinberg::tresult r = controller_->getParamStringByValue(id, value, out);
    if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
        return Napi::String::New(env, "");
    }
    return Napi::String::New(env, string128ToUtf8(out));
}

Napi::Value PluginInstance::ParseParameter(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsString()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "parseParameter(id, str) requires (number, string)");
    }
    Steinberg::Vst::ParamID id = static_cast<Steinberg::Vst::ParamID>(info[0].As<Napi::Number>().Int32Value());
    std::string str = info[1].As<Napi::String>().Utf8Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::String128 s128;
        utf8ToString128(str, s128);
        Steinberg::Vst::ParamValue value = 0.0;
        Steinberg::tresult r = controller_->getParamValueByString(id, s128, value);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter,
                     "IEditController::getParamValueByString returned kResultFalse");
        }
        return Napi::Number::New(env, value);
    });
}

Napi::Value PluginInstance::PlainToNormalized(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "plainToNormalized(id, plain) requires two numbers");
    }
    Steinberg::Vst::ParamID id = static_cast<Steinberg::Vst::ParamID>(info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::ParamValue plain = info[1].As<Napi::Number>().DoubleValue();
    return translateExceptions(env, [&]() -> Napi::Value {
        // IEditController::plainParamToNormalized returns a ParamValue
        // directly (no tresult out-param variant exists in the SDK).
        Steinberg::Vst::ParamValue value =
            controller_->plainParamToNormalized(id, plain);
        return Napi::Number::New(env, value);
    });
}

Napi::Value PluginInstance::NormalizedToPlain(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!controller_) throwNapiError(env, ErrorCode::Unknown, "No edit controller");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "normalizedToPlain(id, normalized) requires two numbers");
    }
    Steinberg::Vst::ParamID id = static_cast<Steinberg::Vst::ParamID>(info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::ParamValue normalized = info[1].As<Napi::Number>().DoubleValue();
    return translateExceptions(env, [&]() -> Napi::Value {
        // IEditController::normalizedParamToPlain returns a ParamValue
        // directly (no tresult out-param variant exists in the SDK).
        Steinberg::Vst::ParamValue value =
            controller_->normalizedParamToPlain(id, normalized);
        return Napi::Number::New(env, value);
    });
}

//------------------------------------------------------------------------
// MIDI / Events
//------------------------------------------------------------------------
Napi::Value PluginInstance::AddMidiEvent(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 1 || !info[0].IsObject()) {
        throwNapiError(env, ErrorCode::MidiError, "addMidiEvent(event) requires an object");
    }
    Napi::Object o = info[0].As<Napi::Object>();
    return translateExceptions(env, [&]() -> Napi::Value {
        int type = o.Has("type") ? o.Get("type").As<Napi::Number>().Int32Value() : -1;
        int channel = o.Has("channel") ? o.Get("channel").As<Napi::Number>().Int32Value() : 0;
        int note = o.Has("note") ? o.Get("note").As<Napi::Number>().Int32Value() : 0;
        int velocity = o.Has("velocity") ? o.Get("velocity").As<Napi::Number>().Int32Value() : 0;
        int cc = o.Has("controllerNumber") ? o.Get("controllerNumber").As<Napi::Number>().Int32Value() : 0;
        int ccVal = o.Has("controllerValue") ? o.Get("controllerValue").As<Napi::Number>().Int32Value() : 0;
        int prog = o.Has("programNumber") ? o.Get("programNumber").As<Napi::Number>().Int32Value() : 0;
        int pressure = o.Has("pressure") ? o.Get("pressure").As<Napi::Number>().Int32Value() : 0;
        int pb = o.Has("pitchBend") ? o.Get("pitchBend").As<Napi::Number>().Int32Value() : 0;
        int32_t sampleOffset = o.Has("sampleOffset") ? o.Get("sampleOffset").As<Napi::Number>().Int32Value() : 0;
        // Optional noteId (default 0). Propagated to Event::noteOn/noteOff/
        // polyPressure.noteId so note-expression events can target a specific
        // note instance by ID.
        int32_t noteId = (o.Has("noteId") && o.Get("noteId").IsNumber())
            ? o.Get("noteId").As<Napi::Number>().Int32Value() : 0;

        const uint8_t* sysexData = nullptr;
        size_t sysexSize = 0;
        if (o.Has("sysEx") && o.Get("sysEx").IsTypedArray()) {
            Napi::Uint8Array arr = o.Get("sysEx").As<Napi::Uint8Array>();
            sysexData = arr.Data();
            sysexSize = arr.ElementLength();
            // Hold the buffer alive until next process() clears sysexHeld_
            sysexHeld_.emplace_back(arr.Data(), arr.Data() + arr.ElementLength());
            sysexData = sysexHeld_.back().data();
        }

        // For CC / Program Change / Channel Pressure / Pitch Bend, prefer the
        // IMidiMapping interface (VST3-spec compliant): map the controller to
        // a parameter and queue a parameter change for the next process().
        // If the plugin doesn't implement IMidiMapping (or doesn't map this
        // specific controller), fall back to the kLegacyMIDICCOutEvent path
        // below by letting structuredMidiToEvent handle it.
        auto t = static_cast<MidiEventType>(type);
        int16_t ch = static_cast<int16_t>(channel & 0x0F);
        if (t == MidiEventType::Controller) {
            auto ctrl = static_cast<Steinberg::Vst::CtrlNumber>(cc & 0x7F);
            double norm = static_cast<double>(ccVal & 0x7F) / 127.0;
            if (tryMapMidiController(ch, ctrl, norm)) return env.Undefined();
        } else if (t == MidiEventType::ProgramChange) {
            double norm = static_cast<double>(prog & 0x7F) / 127.0;
            if (tryMapMidiController(ch, Steinberg::Vst::kCtrlProgramChange, norm))
                return env.Undefined();
        } else if (t == MidiEventType::ChannelPressure) {
            double norm = static_cast<double>(pressure & 0x7F) / 127.0;
            if (tryMapMidiController(ch, Steinberg::Vst::kAfterTouch, norm))
                return env.Undefined();
        } else if (t == MidiEventType::PitchBend) {
            // pitchBend is signed [-8192, 8191]; normalize to [0, 1].
            int32_t pb14 = (pb + 8192) & 0x3FFF;
            double norm = static_cast<double>(pb14) / 16383.0;
            if (tryMapMidiController(ch, Steinberg::Vst::kPitchBend, norm))
                return env.Undefined();
        }

        // Event::kIsLive is cleared when the process mode is not realtime so
        // plugins don't treat queued events as live player input.
        const bool isLive = (processMode_ == static_cast<int32_t>(Steinberg::Vst::kRealtime));
        Steinberg::Vst::Event e;
        if (!structuredMidiToEvent(type, channel, note, velocity, cc, ccVal, prog,
                                   pressure, pb, sysexData, sysexSize, sampleOffset, e,
                                   isLive, noteId)) {
            throwNst(ErrorCode::MidiError, "Failed to convert MIDI event");
        }
        inputEvents_.addEvent(e);
        return env.Undefined();
    });
}

Napi::Value PluginInstance::AddMidiBytes(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 2 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::MidiError, "addMidiBytes(sampleOffset, bytes) requires (number, Uint8Array)");
    }
    int32_t sampleOffset = info[0].As<Napi::Number>().Int32Value();
    if (!info[1].IsTypedArray()) {
        throwNapiError(env, ErrorCode::MidiError, "addMidiBytes: second arg must be a Uint8Array");
    }
    Napi::Uint8Array arr = info[1].As<Napi::Uint8Array>();
    return translateExceptions(env, [&]() -> Napi::Value {
        // Event::kIsLive is cleared when the process mode is not realtime so
        // plugins don't treat queued events as live player input.
        const bool isLive = (processMode_ == static_cast<int32_t>(Steinberg::Vst::kRealtime));
        // For SysEx we need to hold the buffer; for other messages we copy into the Event.
        if (arr.ElementLength() > 0 && arr.Data()[0] == 0xF0) {
            sysexHeld_.emplace_back(arr.Data(), arr.Data() + arr.ElementLength());
            Steinberg::Vst::Event e;
            if (!midiBytesToEvent(sysexHeld_.back().data(), sysexHeld_.back().size(),
                                  sampleOffset, e, isLive, /*noteId*/ 0)) {
                throwNst(ErrorCode::MidiError, "Failed to parse SysEx bytes");
            }
            // The Event points to our held buffer; safe until next process()
            inputEvents_.addEvent(e);
            return env.Undefined();
        }

        // Parse the bytes first to know which controller type this is.
        Steinberg::Vst::Event e;
        if (!midiBytesToEvent(arr.Data(), arr.ElementLength(), sampleOffset, e, isLive, /*noteId*/ 0)) {
            throwNst(ErrorCode::MidiError, "Failed to parse MIDI bytes");
        }

        // If the parsed event is a legacy MIDI CC event, try IMidiMapping
        // first. The mapping is preferred per the VST3 spec for plugins that
        // declare CC/PB/PC/CP as parameters.
        if (e.type == Steinberg::Vst::Event::kLegacyMIDICCOutEvent) {
            int16_t ch = static_cast<int16_t>(e.midiCCOut.channel);
            auto ctrl = static_cast<Steinberg::Vst::CtrlNumber>(
                e.midiCCOut.controlNumber);
            double norm = 0.0;
            if (ctrl == Steinberg::Vst::kPitchBend) {
                int32_t pb14 = (static_cast<int32_t>(e.midiCCOut.value & 0x7F)) |
                               (static_cast<int32_t>(e.midiCCOut.value2 & 0x7F) << 7);
                norm = static_cast<double>(pb14) / 16383.0;
            } else {
                norm = static_cast<double>(e.midiCCOut.value & 0x7F) / 127.0;
            }
            if (tryMapMidiController(ch, ctrl, norm)) {
                return env.Undefined();  // mapped — skip legacy event
            }
        }

        inputEvents_.addEvent(e);
        return env.Undefined();
    });
}

Napi::Value PluginInstance::TakeOutputEvents(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    int32_t count = outputEvents_.getEventCount();
    Napi::Array result = Napi::Array::New(env, count);
    for (int32_t i = 0; i < count; ++i) {
        Steinberg::Vst::Event e;
        if (outputEvents_.getEvent(i, e) != Steinberg::kResultTrue) continue;
        MidiEventOut out;
        if (!eventToMidiOut(e, out)) continue;
        Napi::Object o = Napi::Object::New(env);
        o.Set("type", Napi::Number::New(env, out.type));
        o.Set("channel", Napi::Number::New(env, out.channel));
        o.Set("note", Napi::Number::New(env, out.note));
        o.Set("velocity", Napi::Number::New(env, out.velocity));
        o.Set("controllerNumber", Napi::Number::New(env, out.controllerNumber));
        o.Set("controllerValue", Napi::Number::New(env, out.controllerValue));
        o.Set("programNumber", Napi::Number::New(env, out.programNumber));
        o.Set("pressure", Napi::Number::New(env, out.pressure));
        o.Set("pitchBend", Napi::Number::New(env, out.pitchBend));
        o.Set("sampleOffset", Napi::Number::New(env, out.sampleOffset));
        if (!out.sysEx.empty()) {
            Napi::Uint8Array buf = Napi::Uint8Array::New(env, out.sysEx.size());
            std::memcpy(buf.Data(), out.sysEx.data(), out.sysEx.size());
            o.Set("sysEx", buf);
        }
        result[i] = o;
    }
    outputEvents_.clear();
    return result;
}

Napi::Value PluginInstance::ClearEvents(const Napi::CallbackInfo& info) {
    checkAlive();
    inputEvents_.clear();
    sysexHeld_.clear();
    return info.Env().Undefined();
}

//------------------------------------------------------------------------
// State
//------------------------------------------------------------------------
// SaveState: serializes both the component state (IComponent::getState) and
// the controller state (IEditController::getState, if the controller exposes
// separate state) into a versioned envelope. The envelope format is:
//   bytes 0-3:   magic 'NST3'
//   byte  4:     version (1)
//   bytes 5-8:   component-state length (uint32 LE)
//   bytes 9..:   component-state bytes
//   next 4:      controller-state length (uint32 LE)
//   next len2:   controller-state bytes (may be empty)
// Many controllers return kResultFalse for getState (they have no separate
// state); in that case the controller length is 0. The envelope is
// backward-compatible: loadState detects the magic and parses it; legacy
// single-blob buffers (without the magic) are loaded as before.
Napi::Value PluginInstance::SaveState(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    return translateExceptions(env, [&]() -> Napi::Value {
        // 1. Component state (always present per the VST3 spec).
        BufferStream compStream;
        Steinberg::tresult r = component_->getState(&compStream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IComponent::getState failed");
        }
        auto compBytes = compStream.takeBuffer();

        // 2. Controller state (optional — many controllers return kResultFalse
        //    because they have no separate state). We only include controller
        //    bytes when getState returns kResultTrue/kResultOk AND the stream
        //    is non-empty; otherwise the controller length is 0.
        std::vector<uint8_t> ctrlBytes;
        if (controller_) {
            BufferStream ctrlStream;
            Steinberg::tresult cr = controller_->getState(&ctrlStream);
            if (cr == Steinberg::kResultTrue || cr == Steinberg::kResultOk) {
                ctrlBytes = ctrlStream.takeBuffer();
            }
        }

        // 3. Build the versioned envelope and return as a Buffer.
        auto envelope = composeStateEnvelope(compBytes, ctrlBytes);
        return Napi::Buffer<uint8_t>::Copy(env, envelope.data(), envelope.size());
    });
}

// LoadState: detects the versioned envelope format (magic 'NST3' + version 1)
// and routes the component and controller bytes to the appropriate setState
// methods. Legacy single-blob buffers (without the magic) are treated as
// plain component state (existing behavior preserved).
Napi::Value PluginInstance::LoadState(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    if (info.Length() < 1 || !info[0].IsBuffer()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "loadState(buffer) requires a Buffer");
    }
    auto buf = info[0].As<Napi::Buffer<uint8_t>>();
    return translateExceptions(env, [&]() -> Napi::Value {
        // Try to parse as versioned envelope (NST3 magic + version 1).
        std::vector<uint8_t> compBytes, ctrlBytes;
        const bool isEnvelope = parseStateEnvelope(buf.Data(), buf.Length(),
                                                    compBytes, ctrlBytes);

        if (isEnvelope) {
            // Versioned envelope path:
            //   1. IComponent::setState(compStream)
            //   2. IEditController::setComponentState(fresh compStream copy)
            //      (if controller exists)
            //   3. IEditController::setState(ctrlStream) (if controller length > 0)
            BufferStream compStream(compBytes.data(), compBytes.size());
            Steinberg::tresult r = component_->setState(&compStream);
            if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
                throwNst(ErrorCode::StateError, "IComponent::setState failed");
            }
            if (controller_) {
                // setComponentState needs a fresh stream positioned at 0
                // (setState already consumed the first stream).
                BufferStream compStream2(compBytes.data(), compBytes.size());
                r = controller_->setComponentState(&compStream2);
                // Some plugins return kResultFalse here but it's typically fine.
                (void)r;
                if (!ctrlBytes.empty()) {
                    BufferStream ctrlStream(ctrlBytes.data(), ctrlBytes.size());
                    r = controller_->setState(&ctrlStream);
                    (void)r;
                }
            }
            return env.Undefined();
        }

        // Legacy single-blob path: treat entire buffer as component state.
        // Existing behavior preserved for backward compatibility.
        BufferStream stream(buf.Data(), buf.Length());
        Steinberg::tresult r = component_->setState(&stream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IComponent::setState failed");
        }
        if (controller_) {
            // Reset stream position so setComponentState reads from the start.
            BufferStream stream2(buf.Data(), buf.Length());
            r = controller_->setComponentState(&stream2);
            // Some plugins return kResultFalse here but it's typically fine.
            (void)r;
        }
        return env.Undefined();
    });
}

//------------------------------------------------------------------------
// IUnitInfo — units + programs
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetUnitCount(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, unitInfo_->getUnitCount());
}

Napi::Value PluginInstance::GetUnitInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) throwNapiError(env, ErrorCode::Unknown, "Plugin does not implement IUnitInfo");
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getUnitInfo(index) requires a number (zero-based unit index)");
    }
    int32_t index = info[0].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::UnitInfo u;
        std::memset(&u, 0, sizeof(u));
        Steinberg::tresult r = unitInfo_->getUnitInfo(index, u);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IUnitInfo::getUnitInfo failed");
        }
        Napi::Object o = Napi::Object::New(env);
        o.Set("id", Napi::Number::New(env, static_cast<double>(u.id)));
        o.Set("name", Napi::String::New(env, string128ToUtf8(u.name)));
        o.Set("programListId", Napi::Number::New(env, static_cast<double>(u.programListId)));
        o.Set("parentUnitId", Napi::Number::New(env, static_cast<double>(u.parentUnitId)));
        // Note: the pinned SDK's UnitInfo struct has only {id, parentUnitId,
        // name, programListId} — there is no `type` field (added in a later
        // SDK). We do not expose a `type` property.
        return o;
    });
}

Napi::Value PluginInstance::GetProgramListCount(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, unitInfo_->getProgramListCount());
}

Napi::Value PluginInstance::GetProgramListInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) throwNapiError(env, ErrorCode::Unknown, "Plugin does not implement IUnitInfo");
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getProgramListInfo(listIndex) requires a number");
    }
    int32_t listIndex = info[0].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::ProgramListInfo pli;
        std::memset(&pli, 0, sizeof(pli));
        Steinberg::tresult r = unitInfo_->getProgramListInfo(listIndex, pli);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IUnitInfo::getProgramListInfo failed");
        }
        Napi::Object o = Napi::Object::New(env);
        o.Set("id", Napi::Number::New(env, static_cast<double>(pli.id)));
        o.Set("name", Napi::String::New(env, string128ToUtf8(pli.name)));
        o.Set("programCount", Napi::Number::New(env, pli.programCount));
        return o;
    });
}

Napi::Value PluginInstance::GetProgramName(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) throwNapiError(env, ErrorCode::Unknown, "Plugin does not implement IUnitInfo");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getProgramName(listId, programIndex) requires two numbers");
    }
    Steinberg::Vst::ProgramListID listId = static_cast<Steinberg::Vst::ProgramListID>(
        info[0].As<Napi::Number>().Int32Value());
    int32_t programIndex = info[1].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::String128 name;
        std::memset(name, 0, sizeof(name));
        Steinberg::tresult r = unitInfo_->getProgramName(listId, programIndex, name);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IUnitInfo::getProgramName failed");
        }
        return Napi::String::New(env, string128ToUtf8(name));
    });
}

Napi::Value PluginInstance::SelectProgram(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) throwNapiError(env, ErrorCode::Unknown, "Plugin does not implement IUnitInfo");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "selectProgram(unitId, programId) requires two numbers");
    }
    Steinberg::Vst::UnitID unitId = static_cast<Steinberg::Vst::UnitID>(
        info[0].As<Napi::Number>().Int32Value());
    int32_t programId = info[1].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        // Per the VST3 convention: selectUnit(unitId) first so the plugin
        // routes the program change to the right unit.
        Steinberg::tresult r = unitInfo_->selectUnit(unitId);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IUnitInfo::selectUnit failed");
        }
        // IUnitInfo does not provide a selectProgram method. The VST3-correct
        // way to switch programs is to set the parameter tagged with
        // ParameterInfo::kIsProgramChange (normalized as programIndex/stepCount).
        // Iterate the controller's parameters to find one with that flag.
        if (controller_) {
            int32_t paramCount = controller_->getParameterCount();
            for (int32_t i = 0; i < paramCount; ++i) {
                Steinberg::Vst::ParameterInfo pi;
                std::memset(&pi, 0, sizeof(pi));
                if (controller_->getParameterInfo(i, pi) != Steinberg::kResultTrue)
                    continue;
                if ((pi.flags & Steinberg::Vst::ParameterInfo::kIsProgramChange) == 0)
                    continue;
                // stepCount is the number of steps minus one (0 = continuous).
                // For a program-change parameter it must be >= 1.
                double stepCount = pi.stepCount > 0 ? static_cast<double>(pi.stepCount) : 1.0;
                double normalized = static_cast<double>(programId) / stepCount;
                if (normalized < 0.0) normalized = 0.0;
                if (normalized > 1.0) normalized = 1.0;
                controller_->setParamNormalized(pi.id, normalized);
                // Mirror the change into the input parameter queue so the next
                // process() call delivers it to the audio processor.
                onPerformEdit(pi.id, normalized);
                return env.Undefined();
            }
        }
        // No kIsProgramChange parameter found: nothing more we can do. The
        // unit selection above still took effect.
        return env.Undefined();
    });
}

Napi::Value PluginInstance::GetCurrentUnit(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, static_cast<double>(unitInfo_->getSelectedUnit()));
}

Napi::Value PluginInstance::GetUnitByBusInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitInfo_) return env.Null();
    if (info.Length() < 1 || !info[0].IsObject()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getUnitByBusInfo({mediaType, direction, busIndex}) requires an object");
    }
    Napi::Object o = info[0].As<Napi::Object>();
    if (!o.Has("mediaType") || !o.Has("direction") || !o.Has("busIndex")) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getUnitByBusInfo requires {mediaType, direction, busIndex}");
    }
    Steinberg::Vst::MediaType mediaType = static_cast<Steinberg::Vst::MediaType>(
        o.Get("mediaType").As<Napi::Number>().Int32Value());
    Steinberg::Vst::BusDirection direction = static_cast<Steinberg::Vst::BusDirection>(
        o.Get("direction").As<Napi::Number>().Int32Value());
    int32_t busIndex = o.Get("busIndex").As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::UnitID unitId = 0;
        // SDK signature: getUnitByBus(MediaType, BusDirection, busIndex,
        // channel, UnitID&). We pass channel = -1 (all channels) since the
        // JS API does not currently expose a per-channel query.
        Steinberg::tresult r = unitInfo_->getUnitByBus(mediaType, direction, busIndex, -1, unitId);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            return env.Null();
        }
        return Napi::Number::New(env, static_cast<double>(unitId));
    });
}

//------------------------------------------------------------------------
// IProgramListData / IUnitData — bulk data persistence
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetProgramData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!programListData_) {
        throwNapiError(env, ErrorCode::Unknown,
                       "Plugin does not implement IProgramListData");
    }
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getProgramData(listId, programIndex) requires two numbers");
    }
    Steinberg::Vst::ProgramListID listId = static_cast<Steinberg::Vst::ProgramListID>(
        info[0].As<Napi::Number>().Int32Value());
    int32_t programIndex = info[1].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        BufferStream stream;
        Steinberg::tresult r = programListData_->getProgramData(listId, programIndex, &stream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IProgramListData::getProgramData failed");
        }
        auto bytes = stream.takeBuffer();
        return Napi::Buffer<uint8_t>::Copy(env, bytes.data(), bytes.size());
    });
}

Napi::Value PluginInstance::SetProgramData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!programListData_) {
        throwNapiError(env, ErrorCode::Unknown,
                       "Plugin does not implement IProgramListData");
    }
    if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsBuffer()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setProgramData(listId, programIndex, buffer) requires (number, number, Buffer)");
    }
    Steinberg::Vst::ProgramListID listId = static_cast<Steinberg::Vst::ProgramListID>(
        info[0].As<Napi::Number>().Int32Value());
    int32_t programIndex = info[1].As<Napi::Number>().Int32Value();
    auto buf = info[2].As<Napi::Buffer<uint8_t>>();
    return translateExceptions(env, [&]() -> Napi::Value {
        BufferStream stream(buf.Data(), buf.Length());
        Steinberg::tresult r = programListData_->setProgramData(listId, programIndex, &stream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IProgramListData::setProgramData failed");
        }
        return env.Undefined();
    });
}

Napi::Value PluginInstance::GetUnitData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitData_) {
        throwNapiError(env, ErrorCode::Unknown,
                       "Plugin does not implement IUnitData");
    }
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getUnitData(unitId) requires a number");
    }
    Steinberg::Vst::UnitID unitId = static_cast<Steinberg::Vst::UnitID>(
        info[0].As<Napi::Number>().Int32Value());
    return translateExceptions(env, [&]() -> Napi::Value {
        BufferStream stream;
        Steinberg::tresult r = unitData_->getUnitData(unitId, &stream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IUnitData::getUnitData failed");
        }
        auto bytes = stream.takeBuffer();
        return Napi::Buffer<uint8_t>::Copy(env, bytes.data(), bytes.size());
    });
}

Napi::Value PluginInstance::SetUnitData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!unitData_) {
        throwNapiError(env, ErrorCode::Unknown,
                       "Plugin does not implement IUnitData");
    }
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsBuffer()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setUnitData(unitId, buffer) requires (number, Buffer)");
    }
    Steinberg::Vst::UnitID unitId = static_cast<Steinberg::Vst::UnitID>(
        info[0].As<Napi::Number>().Int32Value());
    auto buf = info[1].As<Napi::Buffer<uint8_t>>();
    return translateExceptions(env, [&]() -> Napi::Value {
        BufferStream stream(buf.Data(), buf.Length());
        Steinberg::tresult r = unitData_->setUnitData(unitId, &stream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IUnitData::setUnitData failed");
        }
        return env.Undefined();
    });
}

//------------------------------------------------------------------------
// INoteExpressionController — per-note expression events
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetNoteExpressionCount(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!noteExpr_) return Napi::Number::New(env, 0);
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getNoteExpressionCount(busIndex, channel) requires two numbers");
    }
    int32_t busIndex = info[0].As<Napi::Number>().Int32Value();
    int16_t channel = static_cast<int16_t>(info[1].As<Napi::Number>().Int32Value());
    return Napi::Number::New(env, noteExpr_->getNoteExpressionCount(busIndex, channel));
}

Napi::Value PluginInstance::GetNoteExpressionInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!noteExpr_) throwNapiError(env, ErrorCode::Unknown, "Plugin does not implement INoteExpressionController");
    if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getNoteExpressionInfo(busIndex, channel, index) requires three numbers");
    }
    int32_t busIndex = info[0].As<Napi::Number>().Int32Value();
    int16_t channel = static_cast<int16_t>(info[1].As<Napi::Number>().Int32Value());
    int32_t index = info[2].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::NoteExpressionTypeInfo ne;
        std::memset(&ne, 0, sizeof(ne));
        Steinberg::tresult r = noteExpr_->getNoteExpressionInfo(busIndex, channel, index, ne);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "INoteExpressionController::getNoteExpressionInfo failed");
        }
        Napi::Object o = Napi::Object::New(env);
        o.Set("typeId", Napi::Number::New(env, static_cast<double>(ne.typeId)));
        o.Set("title", Napi::String::New(env, string128ToUtf8(ne.title)));
        o.Set("shortTitle", Napi::String::New(env, string128ToUtf8(ne.shortTitle)));
        o.Set("unitId", Napi::Number::New(env, static_cast<double>(ne.unitId)));
        o.Set("associatedParameterId",
              Napi::Number::New(env, static_cast<double>(
                  static_cast<int32_t>(ne.associatedParameterId))));
        o.Set("flags", Napi::Number::New(env, static_cast<double>(ne.flags)));
        return o;
    });
}

Napi::Value PluginInstance::AddNoteExpressionEvent(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 1 || !info[0].IsObject()) {
        throwNapiError(env, ErrorCode::MidiError,
                       "addNoteExpressionEvent({noteId, typeId, value, sampleOffset?}) requires an object");
    }
    Napi::Object o = info[0].As<Napi::Object>();
    if (!o.Has("noteId") || !o.Has("typeId") || !o.Has("value")) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "addNoteExpressionEvent requires {noteId, typeId, value, sampleOffset?}");
    }
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::Event e;
        std::memset(&e, 0, sizeof(e));
        e.type = Steinberg::Vst::Event::kNoteExpressionValueEvent;
        e.noteExpressionValue.noteId =
            o.Get("noteId").As<Napi::Number>().Int32Value();
        e.noteExpressionValue.typeId =
            o.Get("typeId").As<Napi::Number>().Int32Value();
        e.noteExpressionValue.value =
            static_cast<Steinberg::Vst::NoteExpressionValue>(
                o.Get("value").As<Napi::Number>().DoubleValue());
        e.sampleOffset = (o.Has("sampleOffset") && o.Get("sampleOffset").IsNumber())
            ? o.Get("sampleOffset").As<Napi::Number>().Int32Value() : 0;
        // Event::kIsLive is cleared when the process mode is not realtime so
        // plugins don't treat queued events as live player input.
        if (processMode_ == static_cast<int32_t>(Steinberg::Vst::kRealtime)) {
            e.flags |= Steinberg::Vst::Event::kIsLive;
        }
        inputEvents_.addEvent(e);
        return env.Undefined();
    });
}

//------------------------------------------------------------------------
// IKeyswitchController — static keyswitch enumeration
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetKeyswitchCount(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!keyswitchCtrl_) return Napi::Number::New(env, 0);
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getKeyswitchCount(busIndex, channel) requires two numbers");
    }
    int32_t busIndex = info[0].As<Napi::Number>().Int32Value();
    int16_t channel = static_cast<int16_t>(info[1].As<Napi::Number>().Int32Value());
    return Napi::Number::New(env, keyswitchCtrl_->getKeyswitchCount(busIndex, channel));
}

Napi::Value PluginInstance::GetKeyswitchInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!keyswitchCtrl_) {
        throwNapiError(env, ErrorCode::Unknown,
                       "Plugin does not implement IKeyswitchController");
    }
    if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getKeyswitchInfo(busIndex, channel, index) requires three numbers");
    }
    int32_t busIndex = info[0].As<Napi::Number>().Int32Value();
    int16_t channel = static_cast<int16_t>(info[1].As<Napi::Number>().Int32Value());
    int32_t index = info[2].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::KeyswitchInfo ks;
        std::memset(&ks, 0, sizeof(ks));
        Steinberg::tresult r = keyswitchCtrl_->getKeyswitchInfo(busIndex, channel, index, ks);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IKeyswitchController::getKeyswitchInfo failed");
        }
        Napi::Object o = Napi::Object::New(env);
        // KeyswitchInfo fields (per ivstnoteexpression.h): typeId, title,
        // shortTitle, keyswitchMin, keyswitchMax, keyRemapped, unitId, flags.
        // Expose the subset most useful to JS callers; `typeId` maps to the
        // user-facing `keyswitchType` field name.
        o.Set("keyswitchType", Napi::Number::New(env, static_cast<double>(ks.typeId)));
        o.Set("name", Napi::String::New(env, string128ToUtf8(ks.title)));
        o.Set("shortName", Napi::String::New(env, string128ToUtf8(ks.shortTitle)));
        o.Set("keyswitchMin", Napi::Number::New(env, static_cast<double>(ks.keyswitchMin)));
        o.Set("keyswitchMax", Napi::Number::New(env, static_cast<double>(ks.keyswitchMax)));
        o.Set("keyRemapped", Napi::Number::New(env, static_cast<double>(ks.keyRemapped)));
        o.Set("unitId", Napi::Number::New(env, static_cast<double>(ks.unitId)));
        o.Set("flags", Napi::Number::New(env, static_cast<double>(ks.flags)));
        return o;
    });
}

//------------------------------------------------------------------------
// Runtime bus management
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetBusList(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getBusList(mediaType, direction) requires two numbers");
    }
    Steinberg::Vst::MediaType mediaType = static_cast<Steinberg::Vst::MediaType>(
        info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::BusDirection direction = static_cast<Steinberg::Vst::BusDirection>(
        info[1].As<Napi::Number>().Int32Value());
    Steinberg::int32 count = component_->getBusCount(mediaType, direction);
    Napi::Array arr = Napi::Array::New(env, static_cast<size_t>(count));
    for (Steinberg::int32 i = 0; i < count; ++i) {
        // Reuse GetBusInfo by calling it directly via a synthetic CallbackInfo
        // is not trivial; instead, build the BusInfo object inline here.
        Steinberg::Vst::BusInfo bi;
        std::memset(&bi, 0, sizeof(bi));
        if (component_->getBusInfo(mediaType, direction, i, bi) != Steinberg::kResultTrue) {
            continue;
        }
        Napi::Object o = Napi::Object::New(env);
        o.Set("mediaType", Napi::Number::New(env, static_cast<double>(mediaType)));
        o.Set("direction", Napi::Number::New(env, static_cast<double>(direction)));
        o.Set("busIndex", Napi::Number::New(env, static_cast<double>(i)));
        o.Set("name", Napi::String::New(env, string128ToUtf8(bi.name)));
        o.Set("channelCount", Napi::Number::New(env, static_cast<double>(bi.channelCount)));
        o.Set("busType", Napi::Number::New(env, static_cast<double>(bi.busType)));
        o.Set("flags", Napi::Number::New(env, static_cast<double>(bi.flags)));
        // Active state: for audio buses, reflect our cached inputBusInfos_/
        // outputBusInfos_ state. For event buses, default to true.
        bool isActive = true;
        if (mediaType == Steinberg::Vst::kAudio) {
            const auto& vec = (direction == Steinberg::Vst::kInput)
                ? inputBusInfos_ : outputBusInfos_;
            if (static_cast<size_t>(i) < vec.size()) {
                isActive = vec[static_cast<size_t>(i)].isActive;
            }
        }
        o.Set("active", Napi::Boolean::New(env, isActive));
        // Speaker arrangement: only meaningful for audio buses; query the
        // current arrangement via audioProcessor_->getBusArrangement.
        Steinberg::Vst::SpeakerArrangement spkArr = 0;
        if (mediaType == Steinberg::Vst::kAudio && audioProcessor_ &&
            audioProcessor_->getBusArrangement(direction, i, spkArr) == Steinberg::kResultTrue) {
            o.Set("speakerArrangement", Napi::Number::New(env, static_cast<double>(spkArr)));
        } else {
            o.Set("speakerArrangement", Napi::Number::New(env, 0));
        }
        arr[static_cast<uint32_t>(i)] = o;
    }
    return arr;
}

Napi::Value PluginInstance::GetBusInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getBusInfo(mediaType, direction, busIndex) requires three numbers");
    }
    Steinberg::Vst::MediaType mediaType = static_cast<Steinberg::Vst::MediaType>(
        info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::BusDirection direction = static_cast<Steinberg::Vst::BusDirection>(
        info[1].As<Napi::Number>().Int32Value());
    int32_t busIndex = info[2].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::BusInfo bi;
        std::memset(&bi, 0, sizeof(bi));
        Steinberg::tresult r = component_->getBusInfo(mediaType, direction, busIndex, bi);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IComponent::getBusInfo failed");
        }
        Napi::Object o = Napi::Object::New(env);
        o.Set("mediaType", Napi::Number::New(env, static_cast<double>(mediaType)));
        o.Set("direction", Napi::Number::New(env, static_cast<double>(direction)));
        o.Set("busIndex", Napi::Number::New(env, static_cast<double>(busIndex)));
        o.Set("name", Napi::String::New(env, string128ToUtf8(bi.name)));
        o.Set("channelCount", Napi::Number::New(env, static_cast<double>(bi.channelCount)));
        o.Set("busType", Napi::Number::New(env, static_cast<double>(bi.busType)));
        o.Set("flags", Napi::Number::New(env, static_cast<double>(bi.flags)));
        // Active state: for audio buses, reflect our cached bus-info state.
        bool isActive = true;
        if (mediaType == Steinberg::Vst::kAudio) {
            const auto& vec = (direction == Steinberg::Vst::kInput)
                ? inputBusInfos_ : outputBusInfos_;
            if (static_cast<size_t>(busIndex) < vec.size()) {
                isActive = vec[static_cast<size_t>(busIndex)].isActive;
            }
        }
        o.Set("active", Napi::Boolean::New(env, isActive));
        // Speaker arrangement: only meaningful for audio buses.
        Steinberg::Vst::SpeakerArrangement arr = 0;
        if (mediaType == Steinberg::Vst::kAudio && audioProcessor_ &&
            audioProcessor_->getBusArrangement(direction, busIndex, arr) == Steinberg::kResultTrue) {
            o.Set("speakerArrangement", Napi::Number::New(env, static_cast<double>(arr)));
        } else {
            o.Set("speakerArrangement", Napi::Number::New(env, 0));
        }
        return o;
    });
}

Napi::Value PluginInstance::ActivateBus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber()
        || !info[2].IsNumber() || !info[3].IsBoolean()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "activateBus(mediaType, direction, busIndex, active) requires (number, number, number, boolean)");
    }
    if (active_) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "Bus state may only change while inactive; call setActive(false) first");
    }
    Steinberg::Vst::MediaType mediaType = static_cast<Steinberg::Vst::MediaType>(
        info[0].As<Napi::Number>().Int32Value());
    Steinberg::Vst::BusDirection direction = static_cast<Steinberg::Vst::BusDirection>(
        info[1].As<Napi::Number>().Int32Value());
    int32_t busIndex = info[2].As<Napi::Number>().Int32Value();
    bool active = info[3].As<Napi::Boolean>().Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::tresult r = component_->activateBus(mediaType, direction, busIndex, active);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IComponent::activateBus failed");
        }
        // Update cached bus-info state for audio buses so subsequent
        // getBusInfo / getBusList calls reflect the new activation.
        if (mediaType == Steinberg::Vst::kAudio) {
            auto& vec = (direction == Steinberg::Vst::kInput)
                ? inputBusInfos_ : outputBusInfos_;
            if (static_cast<size_t>(busIndex) < vec.size()) {
                vec[static_cast<size_t>(busIndex)].isActive = active;
            }
        }
        return env.Undefined();
    });
}

//------------------------------------------------------------------------
// Speaker arrangement
//------------------------------------------------------------------------
Napi::Value PluginInstance::SetBusArrangement(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!audioProcessor_) throwNapiError(env, ErrorCode::Unknown, "No audio processor");
    if (info.Length() < 2 || !info[0].IsArray() || !info[1].IsArray()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setBusArrangement(inputs, outputs) requires two arrays of SpeakerArrangement numbers");
    }
    Napi::Array inArr = info[0].As<Napi::Array>();
    Napi::Array outArr = info[1].As<Napi::Array>();
    std::vector<Steinberg::Vst::SpeakerArrangement> inVec;
    std::vector<Steinberg::Vst::SpeakerArrangement> outVec;
    inVec.reserve(inArr.Length());
    outVec.reserve(outArr.Length());
    for (uint32_t i = 0; i < inArr.Length(); ++i) {
        Napi::Value v = inArr[i];
        if (!v.IsNumber()) {
            throwNapiError(env, ErrorCode::InvalidParameter,
                           "setBusArrangement: inputs must be an array of numbers");
        }
        inVec.push_back(static_cast<Steinberg::Vst::SpeakerArrangement>(
            v.As<Napi::Number>().Int32Value()));
    }
    for (uint32_t i = 0; i < outArr.Length(); ++i) {
        Napi::Value v = outArr[i];
        if (!v.IsNumber()) {
            throwNapiError(env, ErrorCode::InvalidParameter,
                           "setBusArrangement: outputs must be an array of numbers");
        }
        outVec.push_back(static_cast<Steinberg::Vst::SpeakerArrangement>(
            v.As<Napi::Number>().Int32Value()));
    }
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::tresult r = audioProcessor_->setBusArrangements(
            inVec.data(), static_cast<Steinberg::int32>(inVec.size()),
            outVec.data(), static_cast<Steinberg::int32>(outVec.size()));
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            return Napi::Boolean::New(env, false);
        }
        // Refresh cached bus info (channel counts + arrangements) for inputs
        // and outputs so subsequent getBusInfo / process calls see the new
        // layout. The plugin may have switched bus channel counts.
        for (size_t i = 0; i < inputBusInfos_.size(); ++i) {
            Steinberg::Vst::BusInfo bi;
            if (component_->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kInput,
                                        static_cast<Steinberg::int32>(i), bi) == Steinberg::kResultTrue) {
                inputBusInfos_[i].channelCount = bi.channelCount;
            }
            Steinberg::Vst::SpeakerArrangement arr = 0;
            if (audioProcessor_->getBusArrangement(Steinberg::Vst::kInput,
                                                    static_cast<Steinberg::int32>(i), arr) == Steinberg::kResultTrue) {
                inputBusInfos_[i].arrangement = arr;
            }
        }
        for (size_t i = 0; i < outputBusInfos_.size(); ++i) {
            Steinberg::Vst::BusInfo bi;
            if (component_->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput,
                                        static_cast<Steinberg::int32>(i), bi) == Steinberg::kResultTrue) {
                outputBusInfos_[i].channelCount = bi.channelCount;
            }
            Steinberg::Vst::SpeakerArrangement arr = 0;
            if (audioProcessor_->getBusArrangement(Steinberg::Vst::kOutput,
                                                    static_cast<Steinberg::int32>(i), arr) == Steinberg::kResultTrue) {
                outputBusInfos_[i].arrangement = arr;
            }
        }
        return Napi::Boolean::New(env, true);
    });
}

Napi::Value PluginInstance::GetBusArrangement(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!audioProcessor_) throwNapiError(env, ErrorCode::Unknown, "No audio processor");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getBusArrangement(direction, busIndex) requires two numbers");
    }
    Steinberg::Vst::BusDirection direction = static_cast<Steinberg::Vst::BusDirection>(
        info[0].As<Napi::Number>().Int32Value());
    int32_t busIndex = info[1].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        Steinberg::Vst::SpeakerArrangement arr = 0;
        Steinberg::tresult r = audioProcessor_->getBusArrangement(direction, busIndex, arr);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::InvalidParameter, "IAudioProcessor::getBusArrangement failed");
        }
        return Napi::Number::New(env, static_cast<double>(arr));
    });
}

//------------------------------------------------------------------------
// Routing info
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetRoutingInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "getRoutingInfo(srcBus, dstBus) requires two numbers");
    }
    int32_t srcBus = info[0].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        // Construct RoutingInfo for the source bus. The SDK RoutingInfo struct
        // has only {mediaType, busIndex, channel} — there is no busType field
        // on the input side. We default to kAudio which is the typical case
        // for routing queries.
        Steinberg::Vst::RoutingInfo inInfo;
        std::memset(&inInfo, 0, sizeof(inInfo));
        inInfo.busIndex = srcBus;
        inInfo.mediaType = Steinberg::Vst::kAudio;
        inInfo.channel = -1;
        Steinberg::Vst::RoutingInfo outInfo;
        std::memset(&outInfo, 0, sizeof(outInfo));
        Steinberg::tresult r = component_->getRoutingInfo(inInfo, outInfo);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            return env.Null();
        }
        // The SDK RoutingInfo result exposes {mediaType, busIndex, channel};
        // there is no busType on the result side. We expose those three under
        // the JS field names {busMediaType, dstBus, channel}.
        Napi::Object o = Napi::Object::New(env);
        o.Set("srcBus", Napi::Number::New(env, static_cast<double>(inInfo.busIndex)));
        o.Set("dstBus", Napi::Number::New(env, static_cast<double>(outInfo.busIndex)));
        o.Set("busMediaType", Napi::Number::New(env, static_cast<double>(outInfo.mediaType)));
        o.Set("channel", Napi::Number::New(env, static_cast<double>(outInfo.channel)));
        return o;
    });
}

//------------------------------------------------------------------------
// Configurable ProcessContext — setProcessContext / getProcessContext
//------------------------------------------------------------------------
// SetProcessContext: writes user-supplied fields into processContext_ and
// updates the corresponding state validity bits. Each field is optional;
// only fields present in the JS object are applied (existing values are
// preserved otherwise). State bits are OR'd in for value fields and
// set/cleared for boolean fields.
Napi::Value PluginInstance::SetProcessContext(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 1 || !info[0].IsObject() || info[0].IsNull()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setProcessContext(opts) requires an object");
    }
    Napi::Object o = info[0].As<Napi::Object>();
    return translateExceptions(env, [&]() -> Napi::Value {
        auto readBool = [&](const char* key, bool* out) -> bool {
            if (o.Has(key) && o.Get(key).IsBoolean()) {
                *out = o.Get(key).As<Napi::Boolean>().Value();
                return true;
            }
            return false;
        };
        auto readNumber = [&](const char* key, double* out) -> bool {
            if (o.Has(key) && o.Get(key).IsNumber()) {
                *out = o.Get(key).As<Napi::Number>().DoubleValue();
                return true;
            }
            return false;
        };
        auto readInt32 = [&](const char* key, int32_t* out) -> bool {
            if (o.Has(key) && o.Get(key).IsNumber()) {
                *out = o.Get(key).As<Napi::Number>().Int32Value();
                return true;
            }
            return false;
        };
        auto readInt64 = [&](const char* key, int64_t* out) -> bool {
            if (o.Has(key) && o.Get(key).IsNumber()) {
                // JS Number is a double; cast to int64_t. Sufficient for
                // realistic sample positions and nanosecond timestamps.
                *out = static_cast<int64_t>(o.Get(key).As<Napi::Number>().Int64Value());
                return true;
            }
            return false;
        };

        // tempo (BPM, double)
        double tempo;
        if (readNumber("tempo", &tempo)) {
            processContext_.tempo = tempo;
            processContext_.state |= Steinberg::Vst::ProcessContext::kTempoValid;
            // projectTimeMusic advances correctly only when kProjectTimeMusicValid
            // is set; tempo alone doesn't grant that, but the VST3 convention is
            // that a host supplying a tempo also supplies music time. Set it.
            processContext_.state |= Steinberg::Vst::ProcessContext::kProjectTimeMusicValid;
        }
        // Time signature: numerator + denominator. The spec says setting one
        // should set both (the host needs both for any meaningful display).
        int32_t num = 0, den = 0;
        bool setNum = readInt32("timeSigNumerator", &num);
        bool setDen = readInt32("timeSigDenominator", &den);
        if (setNum || setDen) {
            if (setNum) processContext_.timeSigNumerator = num;
            if (setDen) processContext_.timeSigDenominator = den > 0 ? den : 4;
            processContext_.state |= Steinberg::Vst::ProcessContext::kTimeSigValid;
        }
        // samplePosition (int64 — projectTimeSamples)
        int64_t sp = 0;
        if (readInt64("samplePosition", &sp)) {
            processContext_.projectTimeSamples = sp;
        }
        // barPositionMusic (double)
        double barPos = 0;
        if (readNumber("barPositionMusic", &barPos)) {
            processContext_.barPositionMusic = barPos;
            processContext_.state |= Steinberg::Vst::ProcessContext::kBarPositionValid;
        }
        // samplesToNextClock (int32)
        int32_t snc = 0;
        if (readInt32("samplesToNextClock", &snc)) {
            processContext_.samplesToNextClock = snc;
        }
        // playing (boolean) — set/clear kPlaying bit
        bool playing = false;
        if (readBool("playing", &playing)) {
            if (playing)
                processContext_.state |= Steinberg::Vst::ProcessContext::kPlaying;
            else
                processContext_.state &= ~Steinberg::Vst::ProcessContext::kPlaying;
        }
        // cycleActive (boolean) — set/clear kCycleActive bit
        bool cycle = false;
        if (readBool("cycleActive", &cycle)) {
            if (cycle)
                processContext_.state |= Steinberg::Vst::ProcessContext::kCycleActive;
            else
                processContext_.state &= ~Steinberg::Vst::ProcessContext::kCycleActive;
        }
        // recording (boolean) — set/clear kRecording bit
        bool rec = false;
        if (readBool("recording", &rec)) {
            if (rec)
                processContext_.state |= Steinberg::Vst::ProcessContext::kRecording;
            else
                processContext_.state &= ~Steinberg::Vst::ProcessContext::kRecording;
        }
        // systemTime (int64 — nanoseconds since epoch)
        int64_t sysTime = 0;
        if (readInt64("systemTime", &sysTime)) {
            processContext_.systemTime = sysTime;
            processContext_.state |= Steinberg::Vst::ProcessContext::kSystemTimeValid;
        }
        // continuousTimeSamples (int64) — note: SDK spells "continous" (one 'u')
        int64_t ct = 0;
        if (readInt64("continuousTimeSamples", &ct)) {
            processContext_.continousTimeSamples = ct;
            processContext_.state |= Steinberg::Vst::ProcessContext::kContTimeValid;
        }
        return env.Undefined();
    });
}

// GetProcessContext: returns a JS object snapshot of the current
// ProcessContext. Includes all configurable fields plus the `state` bitmask
// so callers can inspect which fields are currently valid. This is a
// user-callable query (not on the audio thread); allocating a JS object here
// is fine.
Napi::Value PluginInstance::GetProcessContext(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    Napi::Object o = Napi::Object::New(env);
    o.Set("tempo", Napi::Number::New(env, processContext_.tempo));
    o.Set("timeSigNumerator", Napi::Number::New(env, processContext_.timeSigNumerator));
    o.Set("timeSigDenominator", Napi::Number::New(env, processContext_.timeSigDenominator));
    o.Set("samplePosition", Napi::Number::New(env, static_cast<double>(processContext_.projectTimeSamples)));
    o.Set("barPositionMusic", Napi::Number::New(env, processContext_.barPositionMusic));
    o.Set("samplesToNextClock", Napi::Number::New(env, processContext_.samplesToNextClock));
    o.Set("playing", Napi::Boolean::New(env,
        (processContext_.state & Steinberg::Vst::ProcessContext::kPlaying) != 0));
    o.Set("cycleActive", Napi::Boolean::New(env,
        (processContext_.state & Steinberg::Vst::ProcessContext::kCycleActive) != 0));
    o.Set("recording", Napi::Boolean::New(env,
        (processContext_.state & Steinberg::Vst::ProcessContext::kRecording) != 0));
    o.Set("systemTime", Napi::Number::New(env, static_cast<double>(processContext_.systemTime)));
    o.Set("continuousTimeSamples", Napi::Number::New(env, static_cast<double>(processContext_.continousTimeSamples)));
    // Also expose the raw state bitmask so users can see which fields are
    // currently flagged valid.
    o.Set("state", Napi::Number::New(env, static_cast<double>(processContext_.state)));
    return o;
}

//------------------------------------------------------------------------
// IProcessContextRequirements — getProcessContextRequirements
//------------------------------------------------------------------------
Napi::Value PluginInstance::GetProcessContextRequirements(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!processContextReqs_) return Napi::Number::New(env, 0);
    uint32_t mask = processContextReqs_->getProcessContextRequirements();
    return Napi::Number::New(env, static_cast<double>(mask));
}

//------------------------------------------------------------------------
// IAudioPresentationLatency — setAudioPresentationLatency
//------------------------------------------------------------------------
Napi::Value PluginInstance::SetAudioPresentationLatency(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!audioPresLatency_) {
        // Plugin doesn't implement the interface — no-op, report false.
        return Napi::Boolean::New(env, false);
    }
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setAudioPresentationLatency(busIndex, latencySamples) requires two numbers");
    }
    int32_t busIndex = info[0].As<Napi::Number>().Int32Value();
    uint32_t latencySamples = info[1].As<Napi::Number>().Uint32Value();
    // IAudioPresentationLatency::setAudioPresentationLatencySamples takes a
    // BusDirection (kOutput, since this is the host→plugin presentation
    // latency for output buses) + busIndex + latencyInSamples.
    Steinberg::tresult r = audioPresLatency_->setAudioPresentationLatencySamples(
        Steinberg::Vst::kOutput, busIndex, latencySamples);
    return Napi::Boolean::New(env, r == Steinberg::kResultTrue || r == Steinberg::kResultOk);
}

//------------------------------------------------------------------------
// IInfoListener — setChannelContextInfo
//------------------------------------------------------------------------
Napi::Value PluginInstance::SetChannelContextInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!infoListener_) {
        return Napi::Boolean::New(env, false);
    }
    if (info.Length() < 1 || !info[0].IsObject() || info[0].IsNull()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setChannelContextInfo(info) requires an object");
    }
    Napi::Object o = info[0].As<Napi::Object>();
    return translateExceptions(env, [&]() -> Napi::Value {
        // Build an IAttributeList with the known channel-context keys (the
        // SDK exposes these under the Steinberg::Vst::ChannelContext
        // namespace in ivstchannelcontextinfo.h). The plugin's
        // IInfoListener::setChannelContextInfos receives this list and pulls
        // out the keys it cares about. We use the SDK's HostAttributeList
        // helper (a ready-to-use IAttributeList implementation) instead of
        // asking the host application to create one.
        Steinberg::IPtr<Steinberg::Vst::IAttributeList> list(
            Steinberg::Vst::HostAttributeList::make());
        if (!list) {
            throwNst(ErrorCode::Unknown, "Failed to create IAttributeList");
        }

        // channelIdx (int32)
        if (o.Has("channelIdx") && o.Get("channelIdx").IsNumber()) {
            int32_t idx = o.Get("channelIdx").As<Napi::Number>().Int32Value();
            list->setInt(Steinberg::Vst::ChannelContext::kChannelIndexKey,
                         static_cast<Steinberg::int64>(idx));
        }
        // trackName / channelName (String128)
        if (o.Has("trackName") && o.Get("trackName").IsString()) {
            std::string s = o.Get("trackName").As<Napi::String>().Utf8Value();
            Steinberg::Vst::String128 s128;
            utf8ToString128(s, s128);
            list->setString(Steinberg::Vst::ChannelContext::kChannelNameKey,
                            s128);
        }
        // namespaceName (String128) — SDK key is kChannelIndexNamespaceKey.
        if (o.Has("namespaceName") && o.Get("namespaceName").IsString()) {
            std::string s = o.Get("namespaceName").As<Napi::String>().Utf8Value();
            Steinberg::Vst::String128 s128;
            utf8ToString128(s, s128);
            list->setString(Steinberg::Vst::ChannelContext::kChannelIndexNamespaceKey,
                            s128);
        }
        // channelColor (uint32 packed ARGB)
        if (o.Has("channelColor") && o.Get("channelColor").IsNumber()) {
            uint32_t color = o.Get("channelColor").As<Napi::Number>().Uint32Value();
            list->setInt(Steinberg::Vst::ChannelContext::kChannelColorKey,
                         static_cast<Steinberg::int64>(color));
        }

        Steinberg::tresult r = infoListener_->setChannelContextInfos(list);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            return Napi::Boolean::New(env, false);
        }
        return Napi::Boolean::New(env, true);
    });
}

//------------------------------------------------------------------------
// IPrefetchableSupport — isPrefetchable
//------------------------------------------------------------------------
Napi::Value PluginInstance::IsPrefetchable(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!prefetchable_) {
        // Plugin doesn't implement the interface — conservatively false.
        return Napi::Boolean::New(env, false);
    }
    // IPrefetchableSupport::getPrefetchableSupport writes a PrefetchableSupport
    // enum value (kIsNeverPrefetchable / kIsYetPrefetchable / kIsNotYetPrefetchable)
    // into the out parameter and returns a tresult. Treat kIsYetPrefetchable as
    // prefetchable; everything else as not prefetchable.
    Steinberg::Vst::PrefetchableSupport prefetch = Steinberg::Vst::kIsNeverPrefetchable;
    Steinberg::tresult r = prefetchable_->getPrefetchableSupport(prefetch);
    if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
        return Napi::Boolean::New(env, false);
    }
    return Napi::Boolean::New(env, prefetch == Steinberg::Vst::kIsYetPrefetchable);
}

//------------------------------------------------------------------------
// IEditController2 — setKnobMode (host→plugin)
//------------------------------------------------------------------------
// setKnobMode forwards the host's preferred knob interaction mode to the
// plugin's IEditController2. The mode is an int32 matching the SDK's
// IEditController2::KnobMode enum:
//   0 = kCircularMode (absolute circular)
//   1 = kRelativeCircularMode (relative circular)
//   2 = kLinearMode (linear / vertical drag)
// Returns `true` if the plugin implements IEditController2 and accepted the
// mode; `false` if the plugin does not implement IEditController2.
Napi::Value PluginInstance::SetKnobMode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!editController2_) {
        // Plugin doesn't implement IEditController2 — no-op, report false.
        return Napi::Boolean::New(env, false);
    }
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setKnobMode(mode) requires a number (0=circular, 1=relative circular, 2=linear)");
    }
    int32_t mode = info[0].As<Napi::Number>().Int32Value();
    Steinberg::tresult r = editController2_->setKnobMode(mode);
    return Napi::Boolean::New(env, r == Steinberg::kResultTrue || r == Steinberg::kResultOk);
}

//------------------------------------------------------------------------
// Restart auto-react — ApplyRestartFlags / applyRestartFlags
//------------------------------------------------------------------------
// ApplyRestartFlags re-queries the affected SDK state for the given restart
// flag bitmask. It is called automatically by ComponentHandler::restartComponent
// BEFORE the JS 'restart' event fires, and is also exposed as the JS method
// applyRestartFlags(flags) for manual invocation.
//
// The implementation is conservative: for flags where the corresponding SDK
// state is read live on every query (getParameterInfo, getParameter,
// getNoteExpressionInfo, IMidiMapping), there is nothing to cache and the
// branch is a comment-only no-op. For kIoChanged we re-read the full bus
// topology and re-allocate the per-bus buffers if bus counts changed (this is
// a rare event, never on the audio thread, so allocation is acceptable).
void PluginInstance::ApplyRestartFlags(int32_t flags) {
    if (!component_ && !audioProcessor_) return;

    using RF = Steinberg::Vst::RestartFlags;

    if (flags & RF::kLatencyChanged) {
        // GetLatency reads live from IAudioProcessor::getLatencySamples on
        // every call, so there is no cached value to refresh. No-op; the
        // next getLatency() call will observe the new value automatically.
    }
    if (flags & RF::kIoChanged) {
        // Re-read all audio bus info via IComponent::getBusCount +
        // getBusInfo. If bus counts changed, re-allocate the per-bus
        // AudioBusBuffers and channel-pointer vectors (mirrors setup()).
        // This is acceptable because kIoChanged is a rare event and never
        // fires on the audio thread.
        if (component_) {
            Steinberg::int32 inAudioBuses =
                component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
            Steinberg::int32 outAudioBuses =
                component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);

            // Rebuild inputBusInfos_ / outputBusInfos_ from the live bus info.
            inputBusInfos_.clear();
            inputBusInfos_.reserve(static_cast<size_t>(inAudioBuses));
            for (Steinberg::int32 i = 0; i < inAudioBuses; ++i) {
                AudioBusInfoEntry e;
                Steinberg::Vst::BusInfo busInfo;
                if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                            Steinberg::Vst::kInput, i, busInfo)
                    == Steinberg::kResultTrue) {
                    e.channelCount = busInfo.channelCount;
                    e.isActive = (busInfo.flags & Steinberg::Vst::BusInfo::kDefaultActive) != 0;
                }
                inputBusInfos_.push_back(e);
            }
            outputBusInfos_.clear();
            outputBusInfos_.reserve(static_cast<size_t>(outAudioBuses));
            for (Steinberg::int32 i = 0; i < outAudioBuses; ++i) {
                AudioBusInfoEntry e;
                Steinberg::Vst::BusInfo busInfo;
                if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                            Steinberg::Vst::kOutput, i, busInfo)
                    == Steinberg::kResultTrue) {
                    e.channelCount = busInfo.channelCount;
                    e.isActive = (busInfo.flags & Steinberg::Vst::BusInfo::kDefaultActive) != 0;
                }
                outputBusInfos_.push_back(e);
            }

            // Re-allocate per-bus buffers and channel pointer vectors if bus
            // counts changed. Mirrors the allocation block in setup().
            if (static_cast<Steinberg::int32>(inputBuffers_.size()) != inAudioBuses ||
                static_cast<Steinberg::int32>(outputBuffers_.size()) != outAudioBuses) {
                inputBuffers_.assign(static_cast<size_t>(inAudioBuses),
                                     Steinberg::Vst::AudioBusBuffers{});
                outputBuffers_.assign(static_cast<size_t>(outAudioBuses),
                                      Steinberg::Vst::AudioBusBuffers{});
                inputChannelPtrsPerBus_.assign(static_cast<size_t>(inAudioBuses), {});
                outputChannelPtrsPerBus_.assign(static_cast<size_t>(outAudioBuses), {});
                auto assignChannelBuffers = [this](Steinberg::Vst::AudioBusBuffers& buf, void** ptrs) {
                    if (activeSampleSize_ == 64) {
                        buf.channelBuffers64 = reinterpret_cast<double**>(ptrs);
                    } else {
                        buf.channelBuffers32 = reinterpret_cast<float**>(ptrs);
                    }
                };
                for (Steinberg::int32 i = 0; i < inAudioBuses; ++i) {
                    auto& bufs = inputChannelPtrsPerBus_[static_cast<size_t>(i)];
                    bufs.assign(static_cast<size_t>(inputBusInfos_[static_cast<size_t>(i)].channelCount), nullptr);
                    inputBuffers_[static_cast<size_t>(i)].numChannels =
                        inputBusInfos_[static_cast<size_t>(i)].channelCount;
                    inputBuffers_[static_cast<size_t>(i)].silenceFlags = 0;
                    assignChannelBuffers(inputBuffers_[static_cast<size_t>(i)], bufs.data());
                }
                for (Steinberg::int32 i = 0; i < outAudioBuses; ++i) {
                    auto& bufs = outputChannelPtrsPerBus_[static_cast<size_t>(i)];
                    bufs.assign(static_cast<size_t>(outputBusInfos_[static_cast<size_t>(i)].channelCount), nullptr);
                    outputBuffers_[static_cast<size_t>(i)].numChannels =
                        outputBusInfos_[static_cast<size_t>(i)].channelCount;
                    outputBuffers_[static_cast<size_t>(i)].silenceFlags = 0;
                    assignChannelBuffers(outputBuffers_[static_cast<size_t>(i)], bufs.data());
                }
                processData_.numInputs = inAudioBuses;
                processData_.numOutputs = outAudioBuses;
                processData_.inputs = inputBuffers_.data();
                processData_.outputs = outputBuffers_.data();
            }

            // Re-run speaker-arrangement negotiation so the cached
            // arrangements match the plugin's new bus topology.
            negotiateSpeakerArrangements();

            // Refresh summed channel counts in info_ so getInfo() stays
            // consistent after an IO change.
            Steinberg::int32 inAudio = 0, outAudio = 0;
            for (const auto& b : inputBusInfos_) inAudio += b.channelCount;
            for (const auto& b : outputBusInfos_) outAudio += b.channelCount;
            info_.numAudioInputs = inAudio;
            info_.numAudioOutputs = outAudio;
            info_.numMidiInputs =
                component_->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
            info_.numMidiOutputs =
                component_->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);
        }
    }
    if (flags & RF::kMidiCCAssignmentChanged) {
        // IMidiMapping is queried per-call in tryMapMidiController (no cache),
        // so there is nothing to invalidate. No-op.
    }
    if (flags & RF::kRoutingInfoChanged) {
        // getRoutingInfo reads live from IComponent::getRoutingInfo on every
        // call; no cached routing info to invalidate. No-op.
    }
    if (flags & RF::kParamTitlesChanged) {
        // getParameterInfo reads live from the controller; no cache. No-op.
    }
    if (flags & RF::kParamValuesChanged) {
        // getParameter reads live from the controller; no cache. No-op.
    }
    if (flags & RF::kNoteExpressionChanged) {
        // getNoteExpressionInfo reads live from INoteExpressionController; no
        // cache. No-op.
    }
    if (flags & RF::kReloadComponent) {
        // A full reload would require disposing the current instance and
        // re-loading the plugin from scratch, which the host cannot do
        // in-place. No action here — the user should dispose and re-load.
        // The JS 'restart' event still fires with kReloadComponent so the
        // user can react.
    }
    if (flags & RF::kPrefetchableSupportChanged) {
        // IsPrefetchable reads live from IPrefetchableSupport on every call;
        // no cache. No-op.
    }
    if (flags & RF::kIoTitlesChanged) {
        // Bus titles are read live via getBusInfo; no cache. No-op.
    }
}

// JS-facing wrapper for ApplyRestartFlags. Accepts a single numeric flags
// argument (a bitmask of RestartFlags) and invokes the internal re-query
// path. Returns undefined.
Napi::Value PluginInstance::ApplyRestartFlagsJs(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 1 || !info[0].IsNumber()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "applyRestartFlags(flags) requires a number (RestartFlags bitmask)");
    }
    int32_t flags = info[0].As<Napi::Number>().Int32Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        ApplyRestartFlags(flags);
        return env.Undefined();
    });
}

//------------------------------------------------------------------------
// Mutable ProcessSetup — setProcessSetup
//------------------------------------------------------------------------
// SetProcessSetup updates the stored ProcessSetup fields (sampleRate,
// maxBlockSize, processMode, sampleSize) for the next setActive(true) call.
// The VST3 spec forbids changing ProcessSetup while the plugin is active;
// callers must call setActive(false) first. Throws VST3_INVALID_PARAMETER if
// the plugin is currently active.
//
// For sampleSize, the host re-probes canProcessSampleSize with the new size;
// if the plugin refuses, the host falls back to 32 (matching the load-time
// negotiation behavior) rather than throwing.
Napi::Value PluginInstance::SetProcessSetup(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (info.Length() < 1 || !info[0].IsObject() || info[0].IsNull()) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "setProcessSetup(opts) requires an object");
    }
    if (active_) {
        throwNapiError(env, ErrorCode::InvalidParameter,
                       "Cannot change ProcessSetup while active; call setActive(false) first");
    }
    Napi::Object o = info[0].As<Napi::Object>();
    return translateExceptions(env, [&]() -> Napi::Value {
        // sampleRate (number, Hz)
        if (o.Has("sampleRate") && o.Get("sampleRate").IsNumber()) {
            double sr = o.Get("sampleRate").As<Napi::Number>().DoubleValue();
            if (sr > 0.0) opts_.sampleRate = sr;
        }
        // maxBlockSize (number, samples)
        if (o.Has("maxBlockSize") && o.Get("maxBlockSize").IsNumber()) {
            int32_t mbs = o.Get("maxBlockSize").As<Napi::Number>().Int32Value();
            if (mbs > 0) opts_.maxBlockSize = mbs;
        }
        // processMode ('realtime' | 'offline' | 'prefetch')
        if (o.Has("processMode") && o.Get("processMode").IsString()) {
            std::string pm = o.Get("processMode").As<Napi::String>().Utf8Value();
            if (pm == "offline") {
                opts_.processMode = 1;
                processMode_ = 1;
            } else if (pm == "prefetch") {
                opts_.processMode = 2;
                processMode_ = 2;
            } else {
                // "realtime" or anything else → realtime
                opts_.processMode = 0;
                processMode_ = 0;
            }
        }
        // sampleSize (32 or 64). Re-probe canProcessSampleSize with the new
        // size; if the plugin refuses, fall back to 32 (do not throw — this
        // matches the load-time negotiation behavior).
        if (o.Has("sampleSize") && o.Get("sampleSize").IsNumber()) {
            int32_t requested = o.Get("sampleSize").As<Napi::Number>().Int32Value();
            if (requested == 64) {
                if (audioProcessor_ &&
                    audioProcessor_->canProcessSampleSize(Steinberg::Vst::kSample64)
                        == Steinberg::kResultTrue) {
                    opts_.sampleSize = 64;
                    activeSampleSize_ = 64;
                } else {
                    // Plugin refuses 64-bit; fall back to 32 silently.
                    opts_.sampleSize = 32;
                    activeSampleSize_ = 32;
                }
            } else if (requested == 32) {
                opts_.sampleSize = 32;
                activeSampleSize_ = 32;
            }
            // Other values are ignored (no change).
        }

        // Update processData_ so the next process() call sees the new
        // processMode and symbolicSampleSize. setupProcessing() is invoked
        // by the next setActive(true), which propagates the new sample rate
        // and max block size to the plugin.
        processData_.processMode = static_cast<Steinberg::Vst::ProcessModes>(processMode_);
        processData_.symbolicSampleSize = (activeSampleSize_ == 64)
            ? Steinberg::Vst::kSample64 : Steinberg::Vst::kSample32;

        // Keep the ProcessContext sample rate in sync with opts_.sampleRate
        // so plugins that read it during process() see the new rate.
        processContext_.sampleRate = opts_.sampleRate;

        return env.Undefined();
    });
}

//------------------------------------------------------------------------
// on('restart', cb) — and other plugin→host events
//------------------------------------------------------------------------
// Event registration. Supported event names:
//   'restart'      — plugin requested a state refresh (RestartFlags bitmask).
//   'dirty'        — plugin asked the host to mark the project dirty (boolean).
//   'beginGesture' — plugin started an automation gesture (paramId).
//   'endGesture'   — plugin ended an automation gesture (paramId).
//   'startGroup'   — plugin started a group of edits (no payload).
//   'finishGroup'  — plugin finished a group of edits (no payload).
//
// The 'restart' event uses a dedicated TSFN (restartTsfn_) that carries the
// user's callback directly. The host-event events share a single TSFN
// (hostEventTsfn_) whose callback dispatches to the right listener via
// hostEventListeners_. Re-registering an event replaces the previous listener.
Napi::Value PluginInstance::On(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "on(eventName, callback) requires (string, function)");
    }
    std::string eventName = info[0].As<Napi::String>().Utf8Value();
    Napi::Function cb = info[1].As<Napi::Function>();

    if (eventName == "restart") {
        // Existing restart path: the TSFN carries the user's callback directly.
        if (restartTsfnValid_.exchange(false)) {
            restartTsfn_.Release();
        }
        restartTsfn_ = Napi::ThreadSafeFunction::New(
            env, cb, "nst3-restart", 0 /* unlimited queue */, 1 /* initial threads */,
            [](Napi::Env) {});
        restartTsfnValid_.store(true, std::memory_order_release);
        // Wire up the handler's restart callback to call emitRestart.
        if (handler_) {
            handler_->setRestartCallback([this](int32_t flags) { this->emitRestart(flags); });
        }
        return env.Undefined();
    }

    // Host-event listeners: dirty, beginGesture, endGesture, startGroup,
    // finishGroup. These are emitted by ComponentHandler when the plugin
    // invokes the corresponding IComponentHandler{,2} methods (plugin→host).
    if (eventName == "dirty" || eventName == "beginGesture" ||
        eventName == "endGesture" || eventName == "startGroup" ||
        eventName == "finishGroup") {
        // Replace any existing listener for this event name.
        auto it = hostEventListeners_.find(eventName);
        if (it != hostEventListeners_.end()) it->second.Reset();
        hostEventListeners_[eventName] = Napi::Persistent(cb);

        // Lazily create the host-event TSFN on first registration. The TSFN
        // carries a heap-allocated HostEvent and dispatches to the right
        // listener via hostEventListeners_ on the JS thread. The TSFN's JS
        // receiver function is a no-op — the per-call callback does the
        // dispatch.
        if (!hostEventTsfnValid_.exchange(true)) {
            hostEventTsfn_ = Napi::ThreadSafeFunction::New(
                env,
                Napi::Function::New(env, [](const Napi::CallbackInfo&) {}),
                "nst3-host-event", 0 /* unlimited queue */, 1 /* initial threads */,
                [](Napi::Env) {});
            // Wire up the handler's host-event callback to call emitHostEvent.
            if (handler_) {
                handler_->setHostEventCallback(
                    [this](const HostEvent& ev) { this->emitHostEvent(ev); });
            }
        }
        return env.Undefined();
    }

    throwNapiError(env, ErrorCode::InvalidParameter, "Unknown event: " + eventName);
    return env.Undefined();
}

} // namespace nst3
