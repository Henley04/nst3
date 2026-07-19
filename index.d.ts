// Type definitions for nst3 — VST3 Host for Node.js
// Hand-written; mirrors the native addon surface 1:1.

declare const nst3: nst3.Nst3Module;
export = nst3;

declare namespace nst3 {

// -------------------------------------------------------------------------
// Top-level module
// -------------------------------------------------------------------------
export interface Nst3Module {
    Host: typeof Host;
    PluginInstance: typeof PluginInstance;
    version(): VersionInfo;
    ParameterFlags: ParameterFlagsEnum;
    RestartFlags: RestartFlagsEnum;
    BusType: BusTypeEnum;
    MediaType: MediaTypeEnum;
    MidiEventType: MidiEventTypeEnum;
    SampleSize: SampleSizeEnum;
    ProcessMode: ProcessModeEnum;
    BusDirection: BusDirectionEnum;
    KnobMode: KnobModeEnum;
    NoteExpressionTypeIds: NoteExpressionTypeIdsEnum;
    SpeakerArrangement: SpeakerArrangementEnum;
    ProcessContextRequirementFlags: ProcessContextRequirementFlagsEnum;
    ChannelContextInfoFlags: ChannelContextInfoFlagsEnum;
    PluginCategory: PluginCategoryEnum;
    NAPI_VERSION: number;
    SUPPORTED_TRIPLES: readonly string[];
}

export interface VersionInfo {
    /** nst3 native addon version (e.g. "0.1.0"). */
    native: string;
    /** VST3 SDK version string (e.g. "VST 3.8.0"). */
    vst3sdk: string;
    /** Node-API (N-API) version the binary was compiled against. */
    napi: number;
}

// -------------------------------------------------------------------------
// Host class
// -------------------------------------------------------------------------
export interface HostOptions {
    /** Sample rate in Hz (default 48000). */
    sampleRate?: number;
    /** Maximum block size in samples per process() call (default 512). */
    maxBlockSize?: number;
    /** Number of audio input channels to allocate (default 2). */
    audioInputs?: number;
    /** Number of audio output channels to allocate (default 2). */
    audioOutputs?: number;
    /**
     * Audio sample size to negotiate with the plugin (default 32).
     * Use 64 to request the `kSample64` processing path; the host will
     * silently fall back to 32 if `canProcessSampleSize(64)` returns false.
     * Callers can probe support via `plugin.canProcessSampleSize(64)` after
     * load, or read the effective choice via `plugin.getSampleSize()`.
     */
    sampleSize?: 32 | 64;
    /**
     * VST3 process mode (default 'realtime').
     * - 'realtime' → `kRealtime` (live playback; events get `Event::kIsLive`).
     * - 'offline'  → `kOffline`  (offline render; `kIsLive` is cleared).
     * - 'prefetch' → `kPrefetch` (preview render; `kIsLive` is cleared).
     */
    processMode?: 'realtime' | 'offline' | 'prefetch';
}

export interface LoadOptions extends HostOptions {}

/**
 * Options accepted by `plugin.setProcessSetup(opts)`. All fields are
 * optional — only fields present in the JS object are applied. The new
 * setup takes effect on the next `setActive(true)` call (the plugin must
 * be inactive when `setProcessSetup` is invoked; otherwise it throws
 * `VST3_INVALID_PARAMETER`).
 */
export interface ProcessSetupOptions {
    /** Sample rate in Hz (must be > 0). */
    sampleRate?: number;
    /** Maximum block size in samples per process() call (must be > 0). */
    maxBlockSize?: number;
    /**
     * VST3 process mode.
     * - 'realtime' → `kRealtime` (live playback; events get `Event::kIsLive`).
     * - 'offline'  → `kOffline`  (offline render; `kIsLive` is cleared).
     * - 'prefetch' → `kPrefetch` (preview render; `kIsLive` is cleared).
     */
    processMode?: 'realtime' | 'offline' | 'prefetch';
    /**
     * Audio sample size to negotiate. Use 64 to request the `kSample64`
     * processing path; the host will silently fall back to 32 if
     * `canProcessSampleSize(64)` returns false (matching the load-time
     * negotiation behavior).
     */
    sampleSize?: 32 | 64;
}

export class Host {
    constructor(opts?: HostOptions);

    /**
     * Load a VST3 plugin from a `.vst3` module path.
     * Returns a PluginInstance ready to be activated.
     *
     * @throws {NstError} with code 'VST3_LOAD_FAILED' if the module cannot be loaded.
     */
    load(path: string, opts?: LoadOptions): PluginInstance;

    /** Returns a snapshot of the host options used by this Host. */
    getOptions(): Required<HostOptions>;

    /** Scan all platform-default VST3 plugin locations and return metadata. */
    static scanDefaultLocations(): PluginInfo[];

    /** Recursively scan a directory for `.vst3` modules. */
    static scanDirectory(path: string): PluginInfo[];

