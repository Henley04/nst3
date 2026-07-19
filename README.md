# nvst3-host

> Production-grade VST3 host for Node.js — load, automate, and process audio through any VST3 plugin with zero-copy buffers and prebuilt native binaries.

[![CI](https://github.com/Henley04/nvst3-host/workflows/CI/badge.svg)](https://github.com/Henley04/nvst3-host/actions)
[![npm version](https://img.shields.io/npm/v/nvst3-host.svg)](https://www.npmjs.com/package/nvst3-host)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Node.js](https://img.shields.io/badge/node-%3E%3D16.17-brightgreen)](https://nodejs.org/)

`nvst3-host` brings the VST3 plugin ecosystem to Node.js. Load any VST3 plugin, push audio through it with **zero-copy `Float32Array` buffers**, automate parameters, send MIDI events, save and restore plugin state, and react to plugin-initiated restart notifications — all from JavaScript, with no GUI, no DAW, and no compiler toolchain required at install time.

It is built on the **official Steinberg VST3 SDK (v3.8.0, MIT-licensed)** and ships **prebuilt native binaries** for Windows, macOS (Intel + Apple Silicon), and Linux.

## Features

- **Discovery** — scan platform-default VST3 locations, recursively scan an arbitrary directory, or inspect a single `.vst3` module without instantiating the DSP.
- **Lifecycle** — `load()`, `setActive(bool)`, `setProcessing(bool)`, `dispose()` (idempotent), and `[Symbol.dispose]` enabling the `using` keyword for scope-based cleanup.
- **Audio processing** — zero-copy `Float32Array` / `Float64Array` channel buffers; configurable sample size (32 or 64 bit); configurable process mode (`realtime` / `offline` / `prefetch`); parameter-flush blocks via `process({ numSamples: 0 })`; silence-flag propagation on input and output buses; tail-samples query (returns `Number.POSITIVE_INFINITY` for `kInfiniteTail`); latency query.
- **Parameters** — `getParameter` / `setParameter` / `setParameters` (atomic batch), `getParameterInfo`, `formatParameter`, `parseParameter` (string → normalized), `plainToNormalized` / `normalizedToPlain`; `ParameterFlags.IsProgramChange` honored by plugins that expose program-change parameters.
- **MIDI / events** — structured `MidiEvent`s (NoteOn / NoteOff / PolyPressure / Controller / ProgramChange / ChannelPressure / PitchBend / SysEx) plus raw `addMidiBytes`; optional `noteId` on NoteOn/NoteOff/PolyPressure for note-expression targeting; output event retrieval via `takeOutputEvents()`.
- **State persistence** — `saveState()` writes a versioned `NST3` envelope (4-byte magic, 1-byte version, length-prefixed component + controller blobs); `loadState()` auto-detects the envelope and falls back to legacy single-blob loading for backward compatibility with 0.1.0 state files.
- **Units & programs** — `IUnitInfo` enumeration (`getUnitCount`, `getUnitInfo`, `getProgramListCount`, `getProgramListInfo`, `getProgramName`, `selectProgram`, `getCurrentUnit`, `getUnitByBusInfo`); per-program and per-unit bulk data via `IProgramListData` / `IUnitData` (`getProgramData`, `setProgramData`, `getUnitData`, `setUnitData`).
- **Note expression** — `INoteExpressionController` enumeration (`getNoteExpressionCount`, `getNoteExpressionInfo`) and `addNoteExpressionEvent({ noteId, typeId, value, sampleOffset? })` queuing.
- **Keyswitches** — `IKeyswitchController` enumeration (`getKeyswitchCount`, `getKeyswitchInfo`).
- **Bus management** — runtime `getBusList`, `getBusInfo`, `activateBus` (toggle individual buses while inactive); speaker-arrangement negotiation via `setBusArrangement` / `getBusArrangement` with the `SpeakerArrangement` enum; routing info via `getRoutingInfo`.
- **Process context** — configurable tempo / time signature / transport (`playing`, `cycleActive`, `recording`) / `systemTime` / `continuousTimeSamples` via `setProcessContext` / `getProcessContext`; `IProcessContextRequirements` gating so the host skips recomputation of unneeded fields each block.
- **Information interfaces** — `IAudioPresentationLatencySamples` (`setAudioPresentationLatency`); `IInfoListener` (`setChannelContextInfo`); `IPrefetchableSupport` (`isPrefetchable`); `IEditController2` (`setKnobMode`).
- **Restart auto-react** — when the plugin calls `restartComponent`, the host automatically re-queries the affected SDK state (latency, bus info, etc.) BEFORE the JS `restart` event fires; `applyRestartFlags(flags)` is exposed for manual re-query.
- **Mutable ProcessSetup** — `setProcessSetup({ sampleRate?, maxBlockSize?, processMode?, sampleSize? })` re-negotiates the setup for the next `setActive(true)` (requires `setActive(false)` first).
- **Accurate PlugInterfaceSupport** — the host advertises exactly the 13 interfaces it implements via a custom `IPlugInterfaceSupport` (no GUI-only interfaces such as `IPlugView` / `IPlugFrame`).
- **Plugin→host events** — `on('restart')`, `on('dirty')`, `on('beginGesture')`, `on('endGesture')`, `on('startGroup')`, `on('finishGroup')`, all delivered safely across threads via a `Napi::ThreadSafeFunction`.
- **Error handling** — typed `NstError` with `code`, `cause`, `runtimeTriple`, and `supportedTriples` fields; 14 stable error codes covering load, activation, processing, state, MIDI, platform, and unexpected-fault conditions.
- **Cross-platform prebuilt binaries** — `npm install nvst3-host` ships native `.node` files via `prebuildify` for `darwin-x64`, `darwin-arm64`, `linux-x64`, `linux-arm64`, `win32-x64`, and `win32-ia32`; no toolchain needed for end users.
- **MIT-licensed end-to-end** — both `nvst3-host` and the bundled VST3 SDK are MIT-licensed (since SDK v3.7.7), so there are no licensing concerns for commercial or closed-source use.
- **Strong TypeScript types** — a hand-written `index.d.ts` mirrors the native surface 1:1, including all 14 enums and full JSDoc, for editor IntelliSense.

## Installation

```bash
npm install nvst3-host
```

No compiler toolchain required — prebuilt binaries are shipped for:

| Platform            | Triple          |
|---------------------|-----------------|
| Windows x64         | `win32-x64`     |
| macOS Apple Silicon | `darwin-arm64`  |
| Linux x64           | `linux-x64`     |

If no prebuilt binary matches your runtime, `node-gyp-build` automatically falls back to a source build (`node-gyp rebuild`) — this requires Node.js headers and a C++17 compiler.

### Requirements

- Node.js `>= 16.17`
- One of the supported platform triples above
- (For source builds only) a C++17 compiler and Python 3 — see [Building from Source](#building-from-source)

## Quick Start

```js
const { Host } = require('nvst3-host');

// 1. Create a host with the audio format you want to process at.
const host = new Host({
  sampleRate: 48000,
  maxBlockSize: 512,
  audioInputs: 2,   // stereo in
  audioOutputs: 2,  // stereo out
});

// 2. Load a VST3 plugin (path to the .vst3 bundle/module).
const plugin = host.load('/path/to/SomePlugin.vst3');
console.log(plugin.getInfo());

// 3. Activate and start processing.
plugin.setActive(true);
plugin.setProcessing(true);

// 4. Tweak a parameter by ID (normalized 0..1).
const paramInfo = plugin.getParameterInfo(0);
plugin.setParameter(paramInfo.id, 0.75);

// 5. Process a block of audio — buffers are passed zero-copy.
const numSamples = 512;
const inputs = [
  new Float32Array(numSamples),  // left  (silence in)
  new Float32Array(numSamples),  // right
];
const outputs = [
  new Float32Array(numSamples),  // left  (filled by plugin)
  new Float32Array(numSamples),  // right
];
plugin.process({ inputs, outputs, numSamples });

// 6. Dispose when done (idempotent — safe to call twice).
plugin.dispose();
```

If you are on Node.js ≥ 20 with explicit resource management enabled, you can use the `using` keyword for automatic cleanup:

```js
{
  using plugin = host.load('/path/to/SomePlugin.vst3');
  // ... use plugin ...
  // plugin[Symbol.dispose]() is called automatically at end of scope.
}
```

## API

The full surface is documented in [`docs/API.md`](docs/API.md). The two main classes are:

### `Host`

```js
const { Host } = require('nvst3-host');
```

- `new Host(opts?)` — construct a host with `sampleRate`, `maxBlockSize`, `audioInputs`, `audioOutputs` (all optional, with sensible defaults).
- `host.load(path, opts?)` → `PluginInstance` — load and instantiate a VST3 plugin.
- `host.getOptions()` → `Required<HostOptions>` — snapshot of the host's audio format.
- `Host.scanDefaultLocations()` → `PluginInfo[]` — scan platform-default VST3 directories.
- `Host.scanDirectory(path)` → `PluginInfo[]` — recursively scan an arbitrary directory.
- `Host.inspectPlugin(path)` → `PluginInfo | PluginInfo[]` — read metadata from a single `.vst3` without instantiating it.

### `PluginInstance`

Obtained from `host.load(...)`. Never call `new PluginInstance(...)` directly.

- **Lifecycle**: `dispose()`, `[Symbol.dispose]()`, `on('restart', cb)`
- **Metadata**: `getInfo()`, `getLatency()`
- **Processing**: `setActive(bool)`, `setProcessing(bool)`, `process({ inputs, outputs, numSamples })`
- **Parameters**: `getParameterCount()`, `getParameterInfo(index)`, `getParameter(id)`, `setParameter(id, value)`, `setParameters(changes)`, `formatParameter(id, value)`
- **MIDI**: `addMidiEvent(event)`, `addMidiBytes(sampleOffset, bytes)`, `takeOutputEvents()`, `clearEvents()`
- **State**: `saveState()` → `Buffer`, `loadState(buffer)`

## MIDI Events

`addMidiEvent` accepts a discriminated union tagged by `type`. Use the `MidiEventType` enum to construct events:

```js
const { Host, MidiEventType } = require('nvst3-host');

const host = new Host({ sampleRate: 48000, maxBlockSize: 512 });
const synth = host.load('/path/to/Synth.vst3');

synth.setActive(true);
synth.setProcessing(true);

// Schedule a C4 note on at sample offset 0, note off at offset 256.
synth.addMidiEvent({
  type: MidiEventType.NoteOn,
  channel: 0,
  note: 60,        // C4
  velocity: 0.8,
  sampleOffset: 0,
});
synth.addMidiEvent({
  type: MidiEventType.NoteOff,
  channel: 0,
  note: 60,
  velocity: 0.0,
  sampleOffset: 256,
});

const outputs = [new Float32Array(512), new Float32Array(512)];
synth.process({ inputs: [], outputs, numSamples: 512 });

// Capture any output events (note-offs from the synth, SysEx, etc.).
const outEvents = synth.takeOutputEvents();
```

You can also push raw MIDI bytes with `addMidiBytes`:

```js
// Status 0x90 (note on, channel 0), note 60, velocity 100
synth.addMidiBytes(0, Uint8Array.from([0x90, 60, 100]));
```

## Error Handling

All errors thrown by `nvst3-host` carry a `code` property with one of the `VST3_*` error codes documented in [`docs/API.md`](docs/API.md#error-codes).

```js
const { Host } = require('nvst3-host');
const host = new Host();

try {
  const plugin = host.load('/nonexistent/path.vst3');
} catch (err) {
  if (err.code === 'VST3_LOAD_FAILED') {
    console.error('Plugin could not be loaded:', err.message);
  } else if (err.code === 'VST3_FACTORY_MISSING') {
    console.error('Module loaded but no VST3 factory was found.');
  } else {
    throw err;  // rethrow unknown errors
  }
}
```

After any `process()` failure, the plugin enters a **faulted** state and all subsequent method calls (other than `dispose()`) reject with `VST3_FAULTED` until you `dispose()` the instance and create a new one via `host.load()`.

## State Save / Load

```js
const fs = require('fs');
const { Host } = require('nvst3-host');

const host = new Host();
const plugin = host.load('/path/to/SomePlugin.vst3');

plugin.setActive(true);

// Mutate some parameters.
plugin.setParameter(0, 0.42);
plugin.setParameter(1, 0.97);

// Serialize current state to a Buffer.
const state = plugin.saveState();
fs.writeFileSync('plugin-state.bin', state);

// ... later, restore it ...
plugin.loadState(state);
console.log(plugin.getParameter(0));  // 0.42
```

State round-trips through `IComponent::getState` / `setState` and (if present) `IEditController::setComponentState`, so all parameter values are preserved.

## Examples

The `examples/` directory contains runnable sample scripts:

- [`examples/scan-plugins.js`](examples/scan-plugins.js) — list installed VST3 plugins with metadata.
- [`examples/process-file.js`](examples/process-file.js) — read a WAV, process it through a plugin, write a WAV.
- [`examples/midi-synth.js`](examples/midi-synth.js) — schedule MIDI notes through an instrument, write output WAV.
- [`examples/parameter-sweep.js`](examples/parameter-sweep.js) — automate a parameter across blocks, write output WAV.

## Building from Source

End users should never need this — prebuilt binaries cover all supported platforms. For contributors and unsupported platforms:

```bash
# 1. Clone with the VST3 SDK submodule.
git clone --recursive https://github.com/Henley04/nvst3-host.git
cd nvst3-host

# 2. If you cloned without --recursive:
git submodule update --init --recursive

# 3. Install dev dependencies.
npm ci

# 4. Build the native addon (uses node-gyp under the hood).
npm run build

# 5. Verify it loads.
node -e "console.log(require('./').version())"
# { native: '0.1.0', vst3sdk: 'VST 3.8.0', napi: 8 }
```

Prerequisites for source builds:

- **Node.js 20+** (for development; the runtime supports 16.17+)
- **Python 3** (required by `node-gyp`)
- **C++17 compiler**:
  - macOS: Xcode Command Line Tools (`xcode-select --install`)
  - Linux: `g++` ≥ 11 or `clang++` ≥ 13, plus `libasound2-dev` and `libstdc++-12-dev` (or equivalent)
  - Windows: Visual Studio 2022 with the "Desktop development with C++" workload
- **CMake ≥ 3.22** (only required to build the test plugin; see [CONTRIBUTING.md](CONTRIBUTING.md))

To create prebuilds for the current platform:

```bash
npm run prebuild
```

This invokes `prebuildify --napi-version 8 --tag-armv -t 20.0.0` and writes `.node` files into `prebuilds/<triple>/`. See [CONTRIBUTING.md](CONTRIBUTING.md) for the full CI-driven cross-platform build flow.

## Performance

`nvst3-host` is designed for real-time, block-based audio processing:

- **Zero-copy buffers** — `Float32Array` channel data is handed directly to the plugin via `AudioBusBuffers` channel pointers. There is no copy on input or output.
- **No allocations on the hot path** — `ProcessData`, `AudioBusBuffers`, `ParameterChangesContainer`, and `EventListContainer` are reused across `process()` calls; `setParameter` queues changes into pre-allocated queues. Steady-state `process()` does not allocate.
- **No locks during `IAudioProcessor::process`** — restart notifications are posted through a `Napi::ThreadSafeFunction` driven by an atomic flag, so the audio thread never blocks on JavaScript-side state.
- **Block-based processing** — `numSamples` per call is bounded by `maxBlockSize` (default 512). Process the file in chunks and you will get DAW-class throughput on commodity hardware.

### Limitations

- **No GUI/editor support** — `nvst3-host` is a headless host. Plugins that ship only an editor still process audio correctly; their `IPlugView` is never opened.
- **64-bit audio is opt-in** — `kSample32` is the default; 64-bit double precision (`kSample64`) is opt-in via `sampleSize: 64` in `HostOptions`/`LoadOptions`. The host silently falls back to 32 if the plugin refuses 64.
- **Single-process context** — each `PluginInstance` owns its own component/controller pair; there is no built-in signal graph or routing layer. Compose plugins in JavaScript by chaining `process()` calls.
- **Process mode is configurable** — `realtime` is the default; `offline` and `prefetch` modes are opt-in via `processMode` in `HostOptions`/`LoadOptions`.
- **macOS target** — binaries are built with `MACOSX_DEPLOYMENT_TARGET=10.13` (High Sierra and later).
- **Linux target** — prebuilt binaries require `glibc ≥ 2.28` (Ubuntu 18.04+ / Debian 10+).

## License

MIT — see [`LICENSE`](LICENSE).

The bundled VST3 SDK in `third_party/vst3sdk/` is also MIT-licensed (since SDK v3.7.7), so there are no licensing concerns for commercial or closed-source use. See `third_party/vst3sdk/LICENSE.txt` for the SDK's license.
