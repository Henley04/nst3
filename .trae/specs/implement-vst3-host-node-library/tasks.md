# Tasks

Implementation is organized as a sequence of verifiable work items. Each item
maps to a requirement in `spec.md`. Dependencies are noted at the bottom.

## Foundation & Scaffolding

- [ ] Task 1: Bootstrap Rust + napi-rs workspace
  - [ ] SubTask 1.1: Create root `Cargo.toml` workspace + `package.json` + `pnpm`/`yarn` lockfile (use `@napi-rs/cli` v3+ scaffold via `napi new` non-interactive)
  - [ ] SubTask 1.2: Configure `Cargo.toml` with `napi`, `napi-derive`, `libloading`, `widestring`, `log`, `parking_lot` deps and `crate-type = ["cdylib"]`
  - [ ] SubTask 1.3: Create `build.rs` required by napi-rs and `.cargo/config.toml` for cross-compile settings
  - [ ] SubTask 1.4: Verify `npm run build && npm run test` runs the trivial smoke test on the host platform

- [ ] Task 2: Implement minimal VST3 COM FFI bindings in pure Rust
  - [ ] SubTask 2.1: Define `TUID` / `FID` (16-byte interface IDs), `FUnknown` vtable, `IUnknown`-style `queryInterface`/`addRef`/`release`
  - [ ] SubTask 2.2: Define vtables for: `IPluginFactory`, `IPluginFactory2`, `IPluginFactory3`, `IComponent`, `IAudioProcessor`, `IEditController`, `IEditController2`, `IConnectionPoint`, `IParamValueQueue`, `IParameterChanges`, `IEventList`, `IHostApplication`, `IComponentHandler`, `IComponentHandler2`, `IComponentHandler3`, `IMessage`, `IAttributeList`, `IBStream`, `IStreamAttributes`, `IPlugView`
  - [ ] SubTask 2.3: Define structs: `PFactoryInfo`, `PClassInfo`, `PClassInfo2`, `PClassInfoW`, `TUID`, `ProcessSetup`, `ProcessData`, `AudioBusBuffers`, `SpeakerArrangement`, `Event`, `NoteOnEvent`, `NoteOffEvent`, `PolyPressureEvent`, `NoteExpressionValueEvent`, `MidiEvent` (VST3), `ProgramChangeEvent`, `ControllerEvent`, `ParameterInfo`, `BusInfo`, `ParamID`, `ParamValue`, `Sample32`, `Sample64`
  - [ ] SubTask 2.4: Define constant TUIDs for all interfaces (from VST3 public spec) and `kDefaultFactory` entry point name (`GetPluginFactory`)
  - [ ] SubTask 2.5: Implement safe Rust wrappers (`VstPtr<T>` smart pointer with auto-release) over raw COM pointers

- [ ] Task 3: Cross-platform dynamic module loading
  - [ ] SubTask 3.1: Implement `Module::load(path)` using `libloading` (handle macOS `.vst3/Contents/MacOS/<name>` bundles, Linux `Contents/x86_64-linux/<name>.so`, Windows `<name>.vst3` files)
  - [ ] SubTask 3.2: Resolve `GetPluginFactory` export and call to retrieve `IPluginFactory*`
  - [ ] SubTask 3.3: Implement `Host::scan_directory`, `Host::scan_default_locations`, `Host::inspect_plugin` returning `Vec<PluginInfo>` (enumerate `IPluginFactory3::getClassInfos`)

## Core Hosting Pipeline

- [ ] Task 4: Plugin instantiation and initialization
  - [ ] SubTask 4.1: `Host::load(path, opts)`: create `IComponent` via factory's `createInstance`, query `IAudioProcessor`
  - [ ] SubTask 4.2: Initialize component with `IHostApplication` (host-side implementation) and configure host context via `IComponent::setHostApplication`
  - [ ] SubTask 4.3: Create `IEditController` (from component via `IConnectionPoint::queryInterface` or by re-instantiating via factory using the controller CID from `IComponent::getControllerClassId`)
  - [ ] SubTask 4.4: Connect component and controller via `IConnectionPoint` (both directions)
  - [ ] SubTask 4.5: Read `BusInfo` for audio in/out, MIDI in/out, default speaker arrangements
  - [ ] SubTask 4.6: Hold plugin state in `PluginInstance` Rust struct with `parking_lot::Mutex` for thread-safety against JS callbacks

