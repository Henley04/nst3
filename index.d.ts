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
}

export interface LoadOptions extends HostOptions {}

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
     * Per-channel input audio buffers. Each Float32Array must have at
     * least `numSamples` elements. Two shapes are accepted:
     *
     *  - Single-bus (backward compat): a flat array of channel Float32Arrays.
     *    These channels are routed to bus 0 of the plugin. This is the
     *    typical shape for stereo effects.
     *
     *  - Multi-bus: an array of buses, where each bus is itself an array of
     *    channel Float32Arrays. Use this for plugins with sidechain inputs
     *    (e.g. compressor with kAux input) or multi-output instruments.
     *    Example: `[[mainL, mainR], [sidechainL, sidechainR]]`.
     *
     * If undefined / null, all input buses are treated as silent.
     */
    inputs?: Float32Array[] | Float32Array[][];
    /**
     * Per-channel output audio buffers. Same shape rules as `inputs`.
     * Will be filled by the plugin in place (zero-copy).
     */
    outputs?: Float32Array[] | Float32Array[][];
    /** Number of samples to process this block. Must be > 0 and <= hostOptions.maxBlockSize. */
    numSamples: number;
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
export type MidiEvent =
    | { type: MidiEventType.NoteOn; channel: number; note: number; velocity: number; sampleOffset?: number }
    | { type: MidiEventType.NoteOff; channel: number; note: number; velocity: number; sampleOffset?: number }
    | { type: MidiEventType.PolyPressure; channel: number; note: number; pressure: number; sampleOffset?: number }
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

export type RestartEventName = 'restart';
export type RestartListener = (flags: number) => void;

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
     * Float32Array whose element length is >= `block.numSamples`. Audio is
     * passed zero-copy to the plugin.
     *
     * @throws {NstError} with code 'VST3_NOT_ACTIVE' if not activated.
     * @throws {NstError} with code 'VST3_NOT_PROCESSING' if not in processing state.
     * @throws {NstError} with code 'VST3_INVALID_BUFFER' if buffer sizes mismatch.
     * @throws {NstError} with code 'VST3_PROCESSING_ERROR' if the plugin fails.
     */
    process(block: ProcessBlock): void;

    //--- Parameters ------------------------------------------------------
    getParameterCount(): number;
    getParameterInfo(index: number): ParameterInfo;
    getParameter(id: number): number;
    setParameter(id: number, value: number): void;
    setParameters(changes: ParameterChange[]): void;
    formatParameter(id: number, value: number): string;

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

    //--- Events ----------------------------------------------------------
    /**
     * Register a listener for plugin-initiated events.
     * Currently only `'restart'` is supported; the listener receives
     * a bitmask of `RestartFlags`.
     */
    on(event: RestartEventName, listener: RestartListener): void;

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