    /**
     * Load the factory of a single `.vst3` module and return metadata
     * (one entry, or an array if the module exports multiple classes).
     * Does NOT instantiate the DSP.
     */
    static inspectPlugin(path: string): PluginInfo | PluginInfo[];
}

// -------------------------------------------------------------------------
// PluginInfo (discovery metadata)
// -------------------------------------------------------------------------
export interface PluginFactoryInfo {
    vendor: string;
    url: string;
    email: string;
}

export interface PluginInfo {
    /** Filesystem path to the `.vst3` module. */
    path: string;
    /** Plugin class name. */
    name: string;
    /** Vendor (from class info). */
    vendor: string;
    /** Version string (e.g. "1.0.0"). */
    version: string;
    /** Class category (e.g. "Audio Module Class"). */
    category: string;
    /** Pipe-separated sub-categories (e.g. "Fx|Delay"). */
    subCategories: string;
    /** SDK version the plugin was built against (e.g. "VST 3.8.0"). */
    sdkVersion: string;
    /** 32-char lowercase hex representation of the class TUID. */
    classId: string;
    /** Class cardinality (typically 0x7FFFFFFF). */
    cardinality: number;
    factoryInfo: PluginFactoryInfo;
}

// -------------------------------------------------------------------------
// PluginInstance class
// -------------------------------------------------------------------------
export interface PluginInstanceInfo {
    name: string;
    vendor: string;
    version: string;
    category: string;
    subCategories: string;
    sdkVersion: string;
    /** 32-char lowercase hex class TUID. */
    classId: string;
    /** Total audio input channel count across all input buses. */
    numAudioInputs: number;
    /** Total audio output channel count across all output buses. */
    numAudioOutputs: number;
    /** Number of MIDI (event) input buses. */
    numMidiInputs: number;
    /** Number of MIDI (event) output buses. */
    numMidiOutputs: number;
    /** Parameter count reported by the edit controller. */
    parameterCount: number;
    /** True when an IEditController was created/connected. */
    hasController: boolean;
    /** True when component and controller are the same object (single-component effect). */
    isSingleComponent: boolean;
}

export interface ProcessBlock {
    /**
     * Per-channel input audio buffers. Each Float32Array (when sampleSize=32)
     * or Float64Array (when sampleSize=64) must have at least `numSamples`
     * elements. Two shapes are accepted:
     *
     *  - Single-bus (backward compat): a flat array of channel TypedArrays.
     *    These channels are routed to bus 0 of the plugin. This is the
     *    typical shape for stereo effects.
     *
     *  - Multi-bus: an array of buses, where each bus is itself an array of
     *    channel TypedArrays. Use this for plugins with sidechain inputs
     *    (e.g. compressor with kAux input) or multi-output instruments.
     *    Example: `[[mainL, mainR], [sidechainL, sidechainR]]`.
     *
     * All channels in a block must match the active sample size's TypedArrayType
     * (Float32Array for sampleSize=32, Float64Array for sampleSize=64).
     *
     * If undefined / null, all input buses are treated as silent.
     */
    inputs?: Float32Array[] | Float32Array[][] | Float64Array[] | Float64Array[][];
    /**
     * Per-channel output audio buffers. Same shape rules as `inputs`.
     * Will be filled by the plugin in place (zero-copy).
     */
    outputs?: Float32Array[] | Float32Array[][] | Float64Array[] | Float64Array[][];
    /**
     * Number of samples to process this block. Must be in
     * `[0, hostOptions.maxBlockSize]`. A value of `0` is a parameter-flush
     * block: no audio buffer resolution is performed, but the plugin's
     * `IAudioProcessor::process` is still called with `numSamples = 0` so
     * pending parameter changes can be flushed.
     */
    numSamples: number;
    /**
     * Optional per-input-bus silence bitmask. Bit `i` set means channel `i`
     * of that bus is silent; the plugin may skip processing those channels.
     * One entry per input bus, in bus-index order. Missing entries default
     * to 0 (no silence hint). When absent entirely, all silenceFlags are 0
     * (current behavior).
     */
    inputSilenceFlags?: number[];
}

/**
 * Result returned by `plugin.process()`. The `outputSilenceFlags` array has
 * one entry per output bus; bit `i` set means channel `i` is silent. Existing
 * callers that ignore the return value are unaffected — the engine doesn't
 * care whether you read it.
 */
export interface ProcessResult {
    /** Per-output-bus silence bitmask reported by the plugin. */
    outputSilenceFlags: number[];
}

export interface ParameterInfo {
    /** Parameter ID (uint32). */
    id: number;
    /** Human-readable title. */
    title: string;
    /** Short title for compact UIs. */
    shortTitle: string;
    /** Unit label (e.g. "Hz"). */
    units: string;
    /** Number of discrete steps (0 = continuous). */
    stepCount: number;
    /** Default normalized value in [0,1]. */
    defaultNormalizedValue: number;
    /** Associated unit ID. */
    unitId: number;
    /** Bitmask of ParameterFlags. */
    flags: number;
}

export interface ParameterChange {
    id: number;
    /** Normalized value in [0,1]. */
    value: number;
}

// Union of supported MIDI event shapes. Use the `type` field to discriminate.
// NoteOn / NoteOff / PolyPressure accept an optional `noteId` (default 0)
// that is propagated to `Event::noteOn.noteId` / `noteOff.noteId` /
// `polyPressure.noteId` so subsequent note-expression events can target the
// same note instance by ID.
export type MidiEvent =
    | { type: MidiEventType.NoteOn; channel: number; note: number; velocity: number; sampleOffset?: number; noteId?: number }
    | { type: MidiEventType.NoteOff; channel: number; note: number; velocity: number; sampleOffset?: number; noteId?: number }
    | { type: MidiEventType.PolyPressure; channel: number; note: number; pressure: number; sampleOffset?: number; noteId?: number }
    | { type: MidiEventType.Controller; channel: number; controllerNumber: number; controllerValue: number; sampleOffset?: number }
    | { type: MidiEventType.ProgramChange; channel: number; programNumber: number; sampleOffset?: number }
    | { type: MidiEventType.ChannelPressure; channel: number; pressure: number; sampleOffset?: number }
    | { type: MidiEventType.PitchBend; channel: number; pitchBend: number; sampleOffset?: number }
    | { type: MidiEventType.SysEx; sysEx: Uint8Array; sampleOffset?: number };

export interface MidiEventOut {
    type: MidiEventType;
    channel: number;
    note: number;
    velocity: number;
    controllerNumber: number;
    controllerValue: number;
    programNumber: number;
    pressure: number;
    pitchBend: number;
    sampleOffset: number;
    /** Present only for SysEx events. */
    sysEx?: Uint8Array;
}

// -------------------------------------------------------------------------
// IUnitInfo — units + programs
// -------------------------------------------------------------------------
/**
 * A single unit reported by the plugin's IUnitInfo. The root unit has
 * `parentUnitId = -1` (kNoParentUnitId in the SDK) and `programListId = -1`
 * when the unit has no associated program list.
 */
export interface UnitInfo {
    /** Unit ID (matches the SDK UnitID, typically a small int). */
    id: number;
    /** Human-readable unit name (UTF-8). */
    name: string;
    /** Associated program-list ID, or -1 if the unit has no program list. */
    programListId: number;
    /** Parent unit ID, or -1 for the root unit. */
    parentUnitId: number;
}

/** Metadata for a single program list exposed by IUnitInfo. */
export interface ProgramListInfo {
    /** Program-list ID. */
    id: number;
    /** Human-readable program-list name (UTF-8). */
    name: string;
    /** Number of programs in this list. */
    programCount: number;
}

/** Reference to a specific (mediaType, direction, busIndex) tuple. */
export interface BusRef {
    /** One of MediaType (Audio=0, Event=1). */
    mediaType: number;
    /** One of BusDirection (Input=0, Output=1). */
    direction: number;
    /** Zero-based bus index within (mediaType, direction). */
    busIndex: number;
}

// -------------------------------------------------------------------------
// Note expression
// -------------------------------------------------------------------------
/**
 * One note-expression type exposed by the plugin's INoteExpressionController.
 * The `typeId` matches one of the NoteExpressionTypeIds values (Volume=0,
 * Pan=1, Tuning=2, …) or a plugin-defined custom type.
 */
export interface NoteExpressionInfo {
    /** Note-expression type ID (use NoteExpressionTypeIds for the built-ins). */
    typeId: number;
    /** Human-readable title (UTF-8). */
    title: string;
    /** Short title for compact UIs (UTF-8). */
    shortTitle: string;
    /** Associated unit ID (0 = no unit). */
    unitId: number;
    /** Associated parameter ID, or -1 (kNoParamId) when not bound to a parameter. */
    associatedParameterId: number;
    /** Bitmask of NoteExpressionValueFlags (kIsAbsolute, kIsLive). */
    flags: number;
}

/** A single queued note-expression event targeting a specific noteId. */
export interface NoteExpressionEvent {
    /** Target note ID (set on a prior noteOn event). */
    noteId: number;
    /** Note-expression type ID (see NoteExpressionTypeIds). */
    typeId: number;
    /** Normalized value in [0, 1] (or [-1, 1] for bipolar types). */
    value: number;
    /** Sample offset within the next process() block (default 0). */
    sampleOffset?: number;
}

// -------------------------------------------------------------------------
// IKeyswitchController
// -------------------------------------------------------------------------
/** A single static keyswitch exposed by the plugin's IKeyswitchController. */
export interface KeyswitchInfo {
    /** Keyswitch type ID (see `Steinberg::Vst::KeyswitchTypeID`). */
    keyswitchType: number;
    /** Human-readable keyswitch title (UTF-8). */
    name: string;
    /** Short title for compact UIs (UTF-8). */
    shortName: string;
    /** Main keyswitch min (MIDI note 0–127). */
    keyswitchMin: number;
    /** Main keyswitch max (MIDI note 0–127). */
    keyswitchMax: number;
    /** Optional remapped key switch (default -1). */
    keyRemapped: number;
    /** Unit ID this keyswitch belongs to (-1 = no unit). */
    unitId: number;
    /** Reserved flags (currently 0). */
    flags: number;
}

// -------------------------------------------------------------------------
// Runtime bus management
// -------------------------------------------------------------------------
/** Information for a single bus reported by IComponent::getBusInfo. */
export interface BusInfo {
    /** One of MediaType (Audio=0, Event=1). */
    mediaType: number;
    /** One of BusDirection (Input=0, Output=1). */
    direction: number;
    /** Zero-based bus index within (mediaType, direction). */
    busIndex: number;
    /** Human-readable bus name (UTF-8). */
    name: string;
    /** Number of channels in this bus (1 = mono, 2 = stereo, etc.). */
    channelCount: number;
    /** One of BusType (Main=0, Aux=1). */
    busType: number;
    /** Bitmask of BusInfo flags (kDefaultActive = 1, kIsControlVoltage = 2). */
    flags: number;
    /** Whether the bus is currently active (reflects activateBus calls). */
    active: boolean;
    /**
     * Current speaker arrangement (one of SpeakerArrangement). Only meaningful
     * for audio buses; event buses report 0.
     */
    speakerArrangement: number;
}

/**
 * Result of `plugin.getRoutingInfo(srcBus, dstBus)`. Returns `null` when the
 * plugin's IComponent::getRoutingInfo returns kResultFalse.
 */
export interface RoutingInfo {
    /** Source bus index (echoes the requested srcBus). */
    srcBus: number;
    /** Destination bus index reported by the plugin. */
    dstBus: number;
    /** Destination bus media type (0 = audio, 1 = event). */
    busMediaType: number;
    /** Destination channel (-1 for all channels). */
    channel: number;
}

// -------------------------------------------------------------------------
// Configurable ProcessContext
// -------------------------------------------------------------------------
/**
 * Options accepted by `plugin.setProcessContext(opts)`. All fields are
 * optional — only fields present in the JS object are applied. Supplying a
 * value field also OR's the corresponding validity bit into
 * `ProcessContext.state` so the plugin can rely on it. Boolean fields
 * (`playing`, `cycleActive`, `recording`) toggle the corresponding transport
 * state bit on or off.
 *
 * Note: the SDK spells the continuous-time field `continousTimeSamples`
 * (one 'u'); we mirror that spelling here.
 */
export interface ProcessContextOptions {
    /** Tempo in BPM (double). Sets `kTempoValid` (and `kProjectTimeMusicValid`). */
    tempo?: number;
    /** Time signature numerator (int32, default 4). Sets `kTimeSigValid`. */
    timeSigNumerator?: number;
    /** Time signature denominator (int32, default 4). Sets `kTimeSigValid`. */
    timeSigDenominator?: number;
    /** Project time in samples (`projectTimeSamples`, int64). */
    samplePosition?: number;
    /** Bar position in musical time (`barPositionMusic`, double). Sets `kBarPositionValid`. */
    barPositionMusic?: number;
    /** Samples to the next MIDI clock tick (int32, 24 PPQ). */
    samplesToNextClock?: number;
    /** Transport: playing. Toggles `kPlaying` bit on/off. */
    playing?: boolean;
    /** Transport: cycle/loop active. Toggles `kCycleActive` bit on/off. */
    cycleActive?: boolean;
    /** Transport: recording. Toggles `kRecording` bit on/off. */
    recording?: boolean;
    /** System time in nanoseconds since epoch (int64). Sets `kSystemTimeValid`. */
    systemTime?: number;
    /** Continuous time in samples (`continousTimeSamples`, int64). Sets `kContinousTimeValid`. */
    continuousTimeSamples?: number;
}

/**
 * Snapshot returned by `plugin.getProcessContext()`. Mirrors
 * `ProcessContextOptions` plus the raw `state` bitmask so callers can
 * inspect which fields the host currently treats as valid.
 */
export interface ProcessContextSnapshot extends ProcessContextOptions {
    /** Boolean view of the `kPlaying` state bit. */
    playing: boolean;
    /** Boolean view of the `kCycleActive` state bit. */
    cycleActive: boolean;
    /** Boolean view of the `kRecording` state bit. */
    recording: boolean;
    /**
     * Raw `ProcessContext.state` bitmask (logical OR of
     * `kPlaying | kTempoValid | kTimeSigValid | kProjectTimeMusicValid |
     *  kBarPositionValid | kCycleActive | kRecording | kSystemTimeValid |
     *  kContinousTimeValid`).
     */
    state: number;
}

// -------------------------------------------------------------------------
// IInfoListener — Channel context info
// -------------------------------------------------------------------------
/**
 * Channel-context info accepted by `plugin.setChannelContextInfo(info)`. The
 * host builds an `IAttributeList` from these fields and passes it to the
 * plugin's `IInfoListener::setChannelContextInfos`. All fields are optional; only
 * present fields are forwarded.
 */
export interface ChannelContextInfo {
    /** Zero-based channel index within its bus (int32). */
    channelIdx?: number;
    /** Track / channel name (String128). */
    trackName?: string;
    /** Channel namespace name, e.g. the bus or group name (String128). */
    namespaceName?: string;
    /** Channel color as a packed 32-bit ARGB value (uint32). */
    channelColor?: number;
}

/**
 * Bitmask flags describing which ChannelContextInfo fields are present, as a
 * host-side convention (the VST3 SDK does not declare a canonical flags enum
 * for IInfoListener; it simply receives an IAttributeList). Useful for
 * filtering which fields a plugin should consume when reading an
 * IAttributeList.
 */
export enum ChannelContextInfoFlags {
    ContainsPluginName = 1 << 0,
    ContainsTrackName = 1 << 1,
    ContainsTrackColor = 1 << 2,
    ContainsTrackNamespace = 1 << 3,
    ContainsTrackNamespaceColor = 1 << 4,
}

export type RestartEventName = 'restart';
export type RestartListener = (flags: number) => void;

// Plugin→host event names delivered via the shared host-event TSFN.
// These correspond to plugin-initiated calls on the host-side
// IComponentHandler / IComponentHandler2 interfaces.
export type DirtyEventName = 'dirty';
export type DirtyListener = (dirty: boolean) => void;

export type GestureEventName = 'beginGesture' | 'endGesture';
export type GestureListener = (paramId: number) => void;

export type GroupEventName = 'startGroup' | 'finishGroup';
export type GroupListener = () => void;

// Union of all plugin→host event names supported by `plugin.on(...)`.
export type PluginEventName =
    | RestartEventName
    | DirtyEventName
    | GestureEventName
    | GroupEventName;

// Overloaded listener type for `plugin.on(...)`. The payload shape depends
// on the event name (see `PluginEventName` docs on `on()`).
export type PluginEventListener =
    | RestartListener
    | DirtyListener
    | GestureListener
    | GroupListener;

export class PluginInstance {
    // NOTE: Do not call `new PluginInstance(...)` directly — obtain instances
    // from `Host#load`. The constructor is exposed only because node-addon-api
    // ObjectWrap classes must be reflected on the JS prototype.
    constructor();