- [ ] Task 5: Implement host-side VST3 interfaces
  - [ ] SubTask 5.1: `HostApplication` struct implementing `IHostApplication` (returns "Node.js VST3 Host", supports `IMessage`/`IAttributeList` creation)
  - [ ] SubTask 5.2: `ComponentHandler` implementing `IComponentHandler` + `IComponentHandler2` + `IComponentHandler3` (beginEdit/performEdit/endEdit, restartComponent, requestOpenEditor, createContextMenu, requestBusArrangement)
  - [ ] SubTask 5.3: `HostMessage` implementing `IMessage` + `IAttributeList` (for inter-component messages)
  - [ ] SubTask 5.4: `BStream` implementing `IBStream` + `IStreamAttributes` for state save/load (backed by `Vec<u8>` or `Cursor`)
  - [ ] SubTask 5.5: Implement `IConnectionPoint` host-side routing (between component and controller, capturing unsent messages)

- [ ] Task 6: Audio processing pipeline
  - [ ] SubTask 6.1: Implement `ProcessSetup` configuration from `HostOptions` (sample rate, max block size, sample format = Float32, processing mode = realtime)
  - [ ] SubTask 6.2: Implement `setActive(true/false)` calling `IAudioProcessor::setActive` with proper bus activation via `IComponent::activateBus`
  - [ ] SubTask 6.3: Implement `setProcessing(true/false)` calling `IAudioProcessor::setProcessing`
  - [ ] SubTask 6.4: Implement `process(ProcessBlock)`: build `ProcessData` with input/output `AudioBusBuffers` (zero-copy from JS `Float32Array` views via `napi-rs` `Float32Array`), call `IAudioProcessor::process`, validate return code
  - [ ] SubTask 6.5: Implement `getLatency()` reading `IAudioProcessor::getLatencySamples` after activation
  - [ ] SubTask 6.6: Negotiate speaker arrangements: query `getBusArrangement`, set `Stereo` (L R) by default, fall back to mono when plugin requires

- [ ] Task 7: Parameter management
  - [ ] SubTask 7.1: Implement `get_parameter_count`, `get_parameter_info` via `IEditController::getParameterCount`/`getParameterInfo`
  - [ ] SubTask 7.2: Implement `get_parameter(id)` via `IEditController::getParamNormalized`
  - [ ] SubTask 7.3: Implement `set_parameter(id, value)`: update controller + queue `IParamValueQueue` for next process call
  - [ ] SubTask 7.4: Implement `set_parameters(changes[])` batching
  - [ ] SubTask 7.5: Implement `format_parameter(id, value)` via `IEditController::getParamStringByValue`
  - [ ] SubTask 7.6: Implement internal `ParameterChanges` container with `IParamValueQueue` per parameter ID, reused across `process` calls (no per-call allocation)

- [ ] Task 8: MIDI and event handling
  - [ ] SubTask 8.1: Implement input `EventList` (Rust Vec-backed, implements `IEventList`) with capacity growth strategy (start 64, double when full)
  - [ ] SubTask 8.2: Implement `add_midi_event(MidiEvent)` mapping JS event type union to VST3 `Event` struct (noteOn/noteOff/polyPressure/cc/programChange/channelPressure/pitchBend/sysEx)
  - [ ] SubTask 8.3: Implement `add_midi_bytes(sample_offset, bytes)` raw parser using VST3 MIDI 1.0 mapping
  - [ ] SubTask 8.4: Implement output `EventList` capture: pass an output list to `process`, drain after each call, expose via `take_output_events()`
  - [ ] SubTask 8.5: Implement `clear_events()` to reset input list between blocks

- [ ] Task 9: State persistence
  - [ ] SubTask 9.1: Implement `save_state()` -> `Buffer`: create `BStream`, call `IComponent::getState`, return bytes
  - [ ] SubTask 9.2: Implement `load_state(Buffer)`: create `BStream`, call `IComponent::setState` then `IEditController::setComponentState`
  - [ ] SubTask 9.3: Verify round-trip preserves state across multiple save/load cycles

## napi-rs Bindings & API

- [ ] Task 10: napi-rs bindings for Host class
  - [ ] SubTask 10.1: `#[napi] pub struct Host { ... }` with constructor `new(opts: HostOptions) -> Result<Host>`
  - [ ] SubTask 10.2: `Host::scan_default_locations() -> Vec<PluginInfo>` (static method)
  - [ ] SubTask 10.3: `Host::scan_directory(path: String) -> Vec<PluginInfo>` (static method)
  - [ ] SubTask 10.4: `Host::inspect_plugin(path: String) -> PluginInfo | Vec<PluginInfo>` (static method)
  - [ ] SubTask 10.5: `Host::load(path: String, opts: LoadOptions) -> PluginInstance`
  - [ ] SubTask 10.6: Define `HostOptions`, `LoadOptions`, `PluginInfo` napi structs/enums

