# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-07-19

### VST3 Spec Coverage — Complete

This release completes the project's VST3 host implementation to cover the
full set of host-side behaviors defined by the VST3 SDK specification. All
changes are additive; existing callers are unaffected unless explicitly noted
below.

### Added

- 64-bit audio processing (`kSample64`) — `sampleSize: 64` in `HostOptions`/`LoadOptions`; `getSampleSize()`, `canProcessSampleSize(size)`.
- Configurable process mode (`realtime` / `offline` / `prefetch`) — `processMode` in `HostOptions`/`LoadOptions`; `Event::kIsLive` cleared for non-realtime modes.
- Tail-samples query — `getTailSamples()` (returns `Number.POSITIVE_INFINITY` for `kInfiniteTail`).
- Parameter-flush blocks — `process({ numSamples: 0 })` flushes pending parameter changes without audio.
- Silence-flag propagation — `ProcessBlock.inputSilenceFlags` (per input bus) and `ProcessResult.outputSilenceFlags` (per output bus).
- Parameter string parsing — `parseParameter(id, str)`.
- Plain/normalized conversion — `plainToNormalized(id, plain)`, `normalizedToPlain(id, normalized)`.
- `Event::noteId` propagation — NoteOn / NoteOff / PolyPressure `MidiEvent` variants accept `noteId?: number`.
- `IUnitInfo` — `getUnitCount`, `getUnitInfo`, `getProgramListCount`, `getProgramListInfo`, `getProgramName`, `selectProgram`, `getCurrentUnit`, `getUnitByBusInfo`.
- `IProgramListData` / `IUnitData` — `getProgramData`, `setProgramData`, `getUnitData`, `setUnitData`.
- `INoteExpressionController` — `getNoteExpressionCount`, `getNoteExpressionInfo`, `addNoteExpressionEvent`.
- `IKeyswitchController` — `getKeyswitchCount`, `getKeyswitchInfo`.
- Runtime bus management — `getBusList`, `getBusInfo`, `activateBus`.
- Speaker-arrangement API — `setBusArrangement`, `getBusArrangement`, `SpeakerArrangement` enum.
- Routing info — `getRoutingInfo(srcBus, dstBus)`.
- Configurable `ProcessContext` — `setProcessContext(opts)`, `getProcessContext()`, `ProcessContextOptions` type.
- `IProcessContextRequirements` — `getProcessContextRequirements()`, `ProcessContextRequirementFlags` enum; the host gates recompute of unneeded `ProcessContext` fields each block.
- `IAudioPresentationLatencySamples` — `setAudioPresentationLatency(busIndex, latencySamples)`.
- `IInfoListener` — `setChannelContextInfo(info)`, `ChannelContextInfo` type, `ChannelContextInfoFlags` enum.
- `IPrefetchableSupport` — `isPrefetchable()`.
- `IEditController2` — `setKnobMode(mode)`, `KnobMode` enum.
- Restart auto-react — `applyRestartFlags(flags)`; `restartComponent` re-queries affected SDK state BEFORE emitting the JS event.
- Mutable `ProcessSetup` — `setProcessSetup({ sampleRate?, maxBlockSize?, processMode?, sampleSize? })`.
- Custom `IPlugInterfaceSupport` — host advertises exactly the 13 implemented interfaces (no GUI-only interfaces such as `IPlugView` / `IPlugFrame` / `IPlugViewContentScaleSupport`).
- Plugin→host events — `on('dirty')`, `on('beginGesture')`, `on('endGesture')`, `on('startGroup')`, `on('finishGroup')` (in addition to the existing `on('restart')`).
- New enums: `SampleSize`, `ProcessMode`, `BusDirection`, `KnobMode`, `NoteExpressionTypeIds`, `SpeakerArrangement`, `ProcessContextRequirementFlags`, `ChannelContextInfoFlags`.
- New types: `ProcessSetupOptions`, `ProcessResult`, `UnitInfo`, `ProgramListInfo`, `BusRef`, `NoteExpressionInfo`, `NoteExpressionEvent`, `KeyswitchInfo`, `BusInfo`, `RoutingInfo`, `ProcessContextOptions`, `ProcessContextSnapshot`, `ChannelContextInfo`.