    /** Release all native resources (idempotent, safe to call twice). */
    dispose(): void;

    /** Snapshot of plugin metadata (read after `load`). */
    getInfo(): PluginInstanceInfo;

    /** Plugin-reported processing latency in samples. */
    getLatency(): number;

    /**
     * Activate or deactivate the plugin. Activating calls
     * `IAudioProcessor::setupProcessing`, `IComponent::setActive(true)`,
     * and `IAudioProcessor::setActive(true)` in order.
     */
    setActive(active: boolean): void;

    /** Toggle the processing state (must be called after setActive(true)). */
    setProcessing(processing: boolean): void;

    /**
     * Process one audio block. `inputs` and `outputs` must be arrays of
     * Float32Array (when `sampleSize` is 32) or Float64Array (when 64) whose
     * element length is >= `block.numSamples`. Audio is passed zero-copy to
     * the plugin.
     *
     * When `block.numSamples === 0` the call is a parameter-flush block: no
     * audio buffer resolution is performed (the `inputs` / `outputs` fields
     * may be omitted) and the plugin's `IAudioProcessor::process` is invoked
     * with `numSamples = 0` so pending parameter changes and events can be
     * flushed. The returned `outputSilenceFlags` array is empty in that case.
     *
     * On a normal block the returned `ProcessResult.outputSilenceFlags`
     * reports the per-output-bus silence bitmasks the plugin wrote. Existing
     * callers that ignore the return value are unaffected.
     *
     * @throws {NstError} with code 'VST3_NOT_ACTIVE' if not activated.
     * @throws {NstError} with code 'VST3_NOT_PROCESSING' if not in processing state.
     * @throws {NstError} with code 'VST3_INVALID_BUFFER' if buffer sizes mismatch.
     * @throws {NstError} with code 'VST3_PROCESSING_ERROR' if the plugin fails.
     */
    process(block: ProcessBlock): ProcessResult | void;