- [ ] Task 11: napi-rs bindings for PluginInstance class
  - [ ] SubTask 11.1: `#[napi] pub struct PluginInstance { ... }` exposing all methods from Tasks 6–9
  - [ ] SubTask 11.2: Implement `process(block: ProcessBlock)` accepting `Float32Array[]` inputs/outputs via napi-rs typed array zero-copy
  - [ ] SubTask 11.3: Implement `set_active`, `set_processing`, `get_latency`, `get_info`, `get_parameter_count`, `get_parameter_info`, `get_parameter`, `set_parameter`, `set_parameters`, `format_parameter`
  - [ ] SubTask 11.4: Implement `add_midi_event`, `add_midi_bytes`, `take_output_events`, `clear_events`
  - [ ] SubTask 11.5: Implement `save_state` -> `Buffer`, `load_state(Buffer)`
  - [ ] SubTask 11.6: Implement `dispose()` and a `Drop` impl that calls `dispose` if not already disposed; wire `Symbol.dispose` for `using` semantics
  - [ ] SubTask 11.7: Implement `on('restart', cb)` JS-event emitter pattern using `napi::threadsafe_function` for asynchronous restart notifications from `IComponentHandler::restartComponent`
  - [ ] SubTask 11.8: Generate and ship `index.d.ts` via `napi build --release --platform` (napi-rs auto-generates types)

- [ ] Task 12: Error handling and fault isolation
  - [ ] SubTask 12.1: Define `Nst3Error` enum implementing `Error`/`Debug` with variants: `LoadFailed`, `InvalidPath`, `FactoryMissing`, `ComponentCreationFailed`, `ControllerMissing`, `NotActive`, `NotProcessing`, `Faulted`, `PlatformUnsupported`, `InvalidParameter`, `InvalidBuffer`, `IoError`
  - [ ] SubTask 12.2: Convert `Nst3Error` to napi `Error` with proper `code` field set (e.g., `VST3_LOAD_FAILED`)
  - [SubTask] 12.3: Wrap plugin calls in `std::panic::catch_unwind` to survive segfaults in plugin code (best-effort)
  - [ ] SubTask 12.4: Mark `PluginInstance` as faulted after any processing error; reject all subsequent calls with `VST3_FAULTED`

## Distribution & CI

- [ ] Task 13: Cross-platform CI workflow with prebuilt binaries
  - [ ] SubTask 13.1: Write `.github/workflows/CI.yml` with matrix: `windows-latest` (x64), `macos-latest` (x64 + arm64), `ubuntu-latest` (x64 gnu) — use `@napi-rs/cli` `build --platform` and `artifacts`
  - [ ] SubTask 13.2: Add Linux `apt-get install -y libasound2-dev` (no other native deps needed since we use libloading, not the SDK)
  - [ ] SubTask 13.3: Configure `MACOSX_DEPLOYMENT_TARGET=10.13` for binary compatibility on older macOS
  - [ ] SubTask 13.4: Run `cargo test` + `npm test` on each platform before publishing
  - [ ] SubTask 13.5: On tag push: upload artifacts to GitHub Release via `napi artifacts`, then `napi publish` for each platform sub-package + main package

- [ ] Task 14: npm packaging configuration
  - [ ] SubTask 14.1: Configure `package.json` with `napi` field: `{ binaryName: "nst3", packageName: "nst3", targets: [...] }`
  - [ ] SubTask 14.2: Set `optionalDependencies` to `nst3-darwin-x64`, `nst3-darwin-arm64`, `nst3-win32-x64-msvc`, `nst3-linux-x64-gnu` (kept in sync by `napi version`)
  - [ ] SubTask 14.3: Set `engines: { node: ">=16.17" }`, `os`/`cpu` per-platform in sub-packages
  - [ ] SubTask 14.4: Generate `index.js` loader (auto by napi) that throws `VST3_PLATFORM_UNSUPPORTED` when no matching platform package is installed

## Tests, Examples & Docs

- [ ] Task 15: Mock VST3 plugin for testing (Rust)
  - [ ] SubTask 15.1: Create `crates/nst3-test-plugin` crate that compiles to a real `.vst3` module exporting `GetPluginFactory` — a simple gain plugin with 1 parameter (gain 0..1) and stereo in/out
  - [ ] SubTask 15.2: Implement `IComponent` + `IAudioProcessor` + `IEditController` for the test plugin so it can be loaded by `nst3` AND by real DAWs for verification
  - [ ] SubTask 15.3: Cross-compile the test plugin for all 4 target platforms in CI, store as Release asset for download in test step