### Modified

- `process()` return type widened from `void` to `ProcessResult | void` (existing callers ignoring the return value are unaffected).
- `process()` accepts `numSamples: 0` as a parameter-flush block (previously rejected).
- `saveState()` writes a versioned `NST3` envelope (4-byte magic, 1-byte version, length-prefixed component + controller blobs); `loadState()` auto-detects the envelope and falls back to legacy single-blob loading for backward compatibility with 0.1.0 state files.
- `ComponentHandler::restartComponent` now invokes `applyRestartFlags` before emitting the JS `restart` event.
- `ComponentHandler::setDirty` emits a `dirty` JS event (previously a no-op).
- `ComponentHandler::beginEdit` / `endEdit` track active gestures and emit `beginGesture` / `endGesture` JS events.
- `HostOptions` / `LoadOptions` accept `sampleSize?: 32 | 64` and `processMode?: 'realtime' | 'offline' | 'prefetch'`.
- Steady-state `process()` path uses `IProcessContextRequirements` to skip recomputation of unneeded `ProcessContext` fields; zero allocations maintained.

### Test fixtures

- `GainProcessor` test plugin extended: `canProcessSampleSize` returns `kResultTrue` for both 32 and 64; `IUnitInfo` (Root unit + Presets list with Init/Bright programs); `INoteExpressionController` (Volume expression); `kProgramId` parameter tagged `kIsProgramChange`.

### Documentation

- Comprehensive `docs/API.md` `## 0.2.0 — VST3 Spec Coverage` section covering all new methods, types, and enums.
- README "Features" section updated to reflect the now-complete VST3 host capabilities.

### Out of scope (deferred to a future GUI-support spec)

- `IPlugView` / `IPlugFrame` / `IPlugViewContentScaleSupport` (window-handle embedding).
- `IComponentHandler3::createContextMenu` (only meaningful with a visible editor).
- `IComponentHandler2::requestOpenEditor` / `requestZoomFactor` / `notifyZoom` (GUI lifecycle).
- `IStreamAttributes` extension on `BufferStream` (`.vstpreset` file loading).

### Known Limitations

- No GUI/editor support — `nvst3-host` is a headless host. Plugins that ship only an editor still process audio correctly; their `IPlugView` is never opened.
- No built-in signal graph or routing layer — each `PluginInstance` is a single plugin; chaining is the caller's responsibility.
- Prebuilt Linux binaries require `glibc ≥ 2.28` (Ubuntu 18.04+ / Debian 10+).
- macOS binaries target `MACOSX_DEPLOYMENT_TARGET=10.13` (High Sierra and later).
- No native async/batch API — all calls are synchronous from JavaScript's perspective (audio thread work happens inside `process()`).

[0.2.0]: https://github.com/Henley04/nvst3-host/releases/tag/v0.2.0

## [0.1.0] - 2026-07-18

### Added

- Initial release.
- VST3 plugin loading via the official Steinberg VST3 SDK v3.8.0 (MIT-licensed since v3.7.7).
- Cross-platform support: Windows x64, macOS x64, macOS arm64 (Apple Silicon), Linux x64.
- Prebuilt native binaries shipped via `prebuildify` — no compiler toolchain required for `npm install`.
- `Host` class with `load(path, opts)`, `getOptions()`, `scanDefaultLocations()`, `scanDirectory(path)`, `inspectPlugin(path)`.
- `PluginInstance` class covering:
  - **Lifecycle**: `dispose()` (idempotent), `[Symbol.dispose]()` for `using` syntax, `on('restart', cb)`.
  - **Metadata**: `getInfo()`, `getLatency()`.
  - **Processing**: `setActive(bool)`, `setProcessing(bool)`, `process({ inputs, outputs, numSamples })`.
  - **Parameters**: `getParameterCount()`, `getParameterInfo(index)`, `getParameter(id)`, `setParameter(id, value)`, `setParameters(changes[])`, `formatParameter(id, value)`.
  - **MIDI**: `addMidiEvent(event)`, `addMidiBytes(sampleOffset, bytes)`, `takeOutputEvents()`, `clearEvents()`.
  - **State**: `saveState()` → `Buffer`, `loadState(Buffer)`.