    //--- Sample size / process mode queries -----------------------------
    /**
     * Returns the active sample size (32 or 64) for the current
     * `setActive(true)` session. Reflects the value negotiated at load time:
     * if the user requested `sampleSize: 64` but `canProcessSampleSize(64)`
     * returned false, this returns 32.
     */
    getSampleSize(): 32 | 64;
    /**
     * Probe whether the plugin's `IAudioProcessor` supports the given sample
     * size. Always returns `true` for 32 (VST3 spec mandates it).
     */
    canProcessSampleSize(size: 32 | 64): boolean;
    /**
     * Plugin-reported tail length in samples. Returns `Number.POSITIVE_INFINITY`
     * when the plugin reports `kInfiniteTail` (e.g. an infinite reverb).
     * Otherwise returns the integer sample count.
     */
    getTailSamples(): number;

    //--- Parameters ------------------------------------------------------
    getParameterCount(): number;
    getParameterInfo(index: number): ParameterInfo;
    getParameter(id: number): number;
    setParameter(id: number, value: number): void;
    /**
     * Apply a batch of parameter changes atomically. All changes are queued
     * into a single `IParameterChanges` and delivered together in the next
     * `process()` call, so the plugin observes them in one block (no
     * intermediate renders). This is the host→plugin atomic batch primitive
     * — the VST3 SDK does not expose host→plugin `startGroupExecution` /
     * `finishGroupExecution`, so `setParameters` is the supported way to
     * commit multiple parameter edits as one logical group.
     */
    setParameters(changes: ParameterChange[]): void;
    formatParameter(id: number, value: number): string;
    /**
     * Parse a user-supplied string (e.g. "440 Hz") into the parameter's
     * normalized [0,1] value via `IEditController::getParamValueByString`.
     *
     * @throws {NstError} with code 'VST3_INVALID_PARAMETER' if the plugin
     *   refuses the string (returns `kResultFalse`).
     */
    parseParameter(id: number, str: string): number;
    /**
     * Convert a plain (display-unit) value to the normalized [0,1] value via
     * `IEditController::plainToNormalized`.
     */
    plainToNormalized(id: number, plain: number): number;
    /**
     * Convert a normalized [0,1] value to the plain (display-unit) value via
     * `IEditController::normalizedToPlain`.
     */
    normalizedToPlain(id: number, normalized: number): number;

