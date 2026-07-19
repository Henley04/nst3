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
#include "pluginterfaces/vst/ivstnoteexpression.h"   // NoteExpressionTypeIds
#include "pluginterfaces/vst/vstspeaker.h"           // SpeakerArr::kMono, kStereo, ...

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

// SampleSize enum — mirrors Steinberg::Vst::SymbolicSampleSizes but exposed
// to JS as plain integers (32 and 64) matching the user-facing API.
static Napi::Object DefineSampleSize(Napi::Env env) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("Sample32", Napi::Number::New(env, 32));
    o.Set("Sample64", Napi::Number::New(env, 64));
    o.Set("kSample32", Napi::Number::New(env, 32));
    o.Set("kSample64", Napi::Number::New(env, 64));
    return o;
}

// ProcessMode enum — mirrors Steinberg::Vst::ProcessMode (kRealtime=0,
// kOffline=1, kPrefetch=2). Matches the VST3 SDK constants exactly.
static Napi::Object DefineProcessMode(Napi::Env env) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("Realtime", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kRealtime)));
    o.Set("Offline",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kOffline)));
    o.Set("Prefetch", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kPrefetch)));
    o.Set("kRealtime", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kRealtime)));
    o.Set("kOffline",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kOffline)));
    o.Set("kPrefetch", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kPrefetch)));
    return o;
}

// BusDirection enum — mirrors Steinberg::Vst::BusDirection (kInput=0,
// kOutput=1). Matches the VST3 SDK constants exactly.
static Napi::Object DefineBusDirection(Napi::Env env) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("Input",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kInput)));
    o.Set("Output", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kOutput)));
    o.Set("kInput",  Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kInput)));
    o.Set("kOutput", Napi::Number::New(env, static_cast<double>(Steinberg::Vst::kOutput)));
    return o;
}

// KnobMode enum — mirrors Steinberg::Vst::KnobModes values
// (kCircularMode=0, kRelativCircularMode=1, kLinearMode=2). Passed to
// `plugin.setKnobMode(mode)` to drive the plugin's IEditController2.
// Note: the SDK enum is `KnobModes` (plural) and `KnobMode` is just a
// `using KnobMode = int32;` typedef. The SDK also spells the relative-mode
// constant `kRelativCircularMode` (no 'e'); we mirror the SDK spelling for
// the k-alias and expose the correct-English `RelativeCircular` alias.
static Napi::Object DefineKnobMode(Napi::Env env) {
    using KM = Steinberg::Vst::KnobModes;
    Napi::Object o = Napi::Object::New(env);
    o.Set("Circular",           Napi::Number::New(env, static_cast<double>(KM::kCircularMode)));
    o.Set("RelativeCircular",   Napi::Number::New(env, static_cast<double>(KM::kRelativCircularMode)));
    o.Set("Linear",             Napi::Number::New(env, static_cast<double>(KM::kLinearMode)));
    // k-prefixed aliases for SDK-faithful naming (note SDK typo: kRelativCircularMode).
    o.Set("kCircularMode",          Napi::Number::New(env, static_cast<double>(KM::kCircularMode)));
    o.Set("kRelativCircularMode",   Napi::Number::New(env, static_cast<double>(KM::kRelativCircularMode)));
    // Common-misspelling alias for callers who type the correct-English form.
    o.Set("kRelativeCircularMode",  Napi::Number::New(env, static_cast<double>(KM::kRelativCircularMode)));
    o.Set("kLinearMode",            Napi::Number::New(env, static_cast<double>(KM::kLinearMode)));
    return o;
}