- Zero-copy audio processing — `Float32Array` channel buffers passed directly to the plugin via `AudioBusBuffers` channel pointers.
- Thread-safe restart notifications via `Napi::ThreadSafeFunction` (no locks held on the audio thread).
- Full MIDI event support: Note On/Off, Poly Pressure, Controller, Program Change, Channel Pressure, Pitch Bend, and SysEx (input and output).
- Plugin discovery across platform-default VST3 locations plus arbitrary directories.
- State persistence round-trip via `IComponent::getState`/`setState` and `IEditController::setComponentState`.
- Structured error codes: `VST3_LOAD_FAILED`, `VST3_FACTORY_MISSING`, `VST3_COMPONENT_CREATION_FAILED`, `VST3_CONTROLLER_MISSING`, `VST3_NOT_ACTIVE`, `VST3_NOT_PROCESSING`, `VST3_FAULTED`, `VST3_PLATFORM_UNSUPPORTED`, `VST3_INVALID_PARAMETER`, `VST3_INVALID_BUFFER`, `VST3_PROCESSING_ERROR`, `VST3_STATE_ERROR`, `VST3_MIDI_ERROR`, `VST3_UNKNOWN`.
- Faulted-state isolation — after a `process()` failure, subsequent calls reject with `VST3_FAULTED` until `dispose()` is called.
- Hand-written TypeScript definitions (`index.d.ts`) mirroring the native surface 1:1 for editor IntelliSense.
- Enums: `ParameterFlags`, `RestartFlags`, `BusType`, `MediaType`, `MidiEventType`, `PluginCategory`.
- `version()` returning `{ native, vst3sdk, napi }` for diagnostic introspection.
- `SUPPORTED_TRIPLES` constant listing the four supported platform triples.
- `NAPI_VERSION` constant exposing the Node-API version the binary was compiled against.
- Loader (`index.js`) with structured `VST3_PLATFORM_UNSUPPORTED` errors when no prebuilt matches and source-build fallback fails.
- C++17 source build via `node-gyp` with `binding.gyp` configuring VST3 SDK include paths and platform-specific defines.
- macOS binaries built with `MACOSX_DEPLOYMENT_TARGET=10.13` for backwards compatibility.
- Linux binaries linked against `libdl`/`libpthread` only (no plugin-runtime dependencies).

### Known Limitations

- No GUI/editor support — `nst3` is a headless host. Plugins that ship only an editor still process audio correctly; their `IPlugView` is never opened.
- 32-bit float audio only (`kSample32`); 64-bit double precision (`kSample64`) is not exposed.
- `processMode` is always `kRealtime`; no explicit offline rendering mode is requested from plugins.
- No built-in signal graph or routing layer — each `PluginInstance` is a single plugin; chaining is the caller's responsibility.
- Prebuilt Linux binaries require `glibc ≥ 2.28` (Ubuntu 18.04+ / Debian 10+).
- macOS binaries target `MACOSX_DEPLOYMENT_TARGET=10.13` (High Sierra and later).
- No native async/batch API — all calls are synchronous from JavaScript's perspective (audio thread work happens inside `process()`).

[0.1.0]: https://github.com/Henley04/nvst3-host/releases/tag/v0.1.0