    //--- MIDI / events ---------------------------------------------------
    addMidiEvent(event: MidiEvent): void;
    addMidiBytes(sampleOffset: number, bytes: Uint8Array): void;
    takeOutputEvents(): MidiEventOut[];
    clearEvents(): void;

    //--- State -----------------------------------------------------------
    /** Serialize plugin state to a Buffer (calls IComponent::getState). */
    saveState(): Buffer;
    /** Restore plugin state from a Buffer (calls IComponent::setState + setComponentState). */
    loadState(buffer: Buffer): void;

    //--- IUnitInfo (units + programs) -----------------------------------
    /**
     * Returns the number of units reported by `IUnitInfo`, or 0 if the plugin
     * does not implement `IUnitInfo`.
     */
    getUnitCount(): number;
    /**
     * Returns the `UnitInfo` for the given zero-based unit index (NOT a unit
     * ID — the SDK takes an index). The root unit is at index 0.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IUnitInfo`.
     */
    getUnitInfo(index: number): UnitInfo;
    /**
     * Returns the number of program lists reported by `IUnitInfo`, or 0 if
     * the plugin does not implement `IUnitInfo`.
     */
    getProgramListCount(): number;
    /**
     * Returns the `ProgramListInfo` for the given zero-based list index.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IUnitInfo`.
     */
    getProgramListInfo(listIndex: number): ProgramListInfo;
    /**
     * Returns the UTF-8 program name for the given (listId, programIndex).
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IUnitInfo`.
     */
    getProgramName(listId: number, programIndex: number): string;
    /**
     * Selects a program: calls `IUnitInfo::selectUnit(unitId)` followed by
     * `IUnitInfo::selectProgram(unitId, programIndex)`. The plugin updates its
     * parameters to the selected program.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IUnitInfo`.
     */
    selectProgram(unitId: number, programIndex: number): void;
    /**
     * Returns the currently selected unit ID, or 0 if the plugin does not
     * implement `IUnitInfo`.
     */
    getCurrentUnit(): number;
    /**
     * Resolves the unit ID for a specific (mediaType, direction, busIndex)
     * tuple via `IUnitInfo::getUnitByBusInfo`. Returns `null` if the plugin
     * does not implement `IUnitInfo` or the bus is not associated with a unit.
     */
    getUnitByBusInfo(bus: BusRef): number | null;

    //--- IProgramListData / IUnitData ----------------------------------
    /**
     * Reads per-program bulk data via `IProgramListData::getProgramData` and
     * returns it as a `Buffer`.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IProgramListData`.
     */
    getProgramData(listId: number, programIndex: number): Buffer;
    /**
     * Writes per-program bulk data via `IProgramListData::setProgramData`.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IProgramListData`.
     */
    setProgramData(listId: number, programIndex: number, buffer: Buffer): void;
    /**
     * Reads per-unit bulk data via `IUnitData::getUnitData` and returns it as
     * a `Buffer`.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IUnitData`.
     */
    getUnitData(unitId: number): Buffer;
    /**
     * Writes per-unit bulk data via `IUnitData::setUnitData`.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IUnitData`.
     */
    setUnitData(unitId: number, buffer: Buffer): void;

    //--- INoteExpressionController -------------------------------------
    /**
     * Returns the number of note-expression types for a given (busIndex,
     * channel), or 0 if the plugin does not implement
     * `INoteExpressionController`.
     */
    getNoteExpressionCount(busIndex: number, channel: number): number;
    /**
     * Returns the `NoteExpressionInfo` for the given (busIndex, channel,
     * index) tuple.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `INoteExpressionController`.
     */
    getNoteExpressionInfo(busIndex: number, channel: number, index: number): NoteExpressionInfo;
    /**
     * Queues a `kNoteExpressionValueEvent` for the next `process()` call. The
     * `noteId` must match a previously-queued noteOn so the plugin can route
     * the expression to the right note instance.
     */
    addNoteExpressionEvent(event: NoteExpressionEvent): void;