// NoteExpressionTypeIds enum — mirrors Steinberg::Vst::NoteExpressionTypeIDs
// (kVolumeTypeID=0, kPanTypeID=1, kTuningTypeID=2, kVibratoTypeID=3,
// kExpressionTypeID=4, kBrightnessTypeID=5). The pinned SDK version has only
// these six built-in type IDs; later SDKs added kSoundPressureTypeID,
// kSoundPowerOctaveTypeID, and kPitchTypeID, which are intentionally NOT
// exposed here. Custom type IDs start at kCustomStart (= 100000).
static Napi::Object DefineNoteExpressionTypeIds(Napi::Env env) {
    using NET = Steinberg::Vst::NoteExpressionTypeIDs;
    Napi::Object o = Napi::Object::New(env);
    o.Set("Volume",            Napi::Number::New(env, static_cast<double>(NET::kVolumeTypeID)));
    o.Set("Pan",               Napi::Number::New(env, static_cast<double>(NET::kPanTypeID)));
    o.Set("Tuning",            Napi::Number::New(env, static_cast<double>(NET::kTuningTypeID)));
    o.Set("Vibrato",           Napi::Number::New(env, static_cast<double>(NET::kVibratoTypeID)));
    o.Set("Expression",        Napi::Number::New(env, static_cast<double>(NET::kExpressionTypeID)));
    o.Set("Brightness",        Napi::Number::New(env, static_cast<double>(NET::kBrightnessTypeID)));
    // k-prefixed aliases for SDK-faithful naming.
    o.Set("kVolumeTypeID",            Napi::Number::New(env, static_cast<double>(NET::kVolumeTypeID)));
    o.Set("kPanTypeID",               Napi::Number::New(env, static_cast<double>(NET::kPanTypeID)));
    o.Set("kTuningTypeID",            Napi::Number::New(env, static_cast<double>(NET::kTuningTypeID)));
    o.Set("kVibratoTypeID",           Napi::Number::New(env, static_cast<double>(NET::kVibratoTypeID)));
    o.Set("kExpressionTypeID",        Napi::Number::New(env, static_cast<double>(NET::kExpressionTypeID)));
    o.Set("kBrightnessTypeID",        Napi::Number::New(env, static_cast<double>(NET::kBrightnessTypeID)));
    return o;
}

// SpeakerArrangement enum — mirrors Steinberg::Vst::SpeakerArr constants
// (kMono, kStereo, k30Cine, k31Cine, k40Cine, k50, k51, k60Cine, k61Cine,
// k70Cine, k71Cine, k71_2, k71_4). Values are the SDK's integral arrangement
// codes; pass them to setBusArrangement() / read them from getBusArrangement().
static Napi::Object DefineSpeakerArrangement(Napi::Env env) {
    namespace SA = Steinberg::Vst::SpeakerArr;
    Napi::Object o = Napi::Object::New(env);
    o.Set("Mono",        Napi::Number::New(env, static_cast<double>(SA::kMono)));
    o.Set("Stereo",      Napi::Number::New(env, static_cast<double>(SA::kStereo)));
    o.Set("_30Cine",     Napi::Number::New(env, static_cast<double>(SA::k30Cine)));
    o.Set("_31Cine",     Napi::Number::New(env, static_cast<double>(SA::k31Cine)));
    o.Set("_40Cine",     Napi::Number::New(env, static_cast<double>(SA::k40Cine)));
    o.Set("_50",         Napi::Number::New(env, static_cast<double>(SA::k50)));
    o.Set("_51",         Napi::Number::New(env, static_cast<double>(SA::k51)));
    o.Set("_60Cine",     Napi::Number::New(env, static_cast<double>(SA::k60Cine)));
    o.Set("_61Cine",     Napi::Number::New(env, static_cast<double>(SA::k61Cine)));
    o.Set("_70Cine",     Napi::Number::New(env, static_cast<double>(SA::k70Cine)));
    o.Set("_71Cine",     Napi::Number::New(env, static_cast<double>(SA::k71Cine)));
    o.Set("_71_2",       Napi::Number::New(env, static_cast<double>(SA::k71_2)));
    o.Set("_71_4",       Napi::Number::New(env, static_cast<double>(SA::k71_4)));
    // k-prefixed aliases for SDK-faithful naming.
    o.Set("kMono",       Napi::Number::New(env, static_cast<double>(SA::kMono)));
    o.Set("kStereo",     Napi::Number::New(env, static_cast<double>(SA::kStereo)));
    o.Set("k30Cine",     Napi::Number::New(env, static_cast<double>(SA::k30Cine)));
    o.Set("k31Cine",     Napi::Number::New(env, static_cast<double>(SA::k31Cine)));
    o.Set("k40Cine",     Napi::Number::New(env, static_cast<double>(SA::k40Cine)));
    o.Set("k50",         Napi::Number::New(env, static_cast<double>(SA::k50)));
    o.Set("k51",         Napi::Number::New(env, static_cast<double>(SA::k51)));
    o.Set("k60Cine",     Napi::Number::New(env, static_cast<double>(SA::k60Cine)));
    o.Set("k61Cine",     Napi::Number::New(env, static_cast<double>(SA::k61Cine)));
    o.Set("k70Cine",     Napi::Number::New(env, static_cast<double>(SA::k70Cine)));
    o.Set("k71Cine",     Napi::Number::New(env, static_cast<double>(SA::k71Cine)));
    o.Set("k71_2",       Napi::Number::New(env, static_cast<double>(SA::k71_2)));
    o.Set("k71_4",       Napi::Number::New(env, static_cast<double>(SA::k71_4)));
    return o;
}

