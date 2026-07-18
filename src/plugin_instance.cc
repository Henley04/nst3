//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// PluginInstance implementation
//-----------------------------------------------------------------------------
#include "plugin_instance.h"

#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>

#include "errors.h"
#include "string_convert.h"

#include "public.sdk/source/vst/utility/uid.h"
#include "public.sdk/source/vst/hosting/module.h"

#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
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
        InstanceMethod("getParameterCount", &PluginInstance::GetParameterCount),
        InstanceMethod("getParameterInfo", &PluginInstance::GetParameterInfo),
        InstanceMethod("getParameter", &PluginInstance::GetParameter),
        InstanceMethod("setParameter", &PluginInstance::SetParameter),
        InstanceMethod("setParameters", &PluginInstance::SetParameters),
        InstanceMethod("formatParameter", &PluginInstance::FormatParameter),
        InstanceMethod("addMidiEvent", &PluginInstance::AddMidiEvent),
        InstanceMethod("addMidiBytes", &PluginInstance::AddMidiBytes),
        InstanceMethod("takeOutputEvents", &PluginInstance::TakeOutputEvents),
        InstanceMethod("clearEvents", &PluginInstance::ClearEvents),
        InstanceMethod("saveState", &PluginInstance::SaveState),
        InstanceMethod("loadState", &PluginInstance::LoadState),
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

    // Setup ComponentHandler (so plugin's controller can call back into the host)
    handler_ = std::make_unique<ComponentHandler>();
    handler_->setPluginInstance(this);
    handler_->setPerformEditSink(this);  // route performEdit → inputParams_
    if (controller_) {
        controller_->setComponentHandler(handler_.get());
        hostApp_->setComponentHandler(handler_.get());
        info_.hasController = true;
        // Query optional IMidiMapping for proper MIDI CC/PB/PC routing.
        midiMapping_ = Steinberg::U::cast<Steinberg::Vst::IMidiMapping>(controller_);
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

    // Setup ProcessData struct (reused across calls)
    std::memset(&processData_, 0, sizeof(processData_));
    processData_.processMode = Steinberg::Vst::kRealtime;
    processData_.symbolicSampleSize = Steinberg::Vst::kSample32;
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
    for (Steinberg::int32 i = 0; i < inAudioBuses; ++i) {
        auto& bufs = inputChannelPtrsPerBus_[static_cast<size_t>(i)];
        bufs.assign(static_cast<size_t>(inputBusInfos_[static_cast<size_t>(i)].channelCount), nullptr);
        inputBuffers_[static_cast<size_t>(i)].numChannels = inputBusInfos_[static_cast<size_t>(i)].channelCount;
        inputBuffers_[static_cast<size_t>(i)].silenceFlags = 0;
        inputBuffers_[static_cast<size_t>(i)].channelBuffers32 = bufs.data();
    }
    for (Steinberg::int32 i = 0; i < outAudioBuses; ++i) {
        auto& bufs = outputChannelPtrsPerBus_[static_cast<size_t>(i)];
        bufs.assign(static_cast<size_t>(outputBusInfos_[static_cast<size_t>(i)].channelCount), nullptr);
        outputBuffers_[static_cast<size_t>(i)].numChannels = outputBusInfos_[static_cast<size_t>(i)].channelCount;
        outputBuffers_[static_cast<size_t>(i)].silenceFlags = 0;
        outputBuffers_[static_cast<size_t>(i)].channelBuffers32 = bufs.data();
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
            // setupProcessing must be called before setActive
            Steinberg::Vst::ProcessSetup setup{
                Steinberg::Vst::kRealtime,
                Steinberg::Vst::kSample32,
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
    if (numSamples <= 0 || numSamples > opts_.maxBlockSize) {
        throwNapiError(env, ErrorCode::InvalidBuffer,
                       "block.numSamples must be in (0, " + std::to_string(opts_.maxBlockSize) + "]");
    }

    // Resolve input/output buses from the JS `inputs`/`outputs` values.
    // Supported shapes (per direction):
    //   - undefined / null: all buses silent
    //   - Float32Array[ch0, ch1, ...]: single-bus (backward compat)
    //   - Array[Float32Array, Float32Array, ...]: single-bus, explicit channels
    //   - Array[Array[Float32Array,...], Array[Float32Array,...]]: multi-bus
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

    return translateExceptions(env, [&]() -> Napi::Value {
        // Wire per-bus AudioBusBuffers to the freshly resolved channel ptrs.
        for (size_t i = 0; i < inputBuffers_.size(); ++i) {
            inputBuffers_[i].numChannels =
                static_cast<Steinberg::int32>(inputChannelPtrsPerBus_[i].size());
            inputBuffers_[i].channelBuffers32 = inputChannelPtrsPerBus_[i].data();
            // silenceFlags = 0 means "not silent"; the host promises nothing
            // here, the plugin must read input as-is.
            inputBuffers_[i].silenceFlags = 0;
        }
        for (size_t i = 0; i < outputBuffers_.size(); ++i) {
            outputBuffers_[i].numChannels =
                static_cast<Steinberg::int32>(outputChannelPtrsPerBus_[i].size());
            outputBuffers_[i].channelBuffers32 = outputChannelPtrsPerBus_[i].data();
            outputBuffers_[i].silenceFlags = 0;
        }

        processData_.numSamples = numSamples;
        // numInputs / numOutputs / inputs / outputs already wired in setup().

        // Reset output containers (clear stale state from prior process)
        outputParams_.clearQueue();
        outputEvents_.clear();

        // Advance the persistent ProcessContext: project time in samples
        // advances by numSamples each block. projectTimeMusic advances by
        // (numSamples * tempo) / (60 * sampleRate) quarter notes per block.
        processContext_.projectTimeSamples += numSamples;
        if (processContext_.state & Steinberg::Vst::ProcessContext::kTempoValid) {
            double quartersPerSample =
                processContext_.tempo / (60.0 * opts_.sampleRate);
            processContext_.projectTimeMusic +=
                static_cast<double>(numSamples) * quartersPerSample;
            // Bar position is the last bar boundary; recompute from quarter
            // position using the (4/4) signature: one bar = 4 quarter notes
            // in 4/4. For non-4/4 this is approximate but still useful.
            double quartersPerBar = static_cast<double>(
                processContext_.timeSigNumerator * 4) /
                static_cast<double>(
                    processContext_.timeSigDenominator > 0
                        ? processContext_.timeSigDenominator : 4);
            double bars =
                std::floor(processContext_.projectTimeMusic / quartersPerBar);
            processContext_.barPositionMusic = bars * quartersPerBar;
        }
        // samplesToNextClock: 24 PPQ per quarter note. Compute the distance
        // from the current sample position to the next MIDI clock tick.
        if (processContext_.state & Steinberg::Vst::ProcessContext::kTempoValid) {
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

        // After process, the SDK convention is that the host clears the input
        // parameter changes and event list. We clear them now so the next call
        // starts fresh.
        inputParams_.clearQueue();
        inputEvents_.clear();
        sysexHeld_.clear();

        return env.Undefined();
    });
}

// Resolve JS `inputs`/`outputs` value to per-bus channel pointer vectors.
// Returns false (and throws) on shape/length errors.
bool PluginInstance::resolveAudioBuses(
    Napi::Env env, Napi::Value value,
    const std::vector<AudioBusInfoEntry>& busInfos,
    std::vector<std::vector<float*>>& outPtrs,
    int32_t numSamples, const char* dirName) {
    // Reset all channel pointers to nullptr (silence).
    for (auto& v : outPtrs) std::fill(v.begin(), v.end(), nullptr);

    if (value.IsNull() || value.IsUndefined()) return true;
    if (!value.IsArray()) {
        throwNapiError(env, ErrorCode::InvalidBuffer,
                       std::string(dirName) + " must be a Float32Array[] or Float32Array[][]");
        return false;
    }
    Napi::Array arr = value.As<Napi::Array>();

    // Helper: extract a Float32Array channel with length check.
    auto extractChannel = [&](Napi::Value v, float*& out) -> bool {
        if (!v.IsTypedArray() ||
            v.As<Napi::TypedArray>().TypedArrayType() != napi_float32_array) {
            throwNapiError(env, ErrorCode::InvalidBuffer,
                           std::string(dirName) + " entries must be Float32Array");
            return false;
        }
        Napi::Float32Array fa = v.As<Napi::Float32Array>();
        if (static_cast<int32_t>(fa.ElementLength()) < numSamples) {
            throwNapiError(env, ErrorCode::InvalidBuffer,
                           std::string(dirName) + " buffer length < numSamples");
            return false;
        }
        out = fa.Data();
        return true;
    };

    // Detect multi-bus form: an array whose first element is itself an array
    // of Float32Arrays. Single-bus form: a flat array of Float32Arrays.
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

        Steinberg::Vst::Event e;
        if (!structuredMidiToEvent(type, channel, note, velocity, cc, ccVal, prog,
                                   pressure, pb, sysexData, sysexSize, sampleOffset, e)) {
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
        // For SysEx we need to hold the buffer; for other messages we copy into the Event.
        if (arr.ElementLength() > 0 && arr.Data()[0] == 0xF0) {
            sysexHeld_.emplace_back(arr.Data(), arr.Data() + arr.ElementLength());
            Steinberg::Vst::Event e;
            if (!midiBytesToEvent(sysexHeld_.back().data(), sysexHeld_.back().size(),
                                  sampleOffset, e)) {
                throwNst(ErrorCode::MidiError, "Failed to parse SysEx bytes");
            }
            // The Event points to our held buffer; safe until next process()
            inputEvents_.addEvent(e);
            return env.Undefined();
        }

        // Parse the bytes first to know which controller type this is.
        Steinberg::Vst::Event e;
        if (!midiBytesToEvent(arr.Data(), arr.ElementLength(), sampleOffset, e)) {
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
Napi::Value PluginInstance::SaveState(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    return translateExceptions(env, [&]() -> Napi::Value {
        BufferStream stream;
        Steinberg::tresult r = component_->getState(&stream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IComponent::getState failed");
        }
        auto bytes = stream.takeBuffer();
        return Napi::Buffer<uint8_t>::Copy(env, bytes.data(), bytes.size());
    });
}

Napi::Value PluginInstance::LoadState(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    checkAlive();
    if (!component_) throwNapiError(env, ErrorCode::Unknown, "No component");
    if (info.Length() < 1 || !info[0].IsBuffer()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "loadState(buffer) requires a Buffer");
    }
    auto buf = info[0].As<Napi::Buffer<uint8_t>>();
    return translateExceptions(env, [&]() -> Napi::Value {
        BufferStream stream(buf.Data(), buf.Length());
        Steinberg::tresult r = component_->setState(&stream);
        if (r != Steinberg::kResultTrue && r != Steinberg::kResultOk) {
            throwNst(ErrorCode::StateError, "IComponent::setState failed");
        }
        if (controller_) {
            // Reset stream position so setComponentState reads from the start
            BufferStream stream2(buf.Data(), buf.Length());
            r = controller_->setComponentState(&stream2);
            // Some plugins return kResultFalse here but it's typically fine
            (void)r;
        }
        return env.Undefined();
    });
}

//------------------------------------------------------------------------
// on('restart', cb)
//------------------------------------------------------------------------
Napi::Value PluginInstance::On(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "on(eventName, callback) requires (string, function)");
    }
    std::string eventName = info[0].As<Napi::String>().Utf8Value();
    Napi::Function cb = info[1].As<Napi::Function>();
    if (eventName != "restart") {
        throwNapiError(env, ErrorCode::InvalidParameter, "Unknown event: " + eventName);
    }
    // If a previous TSFN exists, release it
    if (restartTsfnValid_.exchange(false)) {
        restartTsfn_.Release();
    }
    restartTsfn_ = Napi::ThreadSafeFunction::New(
        env, cb, "nst3-restart", 0 /* unlimited queue */, 1 /* initial threads */,
        [](Napi::Env) {});
    restartTsfnValid_.store(true, std::memory_order_release);

    // Wire up the handler's restart callback to call emitRestart.
    // The ComponentHandler is held by this instance; setting the callback
    // ensures it can call into the TSFN.
    if (handler_) {
        handler_->setRestartCallback([this](int32_t flags) { this->emitRestart(flags); });
    }
    return env.Undefined();
}

} // namespace nst3