- [ ] Task 16: JavaScript integration tests
  - [ ] SubTask 16.1: Test plugin discovery (point to a fixtures dir with the test `.vst3`)
  - [ ] SubTask 16.2: Test load + info (verify name, vendor, audio bus counts, parameter count)
  - [ ] SubTask 16.3: Test activate -> process (silence in, expect silence out for gain=0; expect input passthrough for gain=1)
  - [ ] SubTask 16.4: Test parameter set + read back; test `formatParameter` returns non-empty string
  - [ ] SubTask 16.5: Test MIDI note on/off produces non-silent output for the test plugin (if it's a synth) OR is accepted without error (for effect)
  - [ ] SubTask 16.6: Test state round-trip: save -> mutate -> load -> verify equal
  - [ ] SubTask 16.7: Test dispose frees resources (call dispose twice, second is no-op; verify no crash)
  - [ ] SubTask 16.8: Test error codes: load nonexistent, process when not active, etc.

- [ ] Task 17: Examples
  - [ ] SubTask 17.1: `examples/process-file.js`: read a WAV (using `wav-encoder` or raw PCM), process through a VST3, write output WAV
  - [ ] SubTask 17.2: `examples/scan-plugins.js`: list installed plugins with metadata
  - [ ] SubTask 17.3: `examples/midi-synth.js`: send MIDI notes to a synth plugin and capture output
  - [ ] SubTask 17.4: `examples/parameter-sweep.js`: automate a parameter across blocks

- [ ] Task 18: Documentation
  - [ ] SubTask 18.1: Replace top-level `README.md` with full feature overview, install instructions, quick start, supported platforms table, link to examples
  - [ ] SubTask 18.2: Add `docs/API.md` generated from `index.d.ts` (or rely on TypeScript IntelliSense; manual write-up of Host and PluginInstance methods)
  - [ ] SubTask 18.3: Add `CHANGELOG.md` with `## 0.1.0` initial release notes
  - [ ] SubTask 18.4: Add `CONTRIBUTING.md` with build-from-source instructions and CI flow

## Final Polish

- [ ] Task 19: Performance and real-time safety audit
  - [ ] SubTask 19.1: Audit `process()` path: confirm zero heap allocation after warmup (reuse buffers, pre-allocated event lists)
  - [ ] SubTask 19.2: Verify no locks held during `IAudioProcessor::process` call (only `parking_lot::Mutex` outside the audio path)
  - [ ] SubTask 19.3: Benchmark throughput on a 60s stereo file at 48kHz / 512 samples (target: <1x realtime on commodity hardware for the test gain plugin)

- [ ] Task 20: GitHub repository finalization
  - [ ] SubTask 20.1: Add `.gitignore` (target/, node_modules/, *.node, npm/ subdirs)
  - [ ] SubTask 20.2: Add `rust-toolchain.toml` pinning stable Rust (>=1.88)
  - [ ] SubTask 20.3: Add `LICENSE-MIT` reference in `package.json` and Cargo manifests
  - [ ] SubTask 20.4: Verify `git push origin main` succeeds and CI passes on all platforms
  - [ ] SubTask 20.5: Tag `v0.1.0` and confirm Release artifacts + npm packages publish successfully

# Task Dependencies

- Task 1 (scaffold) blocks all others.
- Task 2 (FFI bindings) blocks Tasks 3, 4, 5, 6, 7, 8, 9.
- Task 3 (module loading) blocks Task 4 (instantiation needs factory).
- Task 4 (instantiation) blocks Tasks 5, 6, 7, 8, 9 (need a live `PluginInstance`).
- Task 5 (host interfaces) is parallelizable with Task 6 (audio) once Task 4 lands — different files.
- Tasks 7, 8, 9 are parallelizable with each other after Task 5.
- Task 10 (Host napi) depends on Tasks 3.
- Task 11 (PluginInstance napi) depends on Tasks 6, 7, 8, 9.
- Task 12 (errors) is cross-cutting; land alongside Tasks 10–11.
- Task 13 (CI) depends on Task 1 and Task 14.
- Task 14 (npm packaging) depends on Task 1.
- Task 15 (mock plugin) is independent — can start in parallel with Task 2.
- Task 16 (JS tests) depends on Tasks 10, 11, 15.
- Task 17 (examples) depends on Task 11.
- Task 18 (docs) depends on Task 11.
- Task 19 (perf audit) depends on Tasks 6, 11.
- Task 20 (finalize) depends on all above.
