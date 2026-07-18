//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// Module entry — registers Host and PluginInstance classes and exposes
// version + enum constants to JavaScript.
//-----------------------------------------------------------------------------
#include <napi.h>

#include <string>

#include "host.h"
#include "plugin_instance.h"
#include "version.h"
#include "midi.h"

#include "pluginterfaces/vst/ivsteditcontroller.h"   // ParameterFlags, RestartFlags
#include "pluginterfaces/vst/ivstcomponent.h"        // MediaTypes, BusTypes
#include "pluginterfaces/vst/ivstaudioprocessor.h"   // PlugType category strings

namespace nst3 {

static Napi::Object DefineParameterFlags(Napi::Env env) {
    using PF = Steinberg::Vst::ParameterInfo::ParameterFlags;
    Napi::Object o = Napi::Object::New(env);
    o.Set("kNoFlags",          Napi::Number::New(env, static_cast<double>(PF::kNoFlags)));
    o.Set("kCanAutomate",      Napi::Number::New(env, static_cast<double>(PF::kCanAutomate)));
    o.Set("kIsReadOnly",       Napi::Number::New(env, static_cast<double>(PF::kIsReadOnly)));
    o.Set("kIsWrapAround",     Napi::Number::New(env, static_cast<double>(PF::kIsWrapAround)));
    o.Set("kIsList",           Napi::Number::New(env, static_cast<double>(PF::kIsList)));
    o.Set("kIsHidden",         Napi::Number::New(env, static_cast<double>(PF::kIsHidden)));
    o.Set("kIsProgramChange",  Napi::Number::New(env, static_cast<double>(PF::kIsProgramChange)));
    o.Set("kIsBypass",         Napi::Number::New(env, static_cast<double>(PF::kIsBypass)));
    // Convenience aliases without the k-prefix (more idiomatic for JS).
    o.Set("NoFlags",          Napi::Number::New(env, static_cast<double>(PF::kNoFlags)));
    o.Set("CanAutomate",      Napi::Number::New(env, static_cast<double>(PF::kCanAutomate)));
    o.Set("IsReadOnly",       Napi::Number::New(env, static_cast<double>(PF::kIsReadOnly)));
    o.Set("IsWrapAround",     Napi::Number::New(env, static_cast<double>(PF::kIsWrapAround)));
    o.Set("IsList",           Napi::Number::New(env, static_cast<double>(PF::kIsList)));
    o.Set("IsHidden",         Napi::Number::New(env, static_cast<double>(PF::kIsHidden)));
    o.Set("IsProgramChange",  Napi::Number::New(env, static_cast<double>(PF::kIsProgramChange)));
    o.Set("IsBypass",         Napi::Number::New(env, static_cast<double>(PF::kIsBypass)));
    return o;
}

static Napi::Object DefineRestartFlags(Napi::Env env) {
    using RF = Steinberg::Vst::RestartFlags;
    Napi::Object o = Napi::Object::New(env);
    o.Set("kReloadComponent",           Napi::Number::New(env, static_cast<double>(RF::kReloadComponent)));
    o.Set("kIoChanged",                 Napi::Number::New(env, static_cast<double>(RF::kIoChanged)));
    o.Set("kParamValuesChanged",        Napi::Number::New(env, static_cast<double>(RF::kParamValuesChanged)));
    o.Set("kLatencyChanged",            Napi::Number::New(env, static_cast<double>(RF::kLatencyChanged)));
    o.Set("kParamTitlesChanged",        Napi::Number::New(env, static_cast<double>(RF::kParamTitlesChanged)));
    o.Set("kMidiCCAssignmentChanged",   Napi::Number::New(env, static_cast<double>(RF::kMidiCCAssignmentChanged)));
    o.Set("kNoteExpressionChanged",     Napi::Number::New(env, static_cast<double>(RF::kNoteExpressionChanged)));
    o.Set("kIoTitlesChanged",           Napi::Number::New(env, static_cast<double>(RF::kIoTitlesChanged)));
    o.Set("kPrefetchableSupportChanged",Napi::Number::New(env, static_cast<double>(RF::kPrefetchableSupportChanged)));
    o.Set("kRoutingInfoChanged",        Napi::Number::New(env, static_cast<double>(RF::kRoutingInfoChanged)));
    // Convenience aliases.
    o.Set("ReloadComponent",           Napi::Number::New(env, static_cast<double>(RF::kReloadComponent)));
    o.Set("IoChanged",                 Napi::Number::New(env, static_cast<double>(RF::kIoChanged)));
    o.Set("ParamValuesChanged",        Napi::Number::New(env, static_cast<double>(RF::kParamValuesChanged)));
    o.Set("LatencyChanged",            Napi::Number::New(env, static_cast<double>(RF::kLatencyChanged)));
    o.Set("ParamTitlesChanged",        Napi::Number::New(env, static_cast<double>(RF::kParamTitlesChanged)));
    o.Set("MidiCCAssignmentChanged",   Napi::Number::New(env, static_cast<double>(RF::kMidiCCAssignmentChanged)));
    o.Set("NoteExpressionChanged",     Napi::Number::New(env, static_cast<double>(RF::kNoteExpressionChanged)));
    o.Set("IoTitlesChanged",           Napi::Number::New(env, static_cast<double>(RF::kIoTitlesChanged)));
    o.Set("PrefetchableSupportChanged",Napi::Number::New(env, static_cast<double>(RF::kPrefetchableSupportChanged)));
    o.Set("RoutingInfoChanged",        Napi::Number::New(env, static_cast<double>(RF::kRoutingInfoChanged)));
    return o;
}

static Napi::Object DefineBusType(Napi::Env env) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("kMain", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kMain)));
    o.Set("kAux",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kAux)));
    o.Set("Main",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kMain)));
    o.Set("Aux",   Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kAux)));
    return o;
}