    //--- IKeyswitchController ------------------------------------------
    /**
     * Returns the number of static keyswitches for a given (busIndex,
     * channel), or 0 if the plugin does not implement
     * `IKeyswitchController`.
     */
    getKeyswitchCount(busIndex: number, channel: number): number;
    /**
     * Returns the `KeyswitchInfo` for the given (busIndex, channel, index)
     * tuple.
     *
     * @throws {NstError} with code 'VST3_UNKNOWN' if the plugin does not
     *   implement `IKeyswitchController`.
     */
    getKeyswitchInfo(busIndex: number, channel: number, index: number): KeyswitchInfo;

    //--- Runtime bus management ----------------------------------------
    /**
     * Returns one `BusInfo` entry per bus of the requested (mediaType,
     * direction) tuple. Iterates `IComponent::getBusCount` and calls
     * `getBusInfo` for each.
     */
    getBusList(mediaType: number, direction: number): BusInfo[];
    /**
     * Returns the `BusInfo` for a single bus. The `active` field reflects
     * cached inputBusInfos_ / outputBusInfos_ state (for audio buses) and
     * `speakerArrangement` is queried from `IAudioProcessor::getBusArrangement`.
     */
    getBusInfo(mediaType: number, direction: number, busIndex: number): BusInfo;
    /**
     * Activates or deactivates a single bus via `IComponent::activateBus`.
     * Must be called while `setActive(false)`; throws `VST3_INVALID_PARAMETER`
     * if the plugin is currently active.
     *
     * @throws {NstError} with code 'VST3_INVALID_PARAMETER' if `setActive(true)`.
     */
    activateBus(mediaType: number, direction: number, busIndex: number, active: boolean): void;

    //--- Speaker arrangement -------------------------------------------
    /**
     * Negotiates a new speaker-arrangement layout for inputs and outputs via
     * `IAudioProcessor::setBusArrangements`. Returns `true` on success (the
     * plugin accepted the layout) or `false` on `kResultFalse` (the previous
     * arrangement is unchanged). On success, cached bus info is refreshed.
     *
     * Both arrays must contain one entry per declared input/output bus.
     */
    setBusArrangement(inputs: number[], outputs: number[]): boolean;
    /**
     * Returns the current `SpeakerArrangement` for the given (direction,
     * busIndex) audio bus.
     */
    getBusArrangement(direction: number, busIndex: number): number;

    //--- Routing info --------------------------------------------------
    /**
     * Queries `IComponent::getRoutingInfo(srcBus, dstBus)` to determine how
     * an input bus routes to an output bus in multi-bus plugins. Returns
     * `null` if the plugin returns `kResultFalse` for the query.
     */
    getRoutingInfo(srcBus: number, dstBus: number): RoutingInfo | null;

    //--- Configurable ProcessContext -----------------------------------
    /**
     * Update the persistent `ProcessContext` from user-supplied fields. Each
     * present field is written into the context, and the corresponding state
     * validity bit is set (or, for the boolean transport fields, toggled
     * on/off). Fields not present are left at their current value.
     *
     * The changes take effect on the next `process()` call. The host keeps
     * the context across blocks: tempo, time signature, and transport flags
     * are sticky; transport positions advance per-block when `playing` is
     * set.
     *
     * @throws {NstError} with code 'VST3_INVALID_PARAMETER' if `opts` is not
     *   an object.
     */
    setProcessContext(opts: ProcessContextOptions): void;
    /**
     * Returns a snapshot of the current `ProcessContext` as a JS object.
     * Includes all configurable fields plus the raw `state` bitmask so
     * callers can inspect which fields are currently flagged valid. This is
     * a user-callable query (not the audio thread); allocating a JS object
     * here is fine.
     */
    getProcessContext(): ProcessContextSnapshot;

    //--- IProcessContextRequirements ----------------------------------
    /**
     * Returns the bitmask from
     * `IProcessContextRequirements::getProcessContextRequirements()` so the
     * user can see which `ProcessContext` fields the plugin actually
     * consumes. Returns `0` if the plugin does not implement the interface.
     * The mask is a logical OR of `ProcessContextRequirementFlags`. When
     * implemented, the host uses it to skip recomputation of unneeded
     * fields each block (and to always set the requested state bits).
     */
    getProcessContextRequirements(): number;

    //--- IAudioPresentationLatency ------------------------------------
    /**
     * Notifies the plugin of the output-presentation latency for a given
     * bus via `IAudioPresentationLatency::setAudioPresentationLatencySamples`.
     * Plugins use this for monitoring-side plugin delay compensation.
     *
     * @returns `true` if the plugin implements the interface and accepted
     *   the value; `false` if the plugin does not implement the interface.
     */
    setAudioPresentationLatency(busIndex: number, latencySamples: number): boolean;

    //--- IInfoListener -------------------------------------------------
    /**
     * Builds an `IAttributeList` from the supplied `info` object and passes
     * it to `IInfoListener::setChannelContextInfos` so the plugin can update its
     * notion of which track / channel it is loaded on. The plugin may use
     * the channel name, color, and namespace for display purposes.
     *
     * @returns `true` if the plugin implements the interface and accepted
     *   the list; `false` if the plugin does not implement the interface.
     */
    setChannelContextInfo(info: ChannelContextInfo): boolean;

    //--- IPrefetchableSupport -----------------------------------------
    /**
     * Returns `true` if the plugin implements `IPrefetchableSupport` and
     * reports itself as prefetchable via `isPrefetchable()`. Returns `false`
     * when the plugin doesn't implement the interface (the conservative
     * default — assume not prefetchable).
     */
    isPrefetchable(): boolean;

