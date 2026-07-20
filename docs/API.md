# nvst3-host API Reference

This document is the authoritative reference for the `nvst3-host` module. It mirrors the hand-written [`index.d.ts`](../index.d.ts) 1:1; if you only need IntelliSense, install the package and your editor will pick up the types automatically.

- [Module Exports](#module-exports)
- [Host](#host)
- [PluginInstance](#plugininstance)
- [0.2.0 — VST3 Spec Coverage](#020--vst3-spec-coverage)
- [0.3.0 — Audit-driven fixes](#030--audit-driven-fixes)
- [Types](#types)
- [Enums](#enums)
- [Error Codes](#error-codes)

---

## Module Exports

The module's default export is an object exposing the following surface.

```js
const nst3 = require('nvst3-host');
// nst3.Host, nst3.PluginInstance, nst3.version, nst3.ParameterFlags, ...
```

### `version(): VersionInfo`

Returns version information about the native addon and its dependencies.

**Returns**: [`VersionInfo`](#versioninfo)

```js
const { version } = require('nvst3-host');
console.log(version());
// { native: '0.1.0', vst3sdk: 'VST 3.8.0', napi: 8 }
```

### `Host`

The `Host` class. See [`Host`](#host) below.

### `PluginInstance`

The `PluginInstance` class. See [`PluginInstance`](#plugininstance) below.

### `SUPPORTED_TRIPLES: readonly string[]`

List of platform triples supported by nvst3-host. Prebuilt binaries are shipped for
`win32-x64`, `darwin-arm64`, and `linux-x64`; `darwin-x64` (Intel Macs) is
supported via source-build fallback.

```js
const { SUPPORTED_TRIPLES } = require('nvst3-host');
// ['win32-x64', 'darwin-x64', 'darwin-arm64', 'linux-x64']
```

### `NAPI_VERSION: number`

The Node-API version the binary was compiled against. Same value as `version().napi`.

### Enum Objects

The module exposes the following enum objects (see [Enums](#enums) and [New Enums](#new-enums) for the full list of values):

- `ParameterFlags`
- `RestartFlags`
- `BusType`
- `MediaType`
- `MidiEventType`
- `PluginCategory`
- `SampleSize` *(0.2.0)*
- `ProcessMode` *(0.2.0)*
- `BusDirection` *(0.2.0)*
- `KnobMode` *(0.2.0)*
- `NoteExpressionTypeIds` *(0.2.0)*
- `SpeakerArrangement` *(0.2.0)*
- `ProcessContextRequirementFlags` *(0.2.0)*
- `ChannelContextInfoFlags` *(0.2.0)*

---

## Host

```ts
class Host {
  constructor(opts?: HostOptions);
  load(path: string, opts?: LoadOptions): PluginInstance;
  getOptions(): Required<HostOptions>;
  static scanDefaultLocations(): PluginInfo[];
  static scanDirectory(path: string): PluginInfo[];
  static inspectPlugin(path: string): PluginInfo | PluginInfo[];
}
```

The `Host` owns the audio processing context (sample rate, block size, bus layout) and is the factory for `PluginInstance` objects. A single `Host` can load multiple plugins; each `PluginInstance` is independent.

### `new Host(opts?)`

Construct a host with the given audio format.

**Parameters**:

| Name                  | Type     | Default | Description                                       |
|-----------------------|----------|---------|---------------------------------------------------|
| `opts.sampleRate`     | `number` | `48000` | Sample rate in Hz.                                |
| `opts.maxBlockSize`   | `number` | `512`   | Maximum block size in samples per `process()` call. |
| `opts.audioInputs`    | `number` | `2`     | Number of audio input channels to allocate.       |
| `opts.audioOutputs`   | `number` | `2`     | Number of audio output channels to allocate.      |

**Returns**: `Host` instance.

**Throws**:

- `VST3_INVALID_PARAMETER` — if any option is out of range (e.g. `sampleRate <= 0`, `maxBlockSize <= 0`).

**Example**:

```js
const { Host } = require('nvst3-host');
const host = new Host({
  sampleRate: 44100,
  maxBlockSize: 256,
  audioInputs: 2,
  audioOutputs: 2,
});
```

### `host.load(path, opts?): PluginInstance`

Load a VST3 plugin from a `.vst3` module path and return a `PluginInstance` ready to be activated.

**Parameters**:

| Name   | Type           | Description                                                                       |
|--------|----------------|-----------------------------------------------------------------------------------|
| `path` | `string`       | Filesystem path to the `.vst3` module (bundle on macOS/Windows, directory on Linux). |
| `opts` | `LoadOptions?` | Optional overrides for `HostOptions` (same shape as the constructor).             |

**Returns**: [`PluginInstance`](#plugininstance)

**Throws**:

- `VST3_LOAD_FAILED` — the module cannot be loaded (file not found, wrong format, etc.).
- `VST3_FACTORY_MISSING` — the module loaded but no VST3 factory was exported.
- `VST3_COMPONENT_CREATION_FAILED` — the factory was found but the audio component could not be created.
- `VST3_CONTROLLER_MISSING` — the component was created but no `IEditController` could be obtained.
- `VST3_INVALID_PARAMETER` — `path` is not a string or `opts` is malformed.

**Example**:

```js
const plugin = host.load('/Library/Audio/Plug-Ins/VST3/SomePlugin.vst3');
console.log(plugin.getInfo().name);
```

### `host.getOptions(): Required<HostOptions>`

Returns a snapshot of the host options used by this `Host`. The returned object has all four fields populated (no `undefined`).

**Returns**: `Required<HostOptions>`

```js
const opts = host.getOptions();
// { sampleRate: 48000, maxBlockSize: 512, audioInputs: 2, audioOutputs: 2 }
```

### `Host.scanDefaultLocations(): PluginInfo[]` *(static)*

Scan all platform-default VST3 plugin locations and return metadata for every plugin found.

**Returns**: [`PluginInfo[]`](#plugininfo)

Default locations per platform:

| Platform | Paths |
|----------|-------|
| macOS    | `/Library/Audio/Plug-Ins/VST3/`, `~/Library/Audio/Plug-Ins/VST3/` |
| Windows  | `C:\Program Files\Common Files\VST3\`, `C:\Program Files (x86)\Common Files\VST3\`, `%LOCALAPPDATA%\Programs\Common\VST3\` |
| Linux    | `/usr/lib/vst3/`, `/usr/local/lib/vst3/`, `~/.vst3/` |

```js
const { Host } = require('nvst3-host');
const plugins = Host.scanDefaultLocations();
for (const p of plugins) {
  console.log(`${p.name} — ${p.vendor} — ${p.version}`);
}
```

### `Host.scanDirectory(path): PluginInfo[]` *(static)*

Recursively scan a directory for `.vst3` modules and return metadata for every class found.

**Parameters**:

| Name   | Type     | Description                         |
|--------|----------|-------------------------------------|
| `path` | `string` | Directory to scan recursively.      |

**Returns**: [`PluginInfo[]`](#plugininfo)

**Throws**:

- `VST3_LOAD_FAILED` — `path` does not exist or is not a directory.

```js
const plugins = Host.scanDirectory('/opt/my-vst3-folder');
```

### `Host.inspectPlugin(path): PluginInfo | PluginInfo[]` *(static)*

Load the factory of a single `.vst3` module and return metadata without instantiating the DSP component. Returns a single `PluginInfo` if the module exports one class, or an array if it exports multiple.

**Parameters**:

| Name   | Type     | Description                              |
|--------|----------|-----------------------------------------|
| `path` | `string` | Filesystem path to the `.vst3` module.  |

**Returns**: [`PluginInfo`](#plugininfo) `|` [`PluginInfo[]`](#plugininfo)

**Throws**:

- `VST3_LOAD_FAILED` — the module cannot be loaded.
- `VST3_FACTORY_MISSING` — the module loaded but no VST3 factory was exported.

```js
const info = Host.inspectPlugin('/path/to/Plugin.vst3');
console.log(info.name, info.classId);
```

---

## PluginInstance

```ts
class PluginInstance {
  constructor();  // do not call directly — obtain from host.load()
  dispose(): void;
  [Symbol.dispose](): void;
  getInfo(): PluginInstanceInfo;
  getLatency(): number;
  setActive(active: boolean): void;
  setProcessing(processing: boolean): void;
  process(block: ProcessBlock): void;
  getParameterCount(): number;
  getParameterInfo(index: number): ParameterInfo;
  getParameter(id: number): number;
  setParameter(id: number, value: number): void;
  setParameters(changes: ParameterChange[]): void;
  formatParameter(id: number, value: number): string;
  addMidiEvent(event: MidiEvent): void;
  addMidiBytes(sampleOffset: number, bytes: Uint8Array): void;
  takeOutputEvents(): MidiEventOut[];
  clearEvents(): void;
  saveState(): Buffer;
  loadState(buffer: Buffer): void;
  on(event: 'restart', listener: (flags: number) => void): void;
}
```

A `PluginInstance` wraps a live VST3 component + audio processor + (optional) edit controller. Instances are obtained from `host.load(...)`; **do not call `new PluginInstance(...)` directly** (the constructor is exposed only because `node-addon-api` ObjectWrap classes are reflected on the JS prototype).

### Lifecycle

#### `dispose(): void`

Release all native resources held by this instance (the `IComponent`, `IAudioProcessor`, `IEditController`, host context, and any queued events). Safe to call multiple times — subsequent calls are no-ops.

Calling any method on a disposed instance (other than `dispose()` itself) throws `VST3_FAULTED`.

```js
plugin.dispose();
plugin.dispose();  // no-op, no throw
```

#### `[Symbol.dispose](): void`

Alias for `dispose()`. Enables the `using` syntax (Node.js ≥ 20 with explicit resource management):

```js
{
  using plugin = host.load('/path/to/Plugin.vst3');
  // ... use plugin ...
}  // plugin[Symbol.dispose]() called here
```

#### `on(event, listener): void`

Register a listener for plugin-initiated events. Currently only `'restart'` is supported.

**Parameters**:

| Name       | Type                      | Description                                              |
|------------|---------------------------|---------------------------------------------------------|
| `event`    | `'restart'`               | Event name.                                              |
| `listener` | `(flags: number) => void` | Callback invoked with a bitmask of `RestartFlags`.       |

The listener is invoked asynchronously on the JavaScript thread via a `Napi::ThreadSafeFunction` — it is safe to call any `PluginInstance` method from inside it. The `flags` argument is a bitmask of one or more `RestartFlags` values (e.g. `RestartFlags.LatencyChanged | RestartFlags.ParamValuesChanged`).

```js
const { RestartFlags } = require('nvst3-host');
plugin.on('restart', (flags) => {
  if (flags & RestartFlags.LatencyChanged) {
    console.log('Latency changed:', plugin.getLatency());
  }
  if (flags & RestartFlags.ReloadComponent) {
    // The plugin wants to be fully reloaded — re-create the instance.
  }
});
```

### Metadata

#### `getInfo(): PluginInstanceInfo`

Returns a snapshot of plugin metadata. Safe to call immediately after `load()`.

**Returns**: [`PluginInstanceInfo`](#plugininstanceinfo)

```js
const info = plugin.getInfo();
console.log(info.name, info.numAudioInputs, info.numAudioOutputs);
```

#### `getLatency(): number`

Returns the plugin-reported processing latency in samples. The value is read from `IAudioProcessor::getLatencySamples` after `setupProcessing` is called (i.e. after `setActive(true)`).

**Returns**: `number`

```js
plugin.setActive(true);
console.log('Latency:', plugin.getLatency(), 'samples');
```

### Processing

#### `setActive(active: boolean): void`

Activate or deactivate the plugin. When activating, nvst3-host calls:

1. `IAudioProcessor::setupProcessing(setup)` with the host's `ProcessSetup`.
2. `IComponent::setActive(true)`.
3. `IAudioProcessor::setActive(true)`.

When deactivating, the reverse sequence runs. Must be called before `setProcessing(true)`.

**Parameters**:

| Name     | Type      | Description                              |
|----------|-----------|-----------------------------------------|
| `active` | `boolean` | `true` to activate, `false` to deactivate. |

**Throws**:

- `VST3_PROCESSING_ERROR` — if the SDK rejected the activation call.
- `VST3_FAULTED` — if the instance is already faulted or disposed.

#### `setProcessing(processing: boolean): void`

Toggle the processing state. Must be called after `setActive(true)` and before `process()`. The recommended sequence is `setActive(true)` → `setProcessing(true)` → `process(...)` → `setProcessing(false)` → `setActive(false)`.

**Parameters**:

| Name          | Type      | Description                                |
|---------------|-----------|-------------------------------------------|
| `processing`  | `boolean` | `true` to enter processing state, `false` to leave. |

**Throws**:

- `VST3_NOT_ACTIVE` — if the plugin is not currently activated.
- `VST3_PROCESSING_ERROR` — if the SDK rejected the call.
- `VST3_FAULTED` — if the instance is already faulted or disposed.

#### `process(block: ProcessBlock): void`

Process one audio block. `inputs` and `outputs` are arrays of `Float32Array` whose element length must be ≥ `block.numSamples`. Audio is passed **zero-copy** to the plugin — the `Float32Array`'s underlying memory is used directly as the channel buffer.

After `process` returns, any parameters set via `setParameter`/`setParameters` since the last call are consumed (their queue is cleared). Any MIDI events added via `addMidiEvent`/`addMidiBytes` since the last call are likewise consumed. Output events produced by the plugin are buffered and can be retrieved with `takeOutputEvents()`.

**Parameters**:

| Name                | Type              | Description                                                       |
|---------------------|-------------------|-----------------------------------------------------------------|
| `block.inputs`      | `Float32Array[]`  | Per-channel input audio. Each array must be ≥ `numSamples` long. |
| `block.outputs`     | `Float32Array[]`  | Per-channel output audio. Filled by the plugin.                  |
| `block.numSamples`  | `number`          | Number of samples to process. Must be `> 0` and `≤ maxBlockSize`. |

**Throws**:

- `VST3_NOT_ACTIVE` — if `setActive(true)` was not called.
- `VST3_NOT_PROCESSING` — if `setProcessing(true)` was not called.
- `VST3_INVALID_BUFFER` — if any `Float32Array` is shorter than `numSamples`, or if `numSamples` is out of range.
- `VST3_PROCESSING_ERROR` — if `IAudioProcessor::process` returned a failure code. After this, the instance enters the faulted state and all subsequent calls reject with `VST3_FAULTED`.
- `VST3_FAULTED` — if the instance is already faulted or disposed.

```js
const numSamples = 512;
const inputs  = [new Float32Array(numSamples), new Float32Array(numSamples)];
const outputs = [new Float32Array(numSamples), new Float32Array(numSamples)];
plugin.process({ inputs, outputs, numSamples });
```

### Parameters

#### `getParameterCount(): number`

Returns the number of parameters reported by the edit controller.

```js
const count = plugin.getParameterCount();
for (let i = 0; i < count; i++) {
  console.log(plugin.getParameterInfo(i));
}
```

#### `getParameterInfo(index): ParameterInfo`

Returns metadata for the parameter at the given zero-based index.

**Parameters**:

| Name    | Type     | Description                  |
|---------|----------|------------------------------|
| `index` | `number` | Zero-based parameter index.  |

**Returns**: [`ParameterInfo`](#parameterinfo)

**Throws**:

- `VST3_INVALID_PARAMETER` — if `index` is out of range.
- `VST3_FAULTED` — if the instance is disposed or faulted.

#### `getParameter(id): number`

Returns the current normalized value (in `[0, 1]`) of the parameter with the given ID.

**Parameters**:

| Name | Type     | Description             |
|------|----------|-------------------------|
| `id` | `number` | Parameter ID (uint32).  |

**Returns**: `number` — normalized value in `[0, 1]`.

**Throws**:

- `VST3_INVALID_PARAMETER` — if `id` is not a known parameter.
- `VST3_FAULTED` — if the instance is disposed or faulted.

#### `setParameter(id, value): void`

Set a parameter's normalized value (must be in `[0, 1]`). The change is queued and applied on the next `process()` call. If a `beginEdit` gesture is in flight (initiated by the plugin's own `IComponentHandler`), `performEdit` is also called.

**Parameters**:

| Name    | Type     | Description                     |
|---------|----------|---------------------------------|
| `id`    | `number` | Parameter ID (uint32).          |
| `value` | `number` | Normalized value in `[0, 1]`.   |

**Throws**:

- `VST3_INVALID_PARAMETER` — if `id` is unknown or `value` is out of range.
- `VST3_FAULTED` — if the instance is disposed or faulted.

#### `setParameters(changes): void`

Batch-apply multiple parameter changes. All changes land in the same parameter queue and are applied atomically on the next `process()` call.

**Parameters**:

| Name      | Type                | Description                              |
|-----------|---------------------|-----------------------------------------|
| `changes` | `ParameterChange[]` | Array of `{ id, value }` changes.       |

**Throws**:

- `VST3_INVALID_PARAMETER` — if any change has an unknown `id` or out-of-range `value`.
- `VST3_FAULTED` — if the instance is disposed or faulted.

```js
plugin.setParameters([
  { id: 0, value: 0.5 },
  { id: 1, value: 0.75 },
  { id: 2, value: 1.0 },
]);
plugin.process({ inputs, outputs, numSamples });
```

#### `formatParameter(id, value): string`

Format a parameter value to its human-readable string representation (e.g. `"440.0 Hz"` for a frequency parameter). Calls `IEditController::getParamStringByValue`.

**Parameters**:

| Name    | Type     | Description                     |
|---------|----------|---------------------------------|
| `id`    | `number` | Parameter ID (uint32).          |
| `value` | `number` | Normalized value in `[0, 1]`.   |

**Returns**: `string`

**Throws**:

- `VST3_INVALID_PARAMETER` — if `id` is unknown or `value` is out of range.
- `VST3_FAULTED` — if the instance is disposed or faulted.

```js
plugin.formatParameter(0, 0.5);  // e.g. "0.50" or "-6.0 dB"
```

### MIDI / Events

#### `addMidiEvent(event): void`

Schedule a MIDI event for the next `process()` call. The event is consumed after the call.

**Parameters**:

| Name    | Type        | Description                                       |
|---------|-------------|---------------------------------------------------|
| `event` | `MidiEvent` | A discriminated union tagged by `type`. See [`MidiEvent`](#midievent). |

**Throws**:

- `VST3_MIDI_ERROR` — if the event shape is invalid.
- `VST3_FAULTED` — if the instance is disposed or faulted.

```js
const { MidiEventType } = require('nvst3-host');
plugin.addMidiEvent({
  type: MidiEventType.NoteOn,
  channel: 0,
  note: 60,
  velocity: 0.9,
  sampleOffset: 0,
});
```

#### `addMidiBytes(sampleOffset, bytes): void`

Schedule a MIDI event from raw bytes. The status byte's high nibble is parsed and the appropriate VST3 `Event` is built per the VST3 MIDI 1.0 mapping.

**Parameters**:

| Name           | Type          | Description                                                              |
|----------------|---------------|--------------------------------------------------------------------------|
| `sampleOffset` | `number`      | Sample offset within the next block.                                     |
| `bytes`        | `Uint8Array`  | Raw MIDI bytes (status + data bytes). For SysEx, pass the full `F0 ... F7` payload. |

**Throws**:

- `VST3_MIDI_ERROR` — if `bytes` is empty or malformed.
- `VST3_FAULTED` — if the instance is disposed or faulted.

```js
// Note on, channel 0, middle C, velocity 100
plugin.addMidiBytes(0, Uint8Array.from([0x90, 60, 100]));
```

#### `takeOutputEvents(): MidiEventOut[]`

Drain and return all output events produced by the plugin during the most recent `process()` call. The internal buffer is cleared after this call returns.

**Returns**: [`MidiEventOut[]`](#midieventout)

```js
plugin.process({ inputs, outputs, numSamples });
const events = plugin.takeOutputEvents();
for (const e of events) {
  if (e.type === MidiEventType.NoteOff) {
    console.log(`Note off on channel ${e.channel}, note ${e.note}`);
  }
}
```

#### `clearEvents(): void`

Discard all pending input MIDI events without processing them. Useful if you've queued events and then decided not to call `process()`.

```js
plugin.clearEvents();
```

### State

#### `saveState(): Buffer`

Serialize the plugin's current state to a `Buffer`. Calls `IComponent::getState(stream)`. The returned buffer can be persisted to disk and later passed to `loadState()` to restore the plugin.

**Returns**: `Buffer`

**Throws**:

- `VST3_STATE_ERROR` — if the SDK failed to serialize the state.
- `VST3_FAULTED` — if the instance is disposed or faulted.

```js
const state = plugin.saveState();
require('fs').writeFileSync('state.bin', state);
```

#### `loadState(buffer): void`

Restore plugin state from a `Buffer`. Calls `IComponent::setState(stream)` and (if a controller is present) `IEditController::setComponentState(stream)`. After this call, all parameters reflect the restored state.

**Parameters**:

| Name    | Type     | Description                                            |
|---------|----------|--------------------------------------------------------|
| `buffer`| `Buffer` | State buffer previously returned by `saveState()`.     |

**Throws**:

- `VST3_STATE_ERROR` — if the buffer is malformed or the SDK rejected it.
- `VST3_INVALID_PARAMETER` — if `buffer` is not a `Buffer` or is empty.
- `VST3_FAULTED` — if the instance is disposed or faulted.

```js
const state = require('fs').readFileSync('state.bin');
plugin.loadState(state);
```

---

## 0.2.0 — VST3 Spec Coverage

This section documents the APIs added in 0.2.0 to bring the host to full VST3 SDK specification coverage. All additions are backward-compatible; existing callers are unaffected unless explicitly noted.

### Quick Start

A typical end-to-end session touches lifecycle, parameters, audio, and cleanup paths. The snippet uses the `using` keyword (Node.js ≥ 20 with explicit resource management) for automatic disposal:

```js
const { Host } = require('nvst3-host');

const host = new Host({ sampleRate: 48000, maxBlockSize: 512, sampleSize: 64 });
using plugin = host.load('/path/to/Plugin.vst3');

plugin.setActive(true);
plugin.setProcessing(true);

plugin.setParameter(0, 0.5);
plugin.addMidiEvent({ type: 1 /* NoteOn */, channel: 0, note: 60, velocity: 0.8, noteId: 42 });

const inputs  = [new Float64Array(512), new Float64Array(512)];
const outputs = [new Float64Array(512), new Float64Array(512)];
const result = plugin.process({ inputs, outputs, numSamples: 512 });
// result.outputSilenceFlags: number[] (per output bus)

plugin.setProcessing(false);
plugin.setActive(false);
// [Symbol.dispose]() fires at end of scope.
```

### Host & Lifecycle

The `Host` constructor and `host.load()` accept two additional fields on `HostOptions` / `LoadOptions`:

| Field          | Type                                            | Default      | Description                                                                                                                       |
|----------------|-------------------------------------------------|--------------|-----------------------------------------------------------------------------------------------------------------------------------|
| `sampleSize`   | `32 \| 64`                                      | `32`         | Audio sample size to negotiate. `64` enables `kSample64` (double-precision) processing. The host silently falls back to `32` if `canProcessSampleSize(64)` returns false. |
| `processMode`  | `'realtime' \| 'offline' \| 'prefetch'`         | `'realtime'` | VST3 process mode. `offline` and `prefetch` clear `Event::kIsLive` on all queued events.                                          |

```ts
interface HostOptions {
  sampleRate?: number;
  maxBlockSize?: number;
  audioInputs?: number;
  audioOutputs?: number;
  sampleSize?: 32 | 64;                                       // NEW
  processMode?: 'realtime' | 'offline' | 'prefetch';          // NEW
}
```

`host.load(path, opts)` returns a `PluginInstance` (no API change; new options accepted).

#### `ProcessSetupOptions`

Options accepted by `plugin.setProcessSetup(opts)`. All fields are optional — only fields present are applied. The plugin must be inactive when `setProcessSetup` is invoked (throws `VST3_INVALID_PARAMETER` otherwise).

```ts
interface ProcessSetupOptions {
  sampleRate?: number;
  maxBlockSize?: number;
  processMode?: 'realtime' | 'offline' | 'prefetch';
  sampleSize?: 32 | 64;
}
```

### PluginInstance — Lifecycle & Info

#### `applyRestartFlags(flags): void`

Re-query the affected SDK state for the given `RestartFlags` bitmask. The host invokes this automatically BEFORE the JS `'restart'` event fires — so users handling `'restart'` do NOT need to call this. It is exposed for the rare case where the user wants to manually trigger a re-query (e.g. after editing plugin state directly without going through the SDK).

What each flag triggers:

- `kLatencyChanged`            → no-op (`getLatency` always reads live).
- `kIoChanged`                 → re-read all bus info via `IComponent::getBusCount` + `getBusInfo`, re-allocate per-bus buffers if counts changed, re-run speaker-arrangement negotiation.
- `kMidiCCAssignmentChanged`   → no-op (`IMidiMapping` is per-call).
- `kRoutingInfoChanged`        → no-op (`getRoutingInfo` reads live).
- `kParamTitlesChanged`        → no-op (`getParameterInfo` reads live).
- `kParamValuesChanged`        → no-op (`getParameter` reads live).
- `kNoteExpressionChanged`     → no-op (`getNoteExpressionInfo` reads live).
- `kReloadComponent`           → no-op (user must dispose and re-load).
- `kPrefetchableSupportChanged`→ no-op (`isPrefetchable` reads live).
- `kIoTitlesChanged`           → no-op (bus titles read live).

**Parameters**:

| Name    | Type     | Description                              |
|---------|----------|------------------------------------------|
| `flags` | `number` | Bitmask of `RestartFlags` values.        |

#### `setProcessSetup(opts): void`

Update the stored `ProcessSetup` fields for the next `setActive(true)` call. The VST3 spec forbids changing `ProcessSetup` while the plugin is active; callers must call `setActive(false)` first.

For `sampleSize`, the host re-probes `canProcessSampleSize` with the new size; if the plugin refuses, the host silently falls back to `32` rather than throwing.

**Parameters**:

| Name   | Type                  | Description                          |
|--------|-----------------------|--------------------------------------|
| `opts` | `ProcessSetupOptions` | New setup fields (all optional).     |

**Throws**:

- `VST3_INVALID_PARAMETER` — if the plugin is currently active, or `opts` is not an object.

### Audio Processing

#### `process(block): ProcessResult | void`

The return type widens from `void` to `ProcessResult | void`; existing callers that ignore the return value are unaffected. `block.numSamples === 0` is now accepted as a parameter-flush block — no audio buffer resolution is performed and the plugin's `IAudioProcessor::process` is invoked with `numSamples = 0` so pending parameter changes and events can be flushed.

`ProcessBlock` gains an optional `inputSilenceFlags` field (one bitmask per input bus) and accepts both `Float32Array` and `Float64Array` channel buffers depending on the active `sampleSize`.

```ts
interface ProcessBlock {
  inputs?:  Float32Array[] | Float32Array[][] | Float64Array[] | Float64Array[][];
  outputs?: Float32Array[] | Float32Array[][] | Float64Array[] | Float64Array[][];
  numSamples: number;
  inputSilenceFlags?: number[];   // NEW — per input bus
}

interface ProcessResult {
  outputSilenceFlags: number[];   // per output bus
}
```

Bit `i` set in a silence flag means channel `i` of that bus is silent. Input flags propagate to `AudioBusBuffers::silenceFlags` on the SDK side; output flags report what the plugin wrote.

```js
const result = plugin.process({
  inputs, outputs, numSamples: 512,
  inputSilenceFlags: [0b11, 0],  // bus 0: both channels silent, bus 1: none
});
console.log(result.outputSilenceFlags);  // e.g. [0, 0]
```

#### `getSampleSize(): 32 | 64`

Returns the active sample size for the current `setActive(true)` session. Reflects the negotiated value: if the user requested `sampleSize: 64` but `canProcessSampleSize(64)` returned false, this returns `32`.

#### `canProcessSampleSize(size): boolean`

Probe whether the plugin's `IAudioProcessor` supports the given sample size. Always returns `true` for `32` (VST3 spec mandates it).

**Parameters**:

| Name   | Type       | Description               |
|--------|------------|---------------------------|
| `size` | `32 \| 64` | Sample size to probe.     |

#### `getTailSamples(): number`

Plugin-reported tail length in samples. Returns `Number.POSITIVE_INFINITY` when the plugin reports `kInfiniteTail` (e.g. an infinite reverb). Otherwise returns the integer sample count.

### Parameters

#### `parseParameter(id, str): number`

Parse a user-supplied string (e.g. `"440 Hz"`) into the parameter's normalized `[0,1]` value via `IEditController::getParamValueByString`.

**Parameters**:

| Name  | Type     | Description                |
|-------|----------|----------------------------|
| `id`  | `number` | Parameter ID (uint32).     |
| `str` | `string` | String to parse.           |

**Returns**: `number` — normalized value in `[0,1]`.

**Throws**:

- `VST3_INVALID_PARAMETER` — if the plugin refuses the string (returns `kResultFalse`).

#### `plainToNormalized(id, plain): number`

Convert a plain (display-unit) value to the normalized `[0,1]` value via `IEditController::plainToNormalized`.

**Parameters**:

| Name    | Type     | Description                          |
|---------|----------|--------------------------------------|
| `id`    | `number` | Parameter ID (uint32).               |
| `plain` | `number` | Plain value (Hz, dB, %, etc.).       |

**Returns**: `number` — normalized value in `[0,1]`.

#### `normalizedToPlain(id, normalized): number`

Convert a normalized `[0,1]` value to the plain (display-unit) value via `IEditController::normalizedToPlain`.

**Parameters**:

| Name         | Type     | Description                     |
|--------------|----------|---------------------------------|
| `id`         | `number` | Parameter ID (uint32).          |
| `normalized` | `number` | Normalized value in `[0,1]`.    |

**Returns**: `number` — plain value.

### MIDI / Events

The `MidiEvent` union variants `NoteOn`, `NoteOff`, and `PolyPressure` now accept an optional `noteId?: number` (default `0`) which propagates to `Event::noteOn.noteId` / `noteOff.noteId` / `polyPressure.noteId`. Subsequent `addNoteExpressionEvent({ noteId, ... })` calls target the same note instance by ID.

```ts
type MidiEvent =
  | { type: MidiEventType.NoteOn;       channel: number; note: number; velocity: number; sampleOffset?: number; noteId?: number }
  | { type: MidiEventType.NoteOff;      channel: number; note: number; velocity: number; sampleOffset?: number; noteId?: number }
  | { type: MidiEventType.PolyPressure; channel: number; note: number; pressure: number; sampleOffset?: number; noteId?: number }
  | { type: MidiEventType.Controller;      channel: number; controllerNumber: number; controllerValue: number; sampleOffset?: number }
  | { type: MidiEventType.ProgramChange;   channel: number; programNumber: number; sampleOffset?: number }
  | { type: MidiEventType.ChannelPressure; channel: number; pressure: number; sampleOffset?: number }
  | { type: MidiEventType.PitchBend;       channel: number; pitchBend: number; sampleOffset?: number }
  | { type: MidiEventType.SysEx;           sysEx: Uint8Array; sampleOffset?: number };
```

### State Persistence

`saveState()` now writes a versioned `NST3` envelope when the plugin's edit controller exposes its own state. `loadState()` auto-detects the envelope format and falls back to legacy single-blob for backward compatibility with 0.1.0 state files.

#### NST3 Envelope Format

| Offset  | Length | Field                    | Description                                                |
|---------|--------|--------------------------|------------------------------------------------------------|
| `0`     | 4      | Magic                    | ASCII `"NST3"` (`0x4E 0x53 0x54 0x33`).                    |
| `4`     | 1      | Version                  | `1`.                                                       |
| `5`     | 4      | Component state length   | Little-endian `uint32`.                                    |
| `9`     | N      | Component state bytes    | `IComponent::getState` blob.                               |
| `9+N`   | 4      | Controller state length  | Little-endian `uint32` (may be `0`).                       |
| `13+N`  | M      | Controller state bytes   | `IEditController::getState` blob (omitted when `M=0`).     |

#### `saveState(): Buffer`

Serialize the plugin's full state. Calls `IComponent::getState(stream)`; if the controller implements `IEditController::getState` and returns a non-empty blob, calls it as well and frames both into the versioned envelope. Plugins whose controller returns an empty blob still get the controller-state length prefix set to `0`.

#### `loadState(buffer): void`

Restore plugin state from a `Buffer`. The system detects the format:

- If the first 4 bytes are the `NST3` magic, parse the versioned envelope and call `IComponent::setState` + `IEditController::setComponentState` + (if controller state is present) `IEditController::setState`.
- Otherwise, treat the entire buffer as legacy component state (preserves backward compatibility with 0.1.0 state files): calls `IComponent::setState` + `IEditController::setComponentState`.

```js
const state = plugin.saveState();           // -> Buffer (NST3 envelope or legacy)
plugin.loadState(state);                    // round-trips
plugin.loadState(legacyBuffer);             // 0.1.0 single-blob still works
```

### Units & Programs (IUnitInfo)

#### `getUnitCount(): number`

Returns the number of units reported by `IUnitInfo`, or `0` if the plugin does not implement `IUnitInfo`.

#### `getUnitInfo(index): UnitInfo`

Returns the `UnitInfo` for the given zero-based unit **index** (NOT a unit ID — the SDK takes an index). The root unit is at index `0`.

**Parameters**:

| Name    | Type     | Description                |
|---------|----------|----------------------------|
| `index` | `number` | Zero-based unit index.     |

**Returns**: [`UnitInfo`](#unitinfo)

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IUnitInfo`.

#### `getProgramListCount(): number`

Returns the number of program lists reported by `IUnitInfo`, or `0` if the plugin does not implement `IUnitInfo`.

#### `getProgramListInfo(listIndex): ProgramListInfo`

Returns the `ProgramListInfo` for the given zero-based list index.

**Parameters**:

| Name        | Type     | Description                          |
|-------------|----------|--------------------------------------|
| `listIndex` | `number` | Zero-based program list index.       |

**Returns**: [`ProgramListInfo`](#programlistinfo)

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IUnitInfo`.

#### `getProgramName(listId, programIndex): string`

Returns the UTF-8 program name for the given `(listId, programIndex)`.

**Parameters**:

| Name           | Type     | Description                          |
|----------------|----------|--------------------------------------|
| `listId`       | `number` | Program-list ID.                     |
| `programIndex` | `number` | Zero-based index within the list.    |

**Returns**: `string`

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IUnitInfo`.

#### `selectProgram(unitId, programIndex): void`

Selects a program: calls `IUnitInfo::selectUnit(unitId)` followed by `IUnitInfo::selectProgram(unitId, programIndex)`. The plugin updates its parameters to the selected program.

**Parameters**:

| Name           | Type     | Description                          |
|----------------|----------|--------------------------------------|
| `unitId`       | `number` | Unit ID to select.                   |
| `programIndex` | `number` | Zero-based program index.            |

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IUnitInfo`.

#### `getCurrentUnit(): number`

Returns the currently selected unit ID, or `0` if the plugin does not implement `IUnitInfo`.

#### `getUnitByBusInfo(bus): number | null`

Resolves the unit ID for a specific `(mediaType, direction, busIndex)` tuple via `IUnitInfo::getUnitByBusInfo`. Returns `null` if the plugin does not implement `IUnitInfo` or the bus is not associated with a unit.

**Parameters**:

| Name  | Type     | Description                                       |
|-------|----------|---------------------------------------------------|
| `bus` | `BusRef` | `{ mediaType, direction, busIndex }` tuple.       |

**Returns**: `number | null`

### Program & Unit Bulk Data (IProgramListData / IUnitData)

#### `getProgramData(listId, programIndex): Buffer`

Reads per-program bulk data via `IProgramListData::getProgramData` and returns it as a `Buffer`.

**Parameters**:

| Name           | Type     | Description                          |
|----------------|----------|--------------------------------------|
| `listId`       | `number` | Program-list ID.                     |
| `programIndex` | `number` | Zero-based program index.            |

**Returns**: `Buffer`

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IProgramListData`.

#### `setProgramData(listId, programIndex, buffer): void`

Writes per-program bulk data via `IProgramListData::setProgramData`.

**Parameters**:

| Name           | Type     | Description                    |
|----------------|----------|--------------------------------|
| `listId`       | `number` | Program-list ID.               |
| `programIndex` | `number` | Zero-based program index.      |
| `buffer`       | `Buffer` | Bulk data to write.            |

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IProgramListData`.

#### `getUnitData(unitId): Buffer`

Reads per-unit bulk data via `IUnitData::getUnitData` and returns it as a `Buffer`.

**Parameters**:

| Name     | Type     | Description       |
|----------|----------|-------------------|
| `unitId` | `number` | Unit ID.          |

**Returns**: `Buffer`

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IUnitData`.

#### `setUnitData(unitId, buffer): void`

Writes per-unit bulk data via `IUnitData::setUnitData`.

**Parameters**:

| Name     | Type     | Description             |
|----------|----------|-------------------------|
| `unitId` | `number` | Unit ID.                |
| `buffer` | `Buffer` | Bulk data to write.     |

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IUnitData`.

### Note Expression (INoteExpressionController)

#### `getNoteExpressionCount(busIndex, channel): number`

Returns the number of note-expression types for a given `(busIndex, channel)`, or `0` if the plugin does not implement `INoteExpressionController`.

#### `getNoteExpressionInfo(busIndex, channel, index): NoteExpressionInfo`

Returns the `NoteExpressionInfo` for the given `(busIndex, channel, index)` tuple.

**Parameters**:

| Name       | Type     | Description                            |
|------------|----------|----------------------------------------|
| `busIndex` | `number` | Zero-based event input bus index.      |
| `channel`  | `number` | Zero-based channel within the bus.     |
| `index`    | `number` | Zero-based expression-type index.      |

**Returns**: [`NoteExpressionInfo`](#noteexpressioninfo)

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `INoteExpressionController`.

#### `addNoteExpressionEvent(event): void`

Queues a `kNoteExpressionValueEvent` for the next `process()` call. The `noteId` must match a previously-queued noteOn so the plugin can route the expression to the right note instance.

**Parameters**:

| Name    | Type                  | Description                                          |
|---------|-----------------------|------------------------------------------------------|
| `event` | `NoteExpressionEvent` | `{ noteId, typeId, value, sampleOffset? }`.          |

```js
plugin.addMidiEvent({ type: 1, channel: 0, note: 60, velocity: 0.9, noteId: 42 });
plugin.addNoteExpressionEvent({ noteId: 42, typeId: 0 /* Volume */, value: 0.5 });
```

### Keyswitches (IKeyswitchController)

#### `getKeyswitchCount(busIndex, channel): number`

Returns the number of static keyswitches for a given `(busIndex, channel)`, or `0` if the plugin does not implement `IKeyswitchController`.

#### `getKeyswitchInfo(busIndex, channel, index): KeyswitchInfo`

Returns the `KeyswitchInfo` for the given `(busIndex, channel, index)` tuple.

**Parameters**:

| Name       | Type     | Description                            |
|------------|----------|----------------------------------------|
| `busIndex` | `number` | Zero-based event input bus index.      |
| `channel`  | `number` | Zero-based channel within the bus.     |
| `index`    | `number` | Zero-based keyswitch index.            |

**Returns**: [`KeyswitchInfo`](#keyswitchinfo)

**Throws**:

- `VST3_UNKNOWN` — if the plugin does not implement `IKeyswitchController`.

### Runtime Bus Management

#### `getBusList(mediaType, direction): BusInfo[]`

Returns one `BusInfo` entry per bus of the requested `(mediaType, direction)` tuple. Iterates `IComponent::getBusCount` and calls `getBusInfo` for each.

**Parameters**:

| Name        | Type     | Description                                              |
|-------------|----------|----------------------------------------------------------|
| `mediaType` | `number` | One of `MediaType` (`Audio=0`, `Event=1`).               |
| `direction` | `number` | One of `BusDirection` (`Input=0`, `Output=1`).           |

**Returns**: [`BusInfo[]`](#businfo)

#### `getBusInfo(mediaType, direction, busIndex): BusInfo`

Returns the `BusInfo` for a single bus. The `active` field reflects cached `inputBusInfos_` / `outputBusInfos_` state (for audio buses) and `speakerArrangement` is queried from `IAudioProcessor::getBusArrangement`.

**Parameters**:

| Name        | Type     | Description                                              |
|-------------|----------|----------------------------------------------------------|
| `mediaType` | `number` | One of `MediaType`.                                      |
| `direction` | `number` | One of `BusDirection`.                                   |
| `busIndex`  | `number` | Zero-based bus index within `(mediaType, direction)`.    |

**Returns**: [`BusInfo`](#businfo)

#### `activateBus(mediaType, direction, busIndex, active): void`

Activates or deactivates a single bus via `IComponent::activateBus`. Must be called while `setActive(false)`.

**Parameters**:

| Name        | Type      | Description                                              |
|-------------|-----------|----------------------------------------------------------|
| `mediaType` | `number`  | One of `MediaType`.                                      |
| `direction` | `number`  | One of `BusDirection`.                                   |
| `busIndex`  | `number`  | Zero-based bus index within `(mediaType, direction)`.    |
| `active`    | `boolean` | `true` to activate, `false` to deactivate.               |

**Throws**:

- `VST3_INVALID_PARAMETER` — if `setActive(true)` is currently active.

### Speaker Arrangement

#### `setBusArrangement(inputs, outputs): boolean`

Negotiates a new speaker-arrangement layout for inputs and outputs via `IAudioProcessor::setBusArrangements`. Returns `true` on success (the plugin accepted the layout) or `false` on `kResultFalse` (the previous arrangement is unchanged). On success, cached bus info is refreshed.

Both arrays must contain one entry per declared input/output bus.

**Parameters**:

| Name      | Type       | Description                                            |
|-----------|------------|--------------------------------------------------------|
| `inputs`  | `number[]` | One `SpeakerArrangement` value per input bus.          |
| `outputs` | `number[]` | One `SpeakerArrangement` value per output bus.         |

**Returns**: `boolean`

#### `getBusArrangement(direction, busIndex): number`

Returns the current `SpeakerArrangement` for the given `(direction, busIndex)` audio bus.

**Parameters**:

| Name        | Type     | Description                          |
|-------------|----------|--------------------------------------|
| `direction` | `number` | One of `BusDirection`.               |
| `busIndex`  | `number` | Zero-based bus index.                |

**Returns**: `number` — one of `SpeakerArrangement`.

### Routing

#### `getRoutingInfo(srcBus, dstBus): RoutingInfo | null`

Queries `IComponent::getRoutingInfo(srcBus, dstBus)` to determine how an input bus routes to an output bus in multi-bus plugins. Returns `null` if the plugin returns `kResultFalse` for the query.

**Parameters**:

| Name      | Type     | Description                       |
|-----------|----------|-----------------------------------|
| `srcBus`  | `number` | Source (input) bus index.         |
| `dstBus`  | `number` | Destination (output) bus index.   |

**Returns**: [`RoutingInfo`](#routinginfo) `| null`

### Process Context

#### `setProcessContext(opts): void`

Update the persistent `ProcessContext` from user-supplied fields. Each present field is written into the context, and the corresponding state validity bit is set (or, for the boolean transport fields, toggled on/off). Fields not present are left at their current value.

Changes take effect on the next `process()` call. The host keeps the context across blocks: tempo, time signature, and transport flags are sticky; transport positions advance per-block when `playing` is set.

**Parameters**:

| Name   | Type                    | Description                          |
|--------|-------------------------|--------------------------------------|
| `opts` | `ProcessContextOptions` | Context fields to apply (optional).  |

**Throws**:

- `VST3_INVALID_PARAMETER` — if `opts` is not an object.

```js
plugin.setProcessContext({
  tempo: 140,
  timeSigNumerator: 6,
  timeSigDenominator: 8,
  playing: true,
});
```

#### `getProcessContext(): ProcessContextSnapshot`

Returns a snapshot of the current `ProcessContext` as a JS object. Includes all configurable fields plus the raw `state` bitmask so callers can inspect which fields are currently flagged valid.

**Returns**: `ProcessContextSnapshot`

#### `getProcessContextRequirements(): number`

Returns the bitmask from `IProcessContextRequirements::getProcessContextRequirements()` so the user can see which `ProcessContext` fields the plugin actually consumes. Returns `0` if the plugin does not implement the interface. The mask is a logical OR of `ProcessContextRequirementFlags`. When implemented, the host uses it to skip recomputation of unneeded fields each block.

**Returns**: `number` — bitmask of `ProcessContextRequirementFlags`.

### Information Interfaces

#### `setAudioPresentationLatency(busIndex, latencySamples): boolean`

Notifies the plugin of the output-presentation latency for a given bus via `IAudioPresentationLatency::setAudioPresentationLatencySamples`. Plugins use this for monitoring-side plugin delay compensation.

**Parameters**:

| Name             | Type     | Description                          |
|------------------|----------|--------------------------------------|
| `busIndex`       | `number` | Zero-based output bus index.         |
| `latencySamples` | `number` | Latency in samples.                  |

**Returns**: `boolean` — `true` if the plugin implements the interface and accepted the value; `false` if the plugin does not implement the interface.

#### `setChannelContextInfo(info): boolean`

Builds an `IAttributeList` from the supplied `info` object and passes it to `IInfoListener::setChannelContextInfos` so the plugin can update its notion of which track / channel it is loaded on. The plugin may use the channel name, color, and namespace for display purposes.

**Parameters**:

| Name   | Type                 | Description                                |
|--------|----------------------|--------------------------------------------|
| `info` | `ChannelContextInfo` | Channel-context fields (all optional).     |

**Returns**: `boolean` — `true` if the plugin implements `IInfoListener` and accepted the list; `false` otherwise.

#### `isPrefetchable(): boolean`

Returns `true` if the plugin implements `IPrefetchableSupport` and reports itself as prefetchable via `isPrefetchable()`. Returns `false` when the plugin doesn't implement the interface (the conservative default — assume not prefetchable).

#### `setKnobMode(mode): boolean`

Forward the host's preferred knob interaction mode to the plugin's `IEditController2::setKnobMode`. The mode is one of the `KnobMode` enum values.

**Parameters**:

| Name   | Type     | Description                                                |
|--------|----------|------------------------------------------------------------|
| `mode` | `number` | One of `KnobMode` (0=circular, 1=relative circular, 2=linear). |

**Returns**: `boolean` — `true` if the plugin implements `IEditController2` and accepted the mode; `false` if the plugin does not implement the interface (no-op).

### Events

`plugin.on(event, listener)` now supports six plugin→host event names. The listener is invoked asynchronously on the JavaScript thread via a `Napi::ThreadSafeFunction` — it is safe to call any `PluginInstance` method from inside it.

| Event            | Listener Signature          | Description                                                                                                                       |
|------------------|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| `'restart'`      | `(flags: number) => void`   | Plugin requests a restart; `flags` is a bitmask of `RestartFlags`. The host has already auto-reacted by the time the listener runs. |
| `'dirty'`        | `(dirty: boolean) => void`  | Plugin signals dirty state via `IComponentHandler2::setDirtyState`.                                                              |
| `'beginGesture'` | `(paramId: number) => void` | Plugin began a parameter-edit gesture via `IComponentHandler::beginEdit`.                                                        |
| `'endGesture'`   | `(paramId: number) => void` | Plugin ended a parameter-edit gesture via `IComponentHandler::endEdit`.                                                          |
| `'startGroup'`   | `() => void`                | Plugin started a group of related parameter edits via `IComponentHandler2::startGroupExecution`.                                 |
| `'finishGroup'`  | `() => void`                | Plugin finished a group of related parameter edits via `IComponentHandler2::finishGroupExecution`.                               |

```js
plugin.on('dirty',        (dirty) => console.log('Plugin dirty:', dirty));
plugin.on('beginGesture', (id)    => console.log('Begin gesture on param', id));
plugin.on('endGesture',   (id)    => console.log('End gesture on param', id));
```

Note: `IEditController2::openHelp` and `openAboutBox` are host→plugin method calls (the host invokes them on the plugin's controller), not plugin→host events. They are intentionally not exposed via `on()` — the host does not have a use case for invoking them programmatically without a GUI.

### New Types

#### `UnitInfo`

```ts
interface UnitInfo {
  id: number;            // Unit ID.
  name: string;          // Human-readable unit name (UTF-8).
  programListId: number; // Associated program-list ID, or -1 if none.
  parentUnitId: number;  // Parent unit ID, or -1 for the root unit.
  type: number;          // 0 = simple unit, 1 = hard-clipboard-group, etc.
}
```

A single unit reported by the plugin's `IUnitInfo`. The root unit has `parentUnitId = -1` (kNoParentUnitId) and `programListId = -1` when the unit has no associated program list.

#### `ProgramListInfo`

```ts
interface ProgramListInfo {
  id: number;            // Program-list ID.
  name: string;          // Human-readable program-list name (UTF-8).
  programCount: number;  // Number of programs in this list.
}
```

Metadata for a single program list exposed by `IUnitInfo`.

#### `BusRef`

```ts
interface BusRef {
  mediaType: number;  // One of MediaType (Audio=0, Event=1).
  direction: number;  // One of BusDirection (Input=0, Output=1).
  busIndex: number;   // Zero-based bus index within (mediaType, direction).
}
```

Reference to a specific `(mediaType, direction, busIndex)` tuple. Used by `getUnitByBusInfo`.

#### `NoteExpressionInfo`

```ts
interface NoteExpressionInfo {
  typeId: number;                // Note-expression type ID (see NoteExpressionTypeIds).
  title: string;                 // Human-readable title (UTF-8).
  shortTitle: string;            // Short title for compact UIs (UTF-8).
  unitId: number;                // Associated unit ID (0 = no unit).
  associatedParameterId: number; // Associated parameter ID, or -1 (kNoParamId).
  flags: number;                 // Bitmask of NoteExpressionValueFlags (kIsAbsolute, kIsLive).
}
```

One note-expression type exposed by the plugin's `INoteExpressionController`. The `typeId` matches one of the `NoteExpressionTypeIds` values or a plugin-defined custom type.

#### `NoteExpressionEvent`

```ts
interface NoteExpressionEvent {
  noteId: number;        // Target note ID (set on a prior noteOn event).
  typeId: number;        // Note-expression type ID (see NoteExpressionTypeIds).
  value: number;         // Normalized value in [0, 1] (or [-1, 1] for bipolar types).
  sampleOffset?: number; // Sample offset within the next process() block (default 0).
}
```

A single queued note-expression event targeting a specific `noteId`.

#### `KeyswitchInfo`

```ts
interface KeyswitchInfo {
  key: number;           // MIDI key number (e.g. 60 = C4).
  name: string;          // Human-readable keyswitch name (UTF-8).
  keyswitchType: number; // kKeySwitchOnOff=0, kKeySwitchOnOnly=1, etc.
}
```

A single static keyswitch exposed by the plugin's `IKeyswitchController`.

#### `BusInfo`

```ts
interface BusInfo {
  mediaType: number;          // One of MediaType (Audio=0, Event=1).
  direction: number;          // One of BusDirection (Input=0, Output=1).
  busIndex: number;           // Zero-based bus index within (mediaType, direction).
  name: string;               // Human-readable bus name (UTF-8).
  channelCount: number;       // Number of channels in this bus (1=mono, 2=stereo, etc.).
  busType: number;            // One of BusType (Main=0, Aux=1).
  flags: number;              // Bitmask of BusInfo flags (kDefaultActive=1, kIsControlVoltage=2).
  active: boolean;            // Whether the bus is currently active (reflects activateBus calls).
  speakerArrangement: number; // Current SpeakerArrangement (audio buses only; event buses report 0).
}
```

Information for a single bus reported by `IComponent::getBusInfo`.

#### `RoutingInfo`

```ts
interface RoutingInfo {
  srcBus: number;        // Source bus index (echoes the requested srcBus).
  dstBus: number;        // Destination bus index reported by the plugin.
  busMediaType: number;  // Destination bus media type (0 = audio, 1 = event).
  busType: number;       // Destination bus type (0 = main, 1 = aux).
}
```

Result of `getRoutingInfo`. Returns `null` when the plugin's `IComponent::getRoutingInfo` returns `kResultFalse`.

#### `ProcessContextOptions`

```ts
interface ProcessContextOptions {
  tempo?: number;                  // BPM. Sets kTempoValid (and kProjectTimeMusicValid).
  timeSigNumerator?: number;       // Default 4. Sets kTimeSigValid.
  timeSigDenominator?: number;     // Default 4. Sets kTimeSigValid.
  samplePosition?: number;         // Project time in samples (int64).
  barPositionMusic?: number;       // Bar position in musical time (double). Sets kBarPositionValid.
  samplesToNextClock?: number;     // Samples to next MIDI clock tick (int32, 24 PPQ).
  playing?: boolean;               // Toggles kPlaying bit.
  cycleActive?: boolean;           // Toggles kCycleActive bit.
  recording?: boolean;             // Toggles kRecording bit.
  systemTime?: number;             // Nanoseconds since epoch (int64). Sets kSystemTimeValid.
  continuousTimeSamples?: number;  // Continuous time in samples (int64). Sets kContinousTimeValid.
}
```

Options accepted by `setProcessContext(opts)`. All fields are optional. Supplying a value field also OR's the corresponding validity bit into `ProcessContext.state`. Boolean fields toggle the corresponding transport state bit on or off.

> Note: the SDK spells the continuous-time field `continousTimeSamples` (one 'u'); the TS surface mirrors that spelling.

#### `ProcessContextSnapshot`

Extends `ProcessContextOptions` with boolean views of the transport bits and the raw `state` bitmask:

```ts
interface ProcessContextSnapshot extends ProcessContextOptions {
  playing: boolean;
  cycleActive: boolean;
  recording: boolean;
  state: number;  // Raw ProcessContext.state bitmask.
}
```

Snapshot returned by `getProcessContext()`.

#### `ChannelContextInfo`

```ts
interface ChannelContextInfo {
  channelIdx?: number;          // Zero-based channel index within its bus (int32).
  trackName?: string;           // Track / channel name (String128).
  namespaceName?: string;       // Channel namespace, e.g. bus or group name (String128).
  channelColor?: number;        // Packed 32-bit ARGB value (uint32).
}
```

Channel-context info accepted by `setChannelContextInfo(info)`. The host builds an `IAttributeList` from these fields and passes it to the plugin's `IInfoListener::setChannelContextInfos`. All fields are optional; only present fields are forwarded.

### New Enums

#### `SampleSize`

Symbolic sample sizes (mirrors `Steinberg::Vst::SymbolicSampleSizes`).

| Name       | Value |
|------------|-------|
| `Sample32` | `32`  |
| `Sample64` | `64`  |

#### `ProcessMode`

VST3 process modes (mirrors `Steinberg::Vst::ProcessMode`).

| Name       | Value |
|------------|-------|
| `Realtime` | `0`   |
| `Offline`  | `1`   |
| `Prefetch` | `2`   |

#### `BusDirection`

VST3 bus directions (mirrors `Steinberg::Vst::BusDirection`).

| Name     | Value |
|----------|-------|
| `Input`  | `0`   |
| `Output` | `1`   |

#### `KnobMode`

VST3 knob modes (mirrors `Steinberg::Vst::IEditController2::KnobMode`). Pass one of these to `setKnobMode(mode)` to forward the host's preferred knob interaction mode.

| Name               | Value |
|--------------------|-------|
| `Circular`         | `0`   |
| `RelativeCircular` | `1`   |
| `Linear`           | `2`   |

#### `NoteExpressionTypeIds`

Note-expression type IDs (mirrors `Steinberg::Vst::NoteExpressionTypeIDs`). The pinned SDK exposes six built-in type IDs; later SDKs add `SoundPressure` / `SoundPowerOctave` / `Pitch`, which are NOT exposed here. Plugins may also define custom type IDs at or above `kCustomStart` (= `100000`).

| Name          | Value |
|---------------|-------|
| `Volume`      | `0`   |
| `Pan`         | `1`   |
| `Tuning`      | `2`   |
| `Vibrato`     | `3`   |
| `Expression`  | `4`   |
| `Brightness`  | `5`   |

#### `SpeakerArrangement`

VST3 speaker arrangements (mirrors `Steinberg::Vst::SpeakerArr` constants). Pass these to `setBusArrangement()` and read them from `getBusArrangement()` / `BusInfo.speakerArrangement`.

The underlying SDK values are bitmask speaker-arrangement codes, **not** a sequential enum — always reference entries by name, never by hardcoded integer.

| Name        | Notes |
|-------------|-------|
| `Mono`      | mono                              |
| `Stereo`    | L, R                              |
| `_30Cine`   | L, R, C                           |
| `_31Cine`   | L, R, C, LFE                      |
| `_40Cine`   | L, R, C, S                        |
| `_50`       | L, R, C, Ls, Rs                   |
| `_51`       | L, R, C, LFE, Ls, Rs              |
| `_60Cine`   | L, R, C, Ls, Rs, Cs               |
| `_61Cine`   | L, R, C, LFE, Ls, Rs, Cs          |
| `_70Cine`   | L, R, C, Ls, Rs, Lc, Rc           |
| `_71Cine`   | L, R, C, LFE, Ls, Rs, Lc, Rc      |
| `_71_2`     | L, R, C, LFE, Ls, Rs, LsR, RsR    |
| `_71_4`     | L, R, C, LFE, Ls, Rs, LsR, RsR, Lc, Rc, Cs, LFE2 |

#### `ProcessContextRequirementFlags`

VST3 process-context requirement flags (mirrors `Steinberg::Vst::IProcessContextRequirements::Flags`). Returned by `getProcessContextRequirements()` as a bitmask so the host can decide which `ProcessContext` fields to recompute each block. Bit values match the SDK exactly.

| Name                     | Value            |
|--------------------------|------------------|
| `NeedSystemTime`         | `1 << 0` (`1`)    |
| `NeedContinousTimeSamples` | `1 << 1` (`2`)  |
| `NeedProjectTimeMusic`   | `1 << 2` (`4`)    |
| `NeedBarPositionMusic`   | `1 << 3` (`8`)    |
| `NeedCycleMusic`         | `1 << 4` (`16`)   |
| `NeedSamplesToNextClock` | `1 << 5` (`32`)   |
| `NeedTempo`              | `1 << 6` (`64`)   |
| `NeedTimeSignature`      | `1 << 7` (`128`)  |
| `NeedChord`              | `1 << 8` (`256`)  |
| `NeedFrameRate`          | `1 << 9` (`512`)  |
| `NeedTransportState`     | `1 << 10` (`1024`)|

#### `ChannelContextInfoFlags`

Host-side convention bitmask describing which `ChannelContextInfo` fields are present. Note: the VST3 SDK at the pinned version has no corresponding `ChannelContextInfoFlags` enum — these flag constants are an nst3-side convention, not a mirror of an SDK enum.

| Name                          | Value       |
|-------------------------------|-------------|
| `ContainsPluginName`          | `1 << 0` (`1`)    |
| `ContainsTrackName`           | `1 << 1` (`2`)    |
| `ContainsTrackColor`          | `1 << 2` (`4`)    |
| `ContainsTrackNamespace`      | `1 << 3` (`8`)    |
| `ContainsTrackNamespaceColor` | `1 << 4` (`16`)   |

### Error Codes

No new error codes were added in 0.2.0. All new methods throw one of the existing 14 `VST3_*` codes (see [Error Codes](#error-codes)); the most common are `VST3_INVALID_PARAMETER` (bad argument or wrong state) and `VST3_UNKNOWN` (plugin does not implement the queried SDK interface).

---

## 0.3.0 — Audit-driven fixes

This section documents the API surface added in 0.3.0 to close the host-side audit findings. All changes are additive or correctness-only; existing callers are unaffected.

### Module-level

#### `SUPPORTED_TRIPLES: readonly string[]`

The list of platform triples the loader recognizes. As of 0.3.0 the value is:

```js
['win32-x64', 'darwin-x64', 'darwin-arm64', 'linux-x64', 'linux-arm64']
```

All five entries are kept for source-build discovery. Prebuilt binaries ship for `win32-x64`, `darwin-arm64`, `linux-x64`, and `linux-arm64` (4 binaries). `darwin-x64` (Intel Macs) and other x86 targets may not have prebuilts available because the corresponding GitHub Actions runners are no longer provided; the loader falls back to a source build via `node-gyp` in that case.

### PluginInstance — Diagnostics

#### `getPluginInfo(): object`

Returns a comprehensive snapshot of the plugin's state and capabilities. Intended for diagnostics and debugging — production callers should prefer the more targeted methods (`getInfo()`, `getLatency()`, `getParameterCount()`, etc.) which return typed objects.

**Returns**: an object with the following shape:

```ts
{
  // Static info (mirrors getInfo())
  name, vendor, version, category, subCategories, sdkVersion, classId: string,
  // Counts
  numAudioInputs, numAudioOutputs, numMidiInputs, numMidiOutputs: number,
  parameterCount: number,
  hasController, isSingleComponent: boolean,
  // Live state
  active, processing, faulted, disposed: boolean,
  sampleSize: 32 | 64,
  processMode: number,           // ProcessMode enum
  sampleRate, maxBlockSize: number,
  // Latency / tail / sample-size capability (present when audioProcessor is)
  latencySamples: number,
  tailSamples: number | Infinity,
  canProcessSample32, canProcessSample64: boolean,
  // Buses (one array per category)
  audioInputBuses:  Array<{ index, name, channelCount, busType, flags, active, speakerArrangement? }>,
  audioOutputBuses: Array<{ index, name, channelCount, busType, flags, active, speakerArrangement? }>,
  eventInputBuses:  Array<{ index, name, channelCount, busType, flags, active }>,
  eventOutputBuses: Array<{ index, name, channelCount, busType, flags, active }>,
  // Optional interface availability
  interfaces: {
    midiMapping, unitInfo, programListData, unitData, noteExpression,
    keyswitchController, processContextRequirements, audioPresentationLatency,
    prefetchableSupport, channelContextInfoListener, editController2: boolean
  }
}
```

**Throws**: `VST3_FAULTED` if the instance is disposed or faulted.

```js
const info = plugin.getPluginInfo();
if (!info.interfaces.noteExpression) {
  console.log('Plugin does not expose note expressions');
}
```

#### `getParameterTree(): object[]`

Returns every parameter reported by the edit controller, grouped by unit. Each unit is one element of the returned array. Parameters whose `unitId` does not match any declared unit (or whose plugin does not implement `IUnitInfo`) are collected under a synthetic root unit.

**Returns**: an array of unit nodes:

```ts
interface ParameterTreeNode {
  unitId: number;            // UnitID (0 = root when IUnitInfo not implemented)
  unitName: string;          // UTF-8 unit name, or "" for the synthetic root
  parentUnitId: number;      // -1 for the root, otherwise the parent's UnitID
  programListIndices: number[]; // Empty if the unit has no program lists
  parameters: Array<{
    id: number;              // ParameterID
    title: string;           // UTF-8 title
    shortTitle: string;      // UTF-8 short title (may be empty)
    unitId: number;          // The unit the parameter reports
    stepCount: number;       // 0 = continuous, >0 = discrete step count
    flags: number;           // ParameterFlags bitmask
    currentNormalizedValue: number; // 0..1, read live from the controller
  }>;
}
```

**Throws**: `VST3_FAULTED` if the instance is disposed or faulted, or `VST3_UNKNOWN` if no edit controller is present.

```js
const tree = plugin.getParameterTree();
for (const unit of tree) {
  console.log(`Unit ${unit.unitName || '<root>'} (${unit.parameters.length} params)`);
  for (const p of unit.parameters) {
    console.log(`  ${p.title}: ${p.currentNormalizedValue.toFixed(3)}`);
  }
}
```

### PluginInstance — Real-time system time

#### `setSystemTime(nanos: number): void`

Updates the host's cached system-time value used to populate `ProcessContext::systemTime` on subsequent `process()` calls. The cache is a `std::atomic<int64_t>` and the audio thread reads it without any syscall.

Pass `0` to disable `systemTime` population — the `kSystemTimeValid` bit in `ProcessContext::state` is cleared and `systemTime` stays at its previous value.

**Parameters**:

| Name    | Type     | Description                                                                 |
|---------|----------|-----------------------------------------------------------------------------|
| `nanos` | `number` | Nanoseconds since the Unix epoch (e.g. `Date.now() * 1e6`). Pass `0` to disable. |

**Throws**: `VST3_INVALID_PARAMETER` if `nanos` is not a finite number, or `VST3_FAULTED` if the instance is disposed or faulted.

The recommended pattern is to call this from a `setInterval` on the JS thread at the host's preferred granularity (e.g. every 50 ms). Plugins that declare `ProcessContextRequirementFlags::kSystemTime` will then receive a monotonic `systemTime` that advances between blocks without the host incurring a syscall on the audio thread.

```js
setInterval(() => plugin.setSystemTime(Date.now() * 1e6), 50);
// Later, to disable:
plugin.setSystemTime(0);
```

#### `process()` — `systemTime` semantics change

In 0.1.0 / 0.2.0, `process()` called `std::chrono::system_clock::now()` on the audio thread and stored the result in `ProcessContext::systemTime`, with `kSystemTimeValid` always set. As of 0.3.0:

- `systemTime` is read from the cache populated by `setSystemTime()`.
- `kSystemTimeValid` is only set when `setSystemTime()` has been called with a non-zero value since the last `setActive(true)`.
- If `setSystemTime()` has never been called, `systemTime` remains `0` and `kSystemTimeValid` stays clear. Plugins that gate behavior on `kSystemTimeValid` will then see the bit unset.

### PluginInstance — Stream attributes (host-side)

`BufferStream` now implements `Vst::IStreamAttributes` in addition to `IBStream`. Plugins that probe `IStreamAttributes` during `setState`/`getState` (to learn whether they are loading a project or a preset, and from which file path) will receive:

- `getFileName()` — the host-supplied UTF-16 file name (empty by default).
- `getAttributes()` — an `IAttributeList` lazily backed by the SDK's `HostAttributeList`.

The JS surface does not expose `BufferStream` directly — `saveState()` and `loadState()` continue to take and return `Buffer`. The host-side mutators (`setFileName`, `setStateType`, `setFilePath`) are C++-only and are used internally when a future `loadPresetFile()` API is added. For now, plugins that probe `IStreamAttributes` get a valid (empty) attribute list rather than `nullptr`, which fixes a class of "preset context unknown" warnings.

### CI matrix

The prebuilt matrix is now:

| Triple        | GitHub Actions runner       | Prebuilt shipped |
|---------------|------------------------------|------------------|
| `win32-x64`   | `windows-latest`             | Yes              |
| `darwin-x64`  | *(retired)*                  | No — source build on `darwin-arm64` |
| `darwin-arm64`| `macos-latest`                | Yes              |
| `linux-x64`   | `ubuntu-latest`              | Yes              |
| `linux-arm64` | `ubuntu-24.04-arm`           | Yes              |

`darwin-x64` (Intel Macs) and other x86 targets may also be unavailable on GitHub Actions; the loader's source-build fallback is the supported path for those triples.

### Out of scope (deferred to a future spec)

- **UMP / MIDI 2.0** — VST3.7+ Event List UMP variants are not yet wired through the JS MIDI API. Full UMP support requires extending the `Event` struct, mapping UMP 1.x/2.x message bytes to VST3 `LegacyMIDICCOutEvent` / `kNoteExpressionValue` events, and adding a new `addUmpMessage(group, status, bytes)` JS surface. This is a major API addition, not a bug fix.
- **Async / batch processing worker thread** — `process()` remains synchronous. VST3 plugins are NOT thread-safe by design: the SDK contract requires `IAudioProcessor::process` to be called from the host's single audio thread. The correct async pattern for VST3 hosts is to use a real-time audio thread on the C++ side with a lock-free ring buffer to the JS thread; that's a substantial architecture addition, deferred to a future spec.
- DAW-style integration test suite (VST3 Validator smoke test is the current substitute).
- Code signing / notarization for prebuilt macOS binaries.

---

## Types

### `HostOptions`

```ts
interface HostOptions {
  sampleRate?: number;     // default 48000
  maxBlockSize?: number;   // default 512
  audioInputs?: number;    // default 2
  audioOutputs?: number;   // default 2
}
```

Constructor options for `Host`. All fields are optional; omitted fields fall back to the defaults shown above.

### `LoadOptions`

```ts
interface LoadOptions extends HostOptions {}
```

Same shape as `HostOptions`. Passed to `host.load(path, opts)` to override the host's defaults for a single plugin.

### `Required<HostOptions>`

```ts
{
  sampleRate: number;
  maxBlockSize: number;
  audioInputs: number;
  audioOutputs: number;
}
```

Return type of `host.getOptions()` — all four fields populated.

### `PluginFactoryInfo`

```ts
interface PluginFactoryInfo {
  vendor: string;
  url: string;
  email: string;
}
```

Vendor information reported by the plugin's `IPluginFactory`.

### `PluginInfo`

```ts
interface PluginInfo {
  path: string;
  name: string;
  vendor: string;
  version: string;
  category: string;
  subCategories: string;
  sdkVersion: string;
  classId: string;
  cardinality: number;
  factoryInfo: PluginFactoryInfo;
}
```

Discovery metadata for a plugin class. Returned by `Host.scanDefaultLocations()`, `Host.scanDirectory()`, and `Host.inspectPlugin()`.

| Field           | Type                  | Description                                                              |
|-----------------|-----------------------|-------------------------------------------------------------------------|
| `path`          | `string`              | Filesystem path to the `.vst3` module.                                   |
| `name`          | `string`              | Plugin class name.                                                       |
| `vendor`        | `string`              | Vendor (from class info).                                                |
| `version`       | `string`              | Version string (e.g. `"1.0.0"`).                                         |
| `category`      | `string`              | Class category (e.g. `"Audio Module Class"`).                            |
| `subCategories` | `string`              | Pipe-separated sub-categories (e.g. `"Fx|Delay"`).                       |
| `sdkVersion`    | `string`              | SDK version the plugin was built against (e.g. `"VST 3.8.0"`).           |
| `classId`       | `string`              | 32-character lowercase hex representation of the class TUID.              |
| `cardinality`   | `number`              | Class cardinality (typically `0x7FFFFFFF`).                              |
| `factoryInfo`   | `PluginFactoryInfo`   | Vendor info from `IPluginFactory`.                                       |

### `PluginInstanceInfo`

```ts
interface PluginInstanceInfo {
  name: string;
  vendor: string;
  version: string;
  category: string;
  subCategories: string;
  sdkVersion: string;
  classId: string;
  numAudioInputs: number;
  numAudioOutputs: number;
  numMidiInputs: number;
  numMidiOutputs: number;
  parameterCount: number;
  hasController: boolean;
  isSingleComponent: boolean;
}
```

Live instance metadata returned by `plugin.getInfo()`.

| Field               | Type      | Description                                                            |
|---------------------|-----------|-----------------------------------------------------------------------|
| `name`              | `string`  | Plugin class name.                                                     |
| `vendor`            | `string`  | Vendor.                                                                |
| `version`           | `string`  | Version string.                                                        |
| `category`          | `string`  | Class category.                                                        |
| `subCategories`     | `string`  | Pipe-separated sub-categories.                                         |
| `sdkVersion`        | `string`  | SDK version the plugin was built against.                              |
| `classId`           | `string`  | 32-character lowercase hex class TUID.                                 |
| `numAudioInputs`    | `number`  | Total audio input channel count across all input buses.                |
| `numAudioOutputs`   | `number`  | Total audio output channel count across all output buses.              |
| `numMidiInputs`     | `number`  | Number of MIDI (event) input buses.                                    |
| `numMidiOutputs`    | `number`  | Number of MIDI (event) output buses.                                   |
| `parameterCount`    | `number`  | Parameter count reported by the edit controller.                        |
| `hasController`     | `boolean` | `true` if an `IEditController` was created/connected.                   |
| `isSingleComponent` | `boolean` | `true` if component and controller are the same object (single-component effect). |

### `ProcessBlock`

```ts
interface ProcessBlock {
  inputs: Float32Array[];
  outputs: Float32Array[];
  numSamples: number;
}
```

Argument to `plugin.process()`. `inputs` and `outputs` are arrays of `Float32Array`; the array length determines the number of channels wired to the plugin, and each `Float32Array` must have at least `numSamples` elements.

### `ParameterInfo`

```ts
interface ParameterInfo {
  id: number;
  title: string;
  shortTitle: string;
  units: string;
  stepCount: number;
  defaultNormalizedValue: number;
  unitId: number;
  flags: number;
}
```

| Field                    | Type     | Description                                                    |
|--------------------------|----------|----------------------------------------------------------------|
| `id`                     | `number` | Parameter ID (uint32).                                         |
| `title`                  | `string` | Human-readable title.                                          |
| `shortTitle`             | `string` | Short title for compact UIs.                                   |
| `units`                  | `string` | Unit label (e.g. `"Hz"`).                                      |
| `stepCount`              | `number` | Number of discrete steps (`0` = continuous).                   |
| `defaultNormalizedValue` | `number` | Default normalized value in `[0, 1]`.                          |
| `unitId`                 | `number` | Associated unit ID.                                            |
| `flags`                  | `number` | Bitmask of [`ParameterFlags`](#parameterflags).                |

### `ParameterChange`

```ts
interface ParameterChange {
  id: number;
  value: number;  // normalized, [0, 1]
}
```

Element of the array passed to `plugin.setParameters()`.

### `MidiEvent`

A discriminated union of MIDI event shapes. Use the `type` field to discriminate.

```ts
type MidiEvent =
  | { type: MidiEventType.NoteOn;          channel: number; note: number;             velocity: number;  sampleOffset?: number }
  | { type: MidiEventType.NoteOff;         channel: number; note: number;             velocity: number;  sampleOffset?: number }
  | { type: MidiEventType.PolyPressure;    channel: number; note: number;             pressure: number;  sampleOffset?: number }
  | { type: MidiEventType.Controller;      channel: number; controllerNumber: number; controllerValue: number; sampleOffset?: number }
  | { type: MidiEventType.ProgramChange;   channel: number; programNumber: number;    sampleOffset?: number }
  | { type: MidiEventType.ChannelPressure; channel: number; pressure: number;         sampleOffset?: number }
  | { type: MidiEventType.PitchBend;       channel: number; pitchBend: number;        sampleOffset?: number }
  | { type: MidiEventType.SysEx;           sysEx: Uint8Array;                        sampleOffset?: number };
```

Notes:

- `channel` is 0-indexed (0–15).
- `note` is a MIDI note number (0–127).
- `velocity`, `pressure` are normalized in `[0, 1]`.
- `controllerNumber` is 0–127; `controllerValue` is 0–127 (raw MIDI value, **not** normalized).
- `programNumber` is 0–127.
- `pitchBend` is a normalized value in `[-1, 1]` (where `0` = no bend).
- `sampleOffset` (optional) is the sample offset within the next `process()` block at which the event should take effect. Defaults to `0`.
- For `SysEx`, `sysEx` is a `Uint8Array` of the full payload (including or excluding the `F0`/`F7` status bytes — both are accepted by the VST3 SDK).

### `MidiEventOut`

```ts
interface MidiEventOut {
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
  sysEx?: Uint8Array;
}
```

Output event returned by `plugin.takeOutputEvents()`. All numeric fields are populated (with `0` if not applicable to the event `type`); `sysEx` is present only for SysEx events.

### `VersionInfo`

```ts
interface VersionInfo {
  native: string;
  vst3sdk: string;
  napi: number;
}
```

Return type of `version()`. See [Module Exports](#module-exports).

### `NstError`

```ts
interface NstError extends Error {
  code: NstErrorCode;
  cause?: unknown;
  runtimeTriple?: string;
  supportedTriples?: readonly string[];
}
```

The shape of every error thrown by `nvst3-host`. `code` is one of the `VST3_*` codes listed under [Error Codes](#error-codes). `runtimeTriple` and `supportedTriples` are populated only on `VST3_PLATFORM_UNSUPPORTED` errors thrown by the loader (`index.js`).

```js
try {
  host.load('/bad/path.vst3');
} catch (err) {
  console.log(err.code);          // 'VST3_LOAD_FAILED'
  console.log(err.message);       // human-readable details
}
```

### `NstErrorCode`

```ts
type NstErrorCode =
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
```

Union of all possible `code` values on an `NstError`. See [Error Codes](#error-codes) for descriptions.

### `RestartListener`

```ts
type RestartListener = (flags: number) => void;
```

The signature of a listener registered via `plugin.on('restart', listener)`. The `flags` argument is a bitmask of one or more [`RestartFlags`](#restartflags) values.

### `RestartEventName`

```ts
type RestartEventName = 'restart';
```

The set of event names accepted by `plugin.on(...)`. Currently only `'restart'` is supported.

---

## Enums

All enums are exposed as runtime objects on the module (e.g. `nst3.ParameterFlags.CanAutomate`). The TypeScript declarations also export them as proper `enum`s for compile-time safety.

### `ParameterFlags`

Bitmask flags describing a parameter's capabilities. Stored in `ParameterInfo.flags`.

| Name              | Numeric value | Description                          |
|-------------------|---------------|--------------------------------------|
| `NoFlags`         | `0`           | No flags set.                        |
| `CanAutomate`     | `1 << 0` (`1`)     | Parameter can be automated.     |
| `IsReadOnly`      | `1 << 1` (`2`)     | Parameter is read-only.        |
| `IsWrapAround`    | `1 << 2` (`4`)     | Parameter wraps around at the extremes. |
| `IsList`          | `1 << 3` (`8`)     | Parameter is a discrete list (use `stepCount`). |
| `IsHidden`        | `1 << 4` (`16`)    | Parameter is hidden from the UI. |
| `IsProgramChange` | `1 << 15` (`32768`)  | Parameter is a program change. |
| `IsBypass`        | `1 << 16` (`65536`)  | Parameter is the bypass switch. |

```js
const { ParameterFlags } = require('nvst3-host');
const info = plugin.getParameterInfo(0);
if (info.flags & ParameterFlags.CanAutomate) {
  console.log('Parameter is automatable');
}
```

### `RestartFlags`

Bitmask flags delivered to a `'restart'` listener. The bitmask may contain multiple values OR'd together.

| Name                          | Numeric value    |
|-------------------------------|------------------|
| `ReloadComponent`             | `1 << 0` (`1`)     |
| `IoChanged`                   | `1 << 1` (`2`)     |
| `ParamValuesChanged`          | `1 << 2` (`4`)     |
| `LatencyChanged`              | `1 << 3` (`8`)     |
| `ParamTitlesChanged`          | `1 << 4` (`16`)    |
| `MidiCCAssignmentChanged`     | `1 << 5` (`32`)    |
| `NoteExpressionChanged`       | `1 << 6` (`64`)    |
| `IoTitlesChanged`             | `1 << 7` (`128`)   |
| `PrefetchableSupportChanged`  | `1 << 8` (`256`)   |
| `RoutingInfoChanged`          | `1 << 9` (`512`)   |

### `BusType`

| Name   | Numeric value |
|--------|---------------|
| `Main` | `0`           |
| `Aux`  | `1`           |

### `MediaType`

| Name    | Numeric value |
|---------|---------------|
| `Audio` | `0`           |
| `Event` | `1`           |

### `MidiEventType`

Discriminator for `MidiEvent` and `MidiEventOut`.

| Name              | Numeric value |
|-------------------|---------------|
| `NoteOff`         | `0`           |
| `NoteOn`          | `1`           |
| `PolyPressure`    | `2`           |
| `Controller`      | `3`           |
| `ProgramChange`   | `4`           |
| `ChannelPressure` | `5`           |
| `PitchBend`       | `6`           |
| `SysEx`           | `7`           |

### `PluginCategory`

A const object mapping friendly names to the VST3 sub-category strings. Useful for comparing against `PluginInfo.subCategories` (note that `subCategories` is pipe-separated, so a plugin may have multiple).

| Key                      | Value                       |
|--------------------------|-----------------------------|
| `Fx`                     | `"Fx"`                      |
| `FxAnalyzer`             | `"Fx|Analyzer"`             |
| `FxDelay`                | `"Fx|Delay"`                |
| `FxDistortion`           | `"Fx|Distortion"`           |
| `FxDynamics`             | `"Fx|Dynamics"`             |
| `FxMastering`            | `"Fx|Mastering"`            |
| `FxModulation`           | `"Fx|Modulation"`           |
| `FxPitchShift`           | `"Fx|Pitch Shift"`          |
| `FxRestoration`          | `"Fx|Restoration"`          |
| `FxReverb`               | `"Fx|Reverb"`               |
| `FxSurround`             | `"Fx|Surround"`             |
| `FxTools`                | `"Fx|Tools"`                |
| `Instrument`             | `"Instrument"`              |
| `InstrumentSynth`        | `"Instrument|Synth"`        |
| `InstrumentSynthSampler` | `"Instrument|Synth|Sampler"` |

```js
const { Host, PluginCategory } = require('nvst3-host');
const reverb = Host.scanDefaultLocations()
  .filter(p => p.subCategories.split('|').includes(PluginCategory.FxReverb));
```

---

## Error Codes

Every error thrown by `nvst3-host` carries a `code` property with one of the following values. The `code` is the stable machine-readable identifier; the `message` is human-readable and may change between versions.

### `VST3_LOAD_FAILED`

The plugin module could not be loaded. Causes include: file not found, wrong format, corrupt bundle, missing entry point, or (rarely) a non-zero exit from `Module::create`.

**Thrown by**: `Host.load()`, `Host.inspectPlugin()`, `Host.scanDirectory()` (per-plugin), and the loader (`index.js`) when a supported triple has no prebuilt binary and source-build fallback fails.

### `VST3_FACTORY_MISSING`

The module loaded successfully but did not export a VST3 factory (`GetPluginFactory`). The file is on disk and is a valid shared library, but it is not a VST3 plugin.

**Thrown by**: `Host.load()`, `Host.inspectPlugin()`.

### `VST3_COMPONENT_CREATION_FAILED`

The factory was found, but creating the audio component (`IComponent`) failed. Typically means the plugin's class ID does not match `kVstAudioEffectClass`, or the plugin's constructor threw.

**Thrown by**: `Host.load()`.

### `VST3_CONTROLLER_MISSING`

The component was created, but no `IEditController` could be obtained — neither via `IComponent::getControllerClassId` + `factory->createInstance` nor via `queryInterface(IEditController)` on the component itself. The plugin is unusable for parameter automation.

**Thrown by**: `Host.load()`.

### `VST3_NOT_ACTIVE`

A processing method was called before `setActive(true)`. Activate the plugin first.

**Thrown by**: `setProcessing()`, `process()`.

### `VST3_NOT_PROCESSING`

`process()` was called before `setProcessing(true)`. Enter the processing state first.

**Thrown by**: `process()`.

### `VST3_FAULTED`

The instance is in a faulted state — a previous `process()` call returned a failure code, or the instance has been disposed. All subsequent method calls (other than `dispose()`) will reject with this code. The only recovery is to `dispose()` and create a new instance via `host.load()`.

**Thrown by**: every `PluginInstance` method after a fault.

### `VST3_PLATFORM_UNSUPPORTED`

The current platform/architecture triple is not in `SUPPORTED_TRIPLES`. The native binary cannot be loaded and source-build fallback is not attempted.

**Thrown by**: the loader (`index.js`) at `require('nvst3-host')` time.

The error object also includes `runtimeTriple` (the detected triple) and `supportedTriples` (the list of supported triples).

```js
try {
  require('nvst3-host');
} catch (err) {
  if (err.code === 'VST3_PLATFORM_UNSUPPORTED') {
    console.error(`Unsupported: ${err.runtimeTriple}`);
    console.error(`Supported: ${err.supportedTriples.join(', ')}`);
  }
}
```

### `VST3_INVALID_PARAMETER`

A JavaScript argument was invalid — wrong type, out of range, or unknown parameter ID.

**Thrown by**: `Host` constructor, `load()`, `setParameter()`, `setParameters()`, `getParameterInfo()`, `formatParameter()`, `loadState()`.

### `VST3_INVALID_BUFFER`

An audio buffer passed to `process()` was malformed — a `Float32Array` was shorter than `numSamples`, the input/output arrays were empty, or `numSamples` was out of range.

**Thrown by**: `process()`.

### `VST3_PROCESSING_ERROR`

The plugin's `IAudioProcessor` returned a failure code from `setupProcessing`, `setActive`, `setProcessing`, or `process`. After this, the instance enters the faulted state and all subsequent calls reject with `VST3_FAULTED`.

**Thrown by**: `setActive()`, `setProcessing()`, `process()`.

### `VST3_STATE_ERROR`

State serialization or deserialization failed. The plugin's `getState`/`setState` returned a non-success code, or the buffer was malformed.

**Thrown by**: `saveState()`, `loadState()`.

### `VST3_MIDI_ERROR`

A MIDI event was malformed. Causes include: unknown event type, missing required fields, out-of-range channel/note/velocity, or invalid SysEx payload.

**Thrown by**: `addMidiEvent()`, `addMidiBytes()`.

### `VST3_UNKNOWN`

An unexpected error occurred that does not map to any of the above categories. Includes the underlying C++ exception message in `err.message` and the original error in `err.cause` (if available). If you encounter this, please [open an issue](https://github.com/Henley04/nvst3-host/issues) with a reproduction.

**Thrown by**: any method, as a catch-all for unexpected SDK exceptions.
