//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// PluginInstance implementation
//-----------------------------------------------------------------------------
#include "plugin_instance.h"

#include <cstring>
#include <algorithm>
#include <sstream>

#include "errors.h"
#include "string_convert.h"

#include "public.sdk/source/vst/utility/uid.h"
#include "public.sdk/source/vst/hosting/module.h"

#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
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
    if (controller_) {
        controller_->setComponentHandler(handler_.get());
        hostApp_->setComponentHandler(handler_.get());
        info_.hasController = true;
    }

    // Read bus info
    Steinberg::int32 inAudio = 0, outAudio = 0, inMidi = 0, outMidi = 0;
    Steinberg::Vst::BusInfo busInfo;
    for (Steinberg::int32 i = 0; i < component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput); ++i) {
        if (component_->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, i, busInfo) == Steinberg::kResultTrue) {
            inAudio += busInfo.channelCount;
        }
    }
    for (Steinberg::int32 i = 0; i < component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput); ++i) {
        if (component_->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, i, busInfo) == Steinberg::kResultTrue) {
            outAudio += busInfo.channelCount;
        }
    }
    inMidi = component_->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
    outMidi = component_->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);

    // Activate default audio buses (first input, first output)
    if (component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput) > 0) {
        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, 0, true);
    }
    if (component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput) > 0) {
        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0, true);
    }
    if (inMidi > 0) component_->activateBus(Steinberg::Vst::kEvent, Steinberg::Vst::kInput, 0, true);
    if (outMidi > 0) component_->activateBus(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput, 0, true);

    // Setup ProcessData struct (reused across calls)
    std::memset(&processData_, 0, sizeof(processData_));
    processData_.processMode = Steinberg::Vst::kRealtime;
    processData_.symbolicSampleSize = Steinberg::Vst::kSample32;
    processData_.inputParameterChanges = &inputParams_;
    processData_.inputEvents = &inputEvents_;
    processData_.outputParameterChanges = &outputParams_;
    processData_.outputEvents = &outputEvents_;

    std::memset(&inputBuffers_, 0, sizeof(inputBuffers_));
    std::memset(&outputBuffers_, 0, sizeof(outputBuffers_));

    // Size channel pointer vectors to bus counts (allocate once)
    inputChannelPtrs_.assign(static_cast<size_t>(std::max<int32_t>(inAudio, opts.audioInputs)), nullptr);
    outputChannelPtrs_.assign(static_cast<size_t>(std::max<int32_t>(outAudio, opts.audioOutputs)), nullptr);

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
    info_.numAudioInputs = inAudio;
    info_.numAudioOutputs = outAudio;
    info_.numMidiInputs = inMidi;
    info_.numMidiOutputs = outMidi;
    info_.parameterCount = controller_ ? controller_->getParameterCount() : 0;

    return true;
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

    // Collect input Float32Arrays
    std::vector<Napi::Float32Array> inputsArr;
    if (block.Has("inputs") && block.Get("inputs").IsArray()) {
        Napi::Array arr = block.Get("inputs").As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            Napi::Value v = arr[i];
            if (!v.IsTypedArray() || v.As<Napi::TypedArray>().TypedArrayType() != napi_float32_array) {
                throwNapiError(env, ErrorCode::InvalidBuffer, "inputs must be Float32Array[]");
            }
            inputsArr.push_back(v.As<Napi::Float32Array>());
            if (static_cast<int32_t>(inputsArr.back().ElementLength()) < numSamples) {
                throwNapiError(env, ErrorCode::InvalidBuffer, "inputs buffer length < numSamples");
            }
        }
    }
    std::vector<Napi::Float32Array> outputsArr;
    if (block.Has("outputs") && block.Get("outputs").IsArray()) {
        Napi::Array arr = block.Get("outputs").As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            Napi::Value v = arr[i];
            if (!v.IsTypedArray() || v.As<Napi::TypedArray>().TypedArrayType() != napi_float32_array) {
                throwNapiError(env, ErrorCode::InvalidBuffer, "outputs must be Float32Array[]");
            }
            outputsArr.push_back(v.As<Napi::Float32Array>());
            if (static_cast<int32_t>(outputsArr.back().ElementLength()) < numSamples) {
                throwNapiError(env, ErrorCode::InvalidBuffer, "outputs buffer length < numSamples");
            }
        }
    }

    return translateExceptions(env, [&]() -> Napi::Value {
        // Wire up AudioBusBuffers (zero-copy from JS Float32Array)
        for (size_t i = 0; i < inputChannelPtrs_.size() && i < inputsArr.size(); ++i) {
            inputChannelPtrs_[i] = inputsArr[i].Data();
        }
        for (size_t i = 0; i < outputChannelPtrs_.size() && i < outputsArr.size(); ++i) {
            outputChannelPtrs_[i] = outputsArr[i].Data();
        }

        inputBuffers_.numChannels = static_cast<Steinberg::int32>(inputChannelPtrs_.size());
        inputBuffers_.channelBuffers32 = inputChannelPtrs_.data();
        outputBuffers_.numChannels = static_cast<Steinberg::int32>(outputChannelPtrs_.size());
        outputBuffers_.channelBuffers32 = outputChannelPtrs_.data();

        processData_.numSamples = numSamples;
        processData_.numInputs = 1;
        processData_.numOutputs = 1;
        processData_.inputs = &inputBuffers_;
        processData_.outputs = &outputBuffers_;

        // Reset output containers (clear stale state from prior process)
        outputParams_.clearQueue();
        outputEvents_.clear();

        // Snapshot process context (transport) — currently off / not playing
        Steinberg::Vst::ProcessContext ctx;
        std::memset(&ctx, 0, sizeof(ctx));
        ctx.sampleRate = opts_.sampleRate;
        ctx.state = 0; // stopped
        processData_.processContext = &ctx;

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
        } else {
            Steinberg::Vst::Event e;
            if (!midiBytesToEvent(arr.Data(), arr.ElementLength(), sampleOffset, e)) {
                throwNst(ErrorCode::MidiError, "Failed to parse MIDI bytes");
            }
            inputEvents_.addEvent(e);
        }
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