    //--- IEditController2 (host→plugin) -------------------------------
    /**
     * Forward the host's preferred knob interaction mode to the plugin's
     * `IEditController2::setKnobMode`. The mode is one of the `KnobMode`
     * enum values (0 = circular, 1 = relative circular, 2 = linear).
     *
     * Note: `IEditController2::openHelp` and `openAboutBox` are also
     * host→plugin method calls on the same interface; they are intentionally
     * not exposed here as events or methods (they are typically triggered by
     * the plugin's own UI rather than the host).
     *
     * @returns `true` if the plugin implements `IEditController2` and
     *   accepted the mode; `false` if the plugin does not implement the
     *   interface (no-op).
     */
    setKnobMode(mode: number): boolean;

    //--- Restart auto-react ---------------------------------------------
    /**
     * Re-query the affected SDK state for the given `RestartFlags` bitmask.
     * This is invoked automatically by the host BEFORE the JS `'restart'`
     * event fires (so users handling `'restart'` themselves do NOT need to
     * call this — the host's cached state is already refreshed by the time
     * the listener runs). It is exposed as a public method for the rare case
     * where the user wants to manually trigger a re-query, e.g. after
     * editing plugin state directly without going through the SDK.
     *
     * What each flag triggers:
     *  - `kLatencyChanged`            → no-op (`getLatency` always reads live).
     *  - `kIoChanged`                 → re-read all bus info via
     *                                    `IComponent::getBusCount` +
     *                                    `getBusInfo`, re-allocate per-bus
     *                                    buffers if bus counts changed, and
     *                                    re-run speaker-arrangement negotiation.
     *  - `kMidiCCAssignmentChanged`   → no-op (`IMidiMapping` is per-call).
     *  - `kRoutingInfoChanged`        → no-op (`getRoutingInfo` reads live).
     *  - `kParamTitlesChanged`        → no-op (`getParameterInfo` reads live).
     *  - `kParamValuesChanged`        → no-op (`getParameter` reads live).
     *  - `kNoteExpressionChanged`     → no-op (`getNoteExpressionInfo` reads live).
     *  - `kReloadComponent`           → no-op (user must dispose and re-load).
     *  - `kPrefetchableSupportChanged`→ no-op (`isPrefetchable` reads live).
     *  - `kIoTitlesChanged`           → no-op (bus titles read live).
     *
     * @param flags bitmask of `RestartFlags` values.
     */
    applyRestartFlags(flags: number): void;

    //--- Mutable ProcessSetup -------------------------------------------
    /**
     * Update the stored `ProcessSetup` fields for the next `setActive(true)`
     * call. The VST3 spec forbids changing `ProcessSetup` while the plugin
     * is active; callers must call `setActive(false)` first.
     *
     * For `sampleSize`, the host re-probes `canProcessSampleSize` with the
     * new size; if the plugin refuses, the host silently falls back to 32
     * (matching the load-time negotiation behavior) rather than throwing.
     *
     * @throws {NstError} with code 'VST3_INVALID_PARAMETER' if the plugin
     *   is currently active, or if `opts` is not an object.
     */
    setProcessSetup(opts: ProcessSetupOptions): void;

    //--- Events ----------------------------------------------------------
    /**
     * Register a listener for plugin-initiated events.
     *
     * Supported event names:
     *  - `'restart'`       — plugin requests a restart; listener receives
     *                        a bitmask of `RestartFlags`.
     *  - `'dirty'`         — plugin signals the host that the edit controller
     *                        state is dirty (`IComponentHandler2::setDirtyState`);
     *                        listener receives a `boolean` (true = dirty).
     *  - `'beginGesture'` — plugin began a parameter-edit gesture
     *                        (`IComponentHandler::beginEdit`); listener
     *                        receives the `ParamID` (number).
     *  - `'endGesture'`   — plugin ended a parameter-edit gesture
     *                        (`IComponentHandler::endEdit`); listener
     *                        receives the `ParamID` (number).
     *  - `'startGroup'`   — plugin started a group of related parameter
     *                        edits (`IComponentHandler2::startGroupExecution`);
     *                        listener receives no payload.
     *  - `'finishGroup'`  — plugin finished a group of related parameter
     *                        edits (`IComponentHandler2::finishGroupExecution`);
     *                        listener receives no payload.
     *
     * For the host→plugin direction, `setParameters` is the atomic batch
     * primitive (see its JSDoc); the SDK does not expose host→plugin group
     * execution.
     */
    on(event: PluginEventName, listener: PluginEventListener): void;

