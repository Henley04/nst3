# nst3 API Reference

This document is the authoritative reference for the `nst3` module. It mirrors the hand-written [`index.d.ts`](../index.d.ts) 1:1; if you only need IntelliSense, install the package and your editor will pick up the types automatically.

- [Module Exports](#module-exports)
- [Host](#host)
- [PluginInstance](#plugininstance)
- [Types](#types)
- [Enums](#enums)
- [Error Codes](#error-codes)

---

## Module Exports

The module's default export is an object exposing the following surface.

```js
const nst3 = require('nst3');
// nst3.Host, nst3.PluginInstance, nst3.version, nst3.ParameterFlags, ...
```

### `version(): VersionInfo`

Returns version information about the native addon and its dependencies.

**Returns**: [`VersionInfo`](#versioninfo)

```js
const { version } = require('nst3');
console.log(version());
// { native: '0.1.0', vst3sdk: 'VST 3.8.0', napi: 8 }
```

### `Host`

The `Host` class. See [`Host`](#host) below.

### `PluginInstance`

The `PluginInstance` class. See [`PluginInstance`](#plugininstance) below.

### `SUPPORTED_TRIPLES: readonly string[]`

List of platform triples for which prebuilt binaries are shipped.

```js
const { SUPPORTED_TRIPLES } = require('nst3');
// ['win32-x64', 'darwin-x64', 'darwin-arm64', 'linux-x64']
```

### `NAPI_VERSION: number`

The Node-API version the binary was compiled against. Same value as `version().napi`.

### Enum Objects

The module exposes the following enum objects (see [Enums](#enums) for the full list of values):

- `ParameterFlags`
- `RestartFlags`
- `BusType`
- `MediaType`
- `MidiEventType`
- `PluginCategory`

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
const { Host } = require('nst3');
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
const { Host } = require('nst3');
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
const { RestartFlags } = require('nst3');
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

Activate or deactivate the plugin. When activating, nst3 calls:

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
const { MidiEventType } = require('nst3');
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

The shape of every error thrown by `nst3`. `code` is one of the `VST3_*` codes listed under [Error Codes](#error-codes). `runtimeTriple` and `supportedTriples` are populated only on `VST3_PLATFORM_UNSUPPORTED` errors thrown by the loader (`index.js`).

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
const { ParameterFlags } = require('nst3');
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
const { Host, PluginCategory } = require('nst3');
const reverb = Host.scanDefaultLocations()
  .filter(p => p.subCategories.split('|').includes(PluginCategory.FxReverb));
```

---

## Error Codes

Every error thrown by `nst3` carries a `code` property with one of the following values. The `code` is the stable machine-readable identifier; the `message` is human-readable and may change between versions.

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

**Thrown by**: the loader (`index.js`) at `require('nst3')` time.

The error object also includes `runtimeTriple` (the detected triple) and `supportedTriples` (the list of supported triples).

```js
try {
  require('nst3');
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

An unexpected error occurred that does not map to any of the above categories. Includes the underlying C++ exception message in `err.message` and the original error in `err.cause` (if available). If you encounter this, please [open an issue](https://github.com/Henley04/nst3/issues) with a reproduction.

**Thrown by**: any method, as a catch-all for unexpected SDK exceptions.