// ProcessContextRequirementFlags enum — mirrors
// Steinberg::Vst::IProcessContextRequirements::Flags. Returned by
// `plugin.getProcessContextRequirements()` as a bitmask; used by the host to
// decide which ProcessContext fields to recompute each block.
static Napi::Object DefineProcessContextRequirementFlags(Napi::Env env) {
    using PCRF = Steinberg::Vst::IProcessContextRequirements::Flags;
    Napi::Object o = Napi::Object::New(env);
    o.Set("NeedSystemTime",            Napi::Number::New(env, static_cast<double>(PCRF::kNeedSystemTime)));
    o.Set("NeedContinousTimeSamples",  Napi::Number::New(env, static_cast<double>(PCRF::kNeedContinousTimeSamples)));
    o.Set("NeedProjectTimeMusic",      Napi::Number::New(env, static_cast<double>(PCRF::kNeedProjectTimeMusic)));
    o.Set("NeedBarPositionMusic",      Napi::Number::New(env, static_cast<double>(PCRF::kNeedBarPositionMusic)));
    o.Set("NeedCycleMusic",            Napi::Number::New(env, static_cast<double>(PCRF::kNeedCycleMusic)));
    o.Set("NeedSamplesToNextClock",    Napi::Number::New(env, static_cast<double>(PCRF::kNeedSamplesToNextClock)));
    o.Set("NeedTempo",                 Napi::Number::New(env, static_cast<double>(PCRF::kNeedTempo)));
    o.Set("NeedTimeSignature",         Napi::Number::New(env, static_cast<double>(PCRF::kNeedTimeSignature)));
    o.Set("NeedChord",                 Napi::Number::New(env, static_cast<double>(PCRF::kNeedChord)));
    o.Set("NeedFrameRate",             Napi::Number::New(env, static_cast<double>(PCRF::kNeedFrameRate)));
    o.Set("NeedTransportState",        Napi::Number::New(env, static_cast<double>(PCRF::kNeedTransportState)));
    // k-prefixed aliases for SDK-faithful naming.
    o.Set("kNeedSystemTime",           Napi::Number::New(env, static_cast<double>(PCRF::kNeedSystemTime)));
    o.Set("kNeedContinousTimeSamples", Napi::Number::New(env, static_cast<double>(PCRF::kNeedContinousTimeSamples)));
    o.Set("kNeedProjectTimeMusic",     Napi::Number::New(env, static_cast<double>(PCRF::kNeedProjectTimeMusic)));
    o.Set("kNeedBarPositionMusic",     Napi::Number::New(env, static_cast<double>(PCRF::kNeedBarPositionMusic)));
    o.Set("kNeedCycleMusic",           Napi::Number::New(env, static_cast<double>(PCRF::kNeedCycleMusic)));
    o.Set("kNeedSamplesToNextClock",   Napi::Number::New(env, static_cast<double>(PCRF::kNeedSamplesToNextClock)));
    o.Set("kNeedTempo",                Napi::Number::New(env, static_cast<double>(PCRF::kNeedTempo)));
    o.Set("kNeedTimeSignature",        Napi::Number::New(env, static_cast<double>(PCRF::kNeedTimeSignature)));
    o.Set("kNeedChord",                Napi::Number::New(env, static_cast<double>(PCRF::kNeedChord)));
    o.Set("kNeedFrameRate",            Napi::Number::New(env, static_cast<double>(PCRF::kNeedFrameRate)));
    o.Set("kNeedTransportState",       Napi::Number::New(env, static_cast<double>(PCRF::kNeedTransportState)));
    return o;
}

