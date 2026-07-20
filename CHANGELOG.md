# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2026-07-20

### Audit-driven fixes — Headless-host hardening

This release closes out the host-side audit findings (10 critical + 7 partial +
8 not-implemented items). All changes are additive or correctness-only; no
breaking API changes. CI matrices and prebuilt-binary platforms have been
updated to reflect GitHub Actions runner availability.

### Added

- ESM dual-format entry — `index.mjs` re-exports the CJS surface so `import { Host } from 'nvst3-host'` works in ESM-only projects. The `package.json` `exports` field routes `.import`, `.require`, and `.types` correctly.
- Instance lifecycle tracking — `Host` constructor wrapper now tracks live `PluginInstance` objects and registers `process.on('beforeExit')` / `process.on('exit')` hooks that best-effort dispose any leaked instances before the native module unloads. Prevents use-after-free crashes when users forget to call `dispose()`.
- `setSystemTime(nanos)` — JS API to feed the host's monotonic system-time cache (nanoseconds since epoch). The real-time `process()` path now reads `systemTime` from a `std::atomic<int64_t>` instead of calling `std::chrono::system_clock::now()`, removing the only syscall on the audio thread.
- `getPluginInfo()` — comprehensive diagnostic snapshot combining static info, live state (active / processing / faulted / disposed), latency / tail / sample-size capability, all four bus categories with per-bus active state and speaker arrangement, and a boolean map of the 11 optional VST3 interfaces the plugin exposes (midiMapping, unitInfo, programListData, unitData, noteExpression, keyswitchController, processContextRequirements, audioPresentationLatency, prefetchableSupport, channelContextInfoListener, editController2).
- `getParameterTree()` — groups every parameter by `unitId` (root unit for orphans) and includes each parameter's current normalized value. Returns an array of unit nodes mirroring the `IUnitInfo` tree plus per-unit parameter lists.
- `IStreamAttributes` on `BufferStream` — `getFileName(String128)` and `getAttributes()` are now implemented (previously returned empty / nullptr). Host-side mutators `setFileName(utf8)`, `setStateType(utf8)`, and `setFilePath(utf8)` populate the stream's meta-info before handing it to the plugin for `setState`/`getState`. The lazy `getAttributes()` reuses the SDK `HostAttributeList` implementation.
- `linux-arm64` prebuilt binary via the new `ubuntu-24.04-arm` GitHub Actions runner. The prebuilt count is now 4 (was 3).
- `test/plugin/moduleinfo.json` — VST3 3.6.10+ module descriptor copied into the test plugin bundle on macOS and Linux by CMake.
- VST3 Validator step in CI (Linux x64 only). Runs `validator` if available on PATH, otherwise skips silently — keeps the matrix green on runners without the validator binary.
- Real-time safety smoke test (`test/rt-safety.test.js`). Three assertions cover the steady-state `process()` loop: (1) `heapUsed` does not grow beyond a small bound across 1000 blocks (catches runaway allocations); (2) the p99 per-block wall-clock time stays below a generous regression ceiling (catches accidental syscalls or heavy work on the audio thread); (3) output is bit-identical across many blocks of identical input (catches stateful drift). The test runs with `--expose-gc` so the heap-growth assertion can force GC snapshots; on runners without `--expose-gc` it falls back to a less-precise delta.

### Changed

- `version()` now reports `0.3.0`. `package.json` and `src/version.cc` kept in sync.
- `NstHostApplication` rewritten to directly implement `IHostApplication` instead of inheriting from `Steinberg::Vst::HostApplication`. The base class's `mPlugInterfaceSupport` was private and could not be reassigned, leaving the curated `NstPlugInterfaceSupport` list as dead code. The new implementation constructs `nstPlugInterfaceSupport_` in its own constructor, forwards `queryInterface` to it, and reuses the SDK's `HostMessage` / `HostAttributeList` for `createInstance`. `addRef` / `release` keep the singleton-style refcount (always returns 1) since lifetime is owned by the `Host` JS wrapper.
- `ComponentHandler::beginEdit` / `endEdit` now guard `activeGestures_` with `std::mutex` — previously the set was accessed from both the JS thread (user gesture begin/end) and the audio thread (restart propagation), with no synchronization.
- `binding.gyp` adds `SMTG_CPP_17=1` to the defines list, enabling the SDK's `std::u16string_view` variants in `vstbus.h` for C++17 hosts.
- `README` platform table now reflects runner reality: prebuilts ship for `win32-x64`, `darwin-arm64`, `linux-x64`, `linux-arm64`. `darwin-x64` (Intel Macs) and other x86 targets may be unavailable on GitHub Actions — source-build fallback is the supported path for those triples.
- `SUPPORTED_TRIPLES` in `index.js` retains all five entries for source-build discovery, but the loader's prebuilt path will only find binaries for the four ARM/x64 combinations actually shipped.

### Fixed

- `process()` no longer calls `std::chrono::system_clock::now()` on the audio thread. Callers that need `ProcessContext::systemTime` populated must either call `setSystemTime(Date.now() * 1e6)` periodically (e.g. from a `setInterval` on the JS thread) or set it once at activation; otherwise `systemTime` is `0` and the `kSystemTimeValid` bit stays clear.
- `NstPlugInterfaceSupport` is now actually installed on the host context — previously the curated FUID list was constructed but never reachable via `queryInterface`, so plugins probing for `IComponentHandler{,2,3}`, `IHostApplication`, `IPlugInterfaceSupport`, etc. would see `kNoInterface` and fall back to degraded behavior.

### CI

- New matrix entry `ubuntu-24.04-arm` (`linux-arm64` triple).
- `darwin-x64` runner is no longer available on GitHub Actions (Intel macOS runners were retired). Source builds remain supported via the `macos-latest` (arm64) cross-compilation target when an Intel Mac is unavailable.
- Release job's prebuild count assertion raised from 3 to 4.

### Out of scope (deferred)

- **UMP / MIDI 2.0** — VST3.7+ Event List UMP variants are not yet wired through the JS MIDI API. Full UMP support requires extending the `Event` struct, mapping UMP 1.x/2.x message bytes to VST3 `LegacyMIDICCOutEvent` / `kNoteExpressionValue` events, and adding a new `addUmpMessage(group, status, bytes)` JS surface. This is a major API addition, not a bug fix, and is intentionally deferred to a future minor-version spec.
- **Async / batch processing worker thread** — `process()` remains synchronous. VST3 plugins are NOT thread-safe by design: the SDK contract requires `IAudioProcessor::process` to be called from the host's single audio thread. Wrapping `process()` in `napi_async_work` would run it on the libuv worker pool, violating the SDK contract for any plugin that assumes single-threaded access (most do — they use thread-local state, non-atomic caches, etc.). The correct async pattern for VST3 hosts is to use a real-time audio thread on the C++ side with a lock-free ring buffer to the JS thread; that's a substantial architecture addition, deferred to a future spec.
- Standalone `.vst3` bundle publishing for the test plugin (only the build artifact is exercised by tests).
- DAW-style integration test suite (Validator smoke test is the current substitute).
- Code signing / notarization for prebuilt macOS binaries (unsigned for now; downstream users can re-sign).

[0.3.0]: https://github.com/Henley04/nvst3-host/releases/tag/v0.3.0

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
- `IAudioPresentationLatency` — `setAudioPresentationLatency(busIndex, latencySamples)`.
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