static Napi::Object DefineMediaType(Napi::Env env) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("kAudio", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kAudio)));
    o.Set("kEvent", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kEvent)));
    o.Set("Audio",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kAudio)));
    o.Set("Event",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kEvent)));
    return o;
}

static Napi::Object DefineMidiEventType(Napi::Env env) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("NoteOff",         Napi::Number::New(env, static_cast<int32_t>(MidiEventType::NoteOff)));
    o.Set("NoteOn",          Napi::Number::New(env, static_cast<int32_t>(MidiEventType::NoteOn)));
    o.Set("PolyPressure",    Napi::Number::New(env, static_cast<int32_t>(MidiEventType::PolyPressure)));
    o.Set("Controller",      Napi::Number::New(env, static_cast<int32_t>(MidiEventType::Controller)));
    o.Set("ProgramChange",   Napi::Number::New(env, static_cast<int32_t>(MidiEventType::ProgramChange)));
    o.Set("ChannelPressure", Napi::Number::New(env, static_cast<int32_t>(MidiEventType::ChannelPressure)));
    o.Set("PitchBend",       Napi::Number::New(env, static_cast<int32_t>(MidiEventType::PitchBend)));
    o.Set("SysEx",           Napi::Number::New(env, static_cast<int32_t>(MidiEventType::SysEx)));
    return o;
}

static Napi::Object DefinePluginCategory(Napi::Env env) {
    namespace PT = Steinberg::Vst::PlugType;
    Napi::Object o = Napi::Object::New(env);
    // SubCategory strings from ivstaudioprocessor.h — useful for filtering scans.
    o.Set("Fx",                 Napi::String::New(env, PT::kFx));
    o.Set("FxAnalyzer",         Napi::String::New(env, PT::kFxAnalyzer));
    o.Set("FxDelay",            Napi::String::New(env, PT::kFxDelay));
    o.Set("FxDistortion",       Napi::String::New(env, PT::kFxDistortion));
    o.Set("FxDynamics",         Napi::String::New(env, PT::kFxDynamics));
    o.Set("FxMastering",        Napi::String::New(env, PT::kFxMastering));
    o.Set("FxModulation",       Napi::String::New(env, PT::kFxModulation));
    o.Set("FxPitchShift",       Napi::String::New(env, PT::kFxPitchShift));
    o.Set("FxRestoration",      Napi::String::New(env, PT::kFxRestoration));
    o.Set("FxReverb",           Napi::String::New(env, PT::kFxReverb));
    o.Set("FxSurround",         Napi::String::New(env, PT::kFxSurround));
    o.Set("FxTools",            Napi::String::New(env, PT::kFxTools));
    o.Set("Instrument",         Napi::String::New(env, PT::kInstrument));
    o.Set("InstrumentSynth",    Napi::String::New(env, PT::kInstrumentSynth));
    o.Set("InstrumentSynthSampler",
          Napi::String::New(env, PT::kInstrumentSynthSampler));
    return o;
}

static Napi::Value Version(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Object o = Napi::Object::New(env);
    o.Set("native", Napi::String::New(env, nst3Version()));
    o.Set("vst3sdk", Napi::String::New(env, vst3SdkVersion()));
    o.Set("napi", Napi::Number::New(env, NAPI_VERSION));
    return o;
}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports) {
    Host::Init(env, exports);
    PluginInstance::Init(env, exports);

    exports.Set("version", Napi::Function::New(env, Version, "version"));
    exports.Set("ParameterFlags", DefineParameterFlags(env));
    exports.Set("RestartFlags",   DefineRestartFlags(env));
    exports.Set("BusType",        DefineBusType(env));
    exports.Set("MediaType",      DefineMediaType(env));
    exports.Set("MidiEventType",  DefineMidiEventType(env));
    exports.Set("PluginCategory", DefinePluginCategory(env));

    // Numeric constants exposed at the top level for convenience.
    exports.Set("NAPI_VERSION", Napi::Number::New(env, NAPI_VERSION));
    return exports;
}

NODE_API_MODULE(addon, InitModule)

} // namespace nst3