    /** [Symbol.dispose] — enables `using plugin = host.load(...)` syntax. */
    [Symbol.dispose](): void;
}

// -------------------------------------------------------------------------
// Enums (mirrored from the native addon)
// -------------------------------------------------------------------------
export enum ParameterFlags {
    NoFlags = 0,
    CanAutomate = 1 << 0,
    IsReadOnly = 1 << 1,
    IsWrapAround = 1 << 2,
    IsList = 1 << 3,
    IsHidden = 1 << 4,
    IsProgramChange = 1 << 15,
    IsBypass = 1 << 16,
}

export enum RestartFlags {
    ReloadComponent = 1 << 0,
    IoChanged = 1 << 1,
    ParamValuesChanged = 1 << 2,
    LatencyChanged = 1 << 3,
    ParamTitlesChanged = 1 << 4,
    MidiCCAssignmentChanged = 1 << 5,
    NoteExpressionChanged = 1 << 6,
    IoTitlesChanged = 1 << 7,
    PrefetchableSupportChanged = 1 << 8,
    RoutingInfoChanged = 1 << 9,
}

export enum BusType {
    Main = 0,
    Aux = 1,
}

export enum MediaType {
    Audio = 0,
    Event = 1,
}

export enum MidiEventType {
    NoteOff = 0,
    NoteOn = 1,
    PolyPressure = 2,
    Controller = 3,
    ProgramChange = 4,
    ChannelPressure = 5,
    PitchBend = 6,
    SysEx = 7,
}

// Symbolic sample sizes (mirrors Steinberg::Vst::SymbolicSampleSizes but
// exposed as plain integers matching the user-facing API).
export enum SampleSize {
    Sample32 = 32,
    Sample64 = 64,
}

// VST3 process modes (mirrors Steinberg::Vst::ProcessMode).
export enum ProcessMode {
    Realtime = 0,
    Offline = 1,
    Prefetch = 2,
}

// VST3 bus directions (mirrors Steinberg::Vst::BusDirection).
export enum BusDirection {
    Input = 0,
    Output = 1,
}

// VST3 knob modes (mirrors Steinberg::Vst::IEditController2::KnobMode).
// Pass one of these values to `plugin.setKnobMode(mode)` to forward the
// host's preferred knob interaction mode to the plugin's IEditController2.
export enum KnobMode {
    Circular = 0,
    RelativeCircular = 1,
    Linear = 2,
}

// Note-expression type IDs (mirrors Steinberg::Vst::NoteExpressionTypeIDs).
// The pinned SDK exposes six built-in type IDs (Volume, Pan, Tuning, Vibrato,
// Expression, Brightness); later SDKs add SoundPressure / SoundPowerOctave /
// Pitch, which are NOT exposed here. Plugins may also define custom type IDs
// at or above kCustomStart (= 100000).
export enum NoteExpressionTypeIds {
    Volume = 0,
    Pan = 1,
    Tuning = 2,
    Vibrato = 3,
    Expression = 4,
    Brightness = 5,
}

// VST3 speaker arrangements (mirrors Steinberg::Vst::SpeakerArr constants).
// Pass these values to setBusArrangement() and read them from
// getBusArrangement() / BusInfo.speakerArrangement. The numeric values are
// the SDK's integral arrangement bitmask codes (e.g. kStereo = kSpeakerL |
// kSpeakerR = 0x03) and are NOT a contiguous enumeration — always reference
// them by name, never by literal number.
export enum SpeakerArrangement {
    Mono,
    Stereo,
    _30Cine,
    _31Cine,
    _40Cine,
    _50,
    _51,
    _60Cine,
    _61Cine,
    _70Cine,
    _71Cine,
    _71_2,
    _71_4,
}

// VST3 process-context requirement flags (mirrors
// Steinberg::Vst::IProcessContextRequirements::Flags). Returned by
// `plugin.getProcessContextRequirements()` as a bitmask so the host can
// decide which ProcessContext fields to recompute each block. The bit values
// match the SDK exactly (kNeedSystemTime = 1<<0, ...).
export enum ProcessContextRequirementFlags {
    NeedSystemTime = 1 << 0,
    NeedContinousTimeSamples = 1 << 1,
    NeedProjectTimeMusic = 1 << 2,
    NeedBarPositionMusic = 1 << 3,
    NeedCycleMusic = 1 << 4,
    NeedSamplesToNextClock = 1 << 5,
    NeedTempo = 1 << 6,
    NeedTimeSignature = 1 << 7,
    NeedChord = 1 << 8,
    NeedFrameRate = 1 << 9,
    NeedTransportState = 1 << 10,
}

export const PluginCategory: {
    readonly Fx: 'Fx';
    readonly FxAnalyzer: 'Fx|Analyzer';
    readonly FxDelay: 'Fx|Delay';
    readonly FxDistortion: 'Fx|Distortion';
    readonly FxDynamics: 'Fx|Dynamics';
    readonly FxMastering: 'Fx|Mastering';
    readonly FxModulation: 'Fx|Modulation';
    readonly FxPitchShift: 'Fx|Pitch Shift';
    readonly FxRestoration: 'Fx|Restoration';
    readonly FxReverb: 'Fx|Reverb';
    readonly FxSurround: 'Fx|Surround';
    readonly FxTools: 'Fx|Tools';
    readonly Instrument: 'Instrument';
    readonly InstrumentSynth: 'Instrument|Synth';
    readonly InstrumentSynthSampler: 'Instrument|Synth|Sampler';
};

// Namespace-level interfaces for the const-enum mirrors exposed by the addon.
// (These describe the runtime objects; the TS `enum` declarations above
// provide compile-time type safety and are erased at emit time.)
export interface ParameterFlagsEnum { [k: string]: number }
export interface RestartFlagsEnum { [k: string]: number }
export interface BusTypeEnum { [k: string]: number }
export interface MediaTypeEnum { [k: string]: number }
export interface MidiEventTypeEnum { [k: string]: number }
export interface SampleSizeEnum { [k: string]: number }
export interface ProcessModeEnum { [k: string]: number }
export interface BusDirectionEnum { [k: string]: number }
export interface KnobModeEnum { [k: string]: number }
export interface NoteExpressionTypeIdsEnum { [k: string]: number }
export interface SpeakerArrangementEnum { [k: string]: number }
export interface ProcessContextRequirementFlagsEnum { [k: string]: number }
export interface ChannelContextInfoFlagsEnum { [k: string]: number }
export interface PluginCategoryEnum { [k: string]: string }

// -------------------------------------------------------------------------
// Errors
// -------------------------------------------------------------------------
export type NstErrorCode =
    | 'VST3_LOAD_FAILED'
    | 'VST3_FACTORY_MISSING'
    | 'VST3_COMPONENT_CREATION_FAILED'
    | 'VST3_CONTROLLER_MISSING'
    | 'VST3_NOT_ACTIVE'
    | 'VST3_NOT_PROCESSING'
    | 'VST3_FAULTED'
    | 'VST3_PLATFORM_UNSUPPORTED'
    | 'VST3_INVALID_PARAMETER'
    | 'VST3_INVALID_BUFFER'
    | 'VST3_PROCESSING_ERROR'
    | 'VST3_STATE_ERROR'
    | 'VST3_MIDI_ERROR'
    | 'VST3_UNKNOWN';

export interface NstError extends Error {
    code: NstErrorCode;
    cause?: unknown;
    runtimeTriple?: string;
    supportedTriples?: readonly string[];
}

}