// ChannelContextInfoFlags enum — convenience bit flags describing which
// channel-context info fields are present when sent via IInfoListener. The
// VST3 SDK does not declare a canonical flags enum for this (IInfoListener
// simply receives an IAttributeList); these bit values are a host-side
// convention mirrored 1:1 from index.d.ts so JS callers can build a bitmask
// of which fields they intend to send.
static Napi::Object DefineChannelContextInfoFlags(Napi::Env env) {
    Napi::Object o = Napi::Object::New(env);
    constexpr double ContainsPluginName          = 1 << 0;
    constexpr double ContainsTrackName           = 1 << 1;
    constexpr double ContainsTrackColor          = 1 << 2;
    constexpr double ContainsTrackNamespace      = 1 << 3;
    constexpr double ContainsTrackNamespaceColor = 1 << 4;
    o.Set("ContainsPluginName",          Napi::Number::New(env, ContainsPluginName));
    o.Set("ContainsTrackName",           Napi::Number::New(env, ContainsTrackName));
    o.Set("ContainsTrackColor",          Napi::Number::New(env, ContainsTrackColor));
    o.Set("ContainsTrackNamespace",      Napi::Number::New(env, ContainsTrackNamespace));
    o.Set("ContainsTrackNamespaceColor", Napi::Number::New(env, ContainsTrackNamespaceColor));
    // k-prefixed aliases.
    o.Set("kContainsPluginName",          Napi::Number::New(env, ContainsPluginName));
    o.Set("kContainsTrackName",           Napi::Number::New(env, ContainsTrackName));
    o.Set("kContainsTrackColor",          Napi::Number::New(env, ContainsTrackColor));
    o.Set("kContainsTrackNamespace",      Napi::Number::New(env, ContainsTrackNamespace));
    o.Set("kContainsTrackNamespaceColor", Napi::Number::New(env, ContainsTrackNamespaceColor));
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
    exports.Set("SampleSize",     DefineSampleSize(env));
    exports.Set("ProcessMode",    DefineProcessMode(env));
    exports.Set("BusDirection",   DefineBusDirection(env));
    exports.Set("KnobMode",       DefineKnobMode(env));
    exports.Set("NoteExpressionTypeIds", DefineNoteExpressionTypeIds(env));
    exports.Set("SpeakerArrangement",    DefineSpeakerArrangement(env));
    exports.Set("ProcessContextRequirementFlags", DefineProcessContextRequirementFlags(env));
    exports.Set("ChannelContextInfoFlags", DefineChannelContextInfoFlags(env));
    exports.Set("PluginCategory", DefinePluginCategory(env));

    // Numeric constants exposed at the top level for convenience.
    exports.Set("NAPI_VERSION", Napi::Number::New(env, NAPI_VERSION));
    return exports;
}

NODE_API_MODULE(addon, InitModule)

} // namespace nst3
