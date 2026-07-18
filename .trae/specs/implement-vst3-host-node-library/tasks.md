# Tasks

Implementation is organized as a sequence of verifiable work items. Each item
maps to a requirement in `spec.md`. Dependencies are noted at the bottom.

Tech stack: **C++17 + node-addon-api + official VST3 SDK (MIT) + prebuildify**.

## Foundation & Scaffolding

- [x] Task 1: Bootstrap C++ + node-addon-api project
  - [x] SubTask 1.1: Create `package.json` with `node-addon-api`, `node-gyp`, `node-gyp-build`, `prebuildify` deps; `engines.node >=16.17`; `scripts.build = "node-gyp-build"` / `scripts.install = "node-gyp-build"`
  - [x] SubTask 1.2: Add `third_party/vst3sdk` as git submodule pointing to `https://github.com/steinbergmedia/vst3sdk.git` (tag v3.7.9 or later MIT release)
  - [x] SubTask 1.3: Write `binding.gyp` configuring target `nst3.node`, C++17 standard, VST3 SDK include paths, common SDK sources (`base/source/*.cpp`, `base/thread/source/*.cpp`, `public.sdk/source/common/*.cpp`, `public.sdk/source/vst/hosting/*.cpp`, `public.sdk/source/vst/utility/*.cpp`), platform-specific defines (`DEVELOPMENT_ENVIRONMENT=1`, `_GNU_SOURCE` on Linux)
  - [x] SubTask 1.4: Write minimal `src/addon.cc` exporting a `version()` function returning the SDK version string; verify `npm run build` and `node -e "console.log(require('./').version())"`

- [x] Task 2: Cross-platform module loading & discovery
  - [x] SubTask 2.1: Use VST3 SDK's `VST3::Hosting::Module::create(path, errorCallback)` to load `.vst3` modules on each platform (handles bundle layout internally)
  - [x] SubTask 2.2: Implement `Host::scanDirectory(path)` walking the directory recursively for `.vst3` entries; on each, load factory via `Module::getFactory()` and enumerate `IPluginFactory3::getClassInfos` (fall back to v2/v1)
  - [x] SubTask 2.3: Implement `Host::scanDefaultLocations()` returning the platform-specific standard paths (see spec.md)
  - [x] SubTask 2.4: Implement `Host::inspectPlugin(path)` returning `PluginInfo` (single) or array (multi-class factory) without instantiating components
  - [x] SubTask 2.5: Define `PluginInfo` struct mapping `PClassInfoW`/`PClassInfo2` fields to JS: `{ path, name, vendor, version, category, subCategories, sdkVersion, factoryInfo, classId, cardinality }`

## Core Hosting Pipeline

- [x] Task 3: Plugin instantiation & host context
  - [x] SubTask 3.1: Subclass `VST3::Hosting::HostApplication` as `NstHostApplication` (override `getName` to return `"Node.js VST3 Host"`); use as the host context object
  - [x] SubTask 3.2: Implement `Host::load(path, opts)`: get factory, find the first `kVstAudioEffectClass` component CID, create `IComponent` via `factory->createInstance(cid, IComponent::iid)`, query `IAudioProcessor`
  - [x] SubTask 3.3: Call `IComponent::initialize(hostApplication)` with our `NstHostApplication`
  - [x] SubTask 3.4: Create `IEditController`: read controller CID via `IComponent::getControllerClassId`, then `factory->createInstance(controllerCid, IEditController::iid)`; alternatively `queryInterface(IEditController)` on the component
  - [x] SubTask 3.5: Initialize controller with `IEditController::initialize(hostApplication)`; connect component↔controller via `IConnectionPoint` (queryInterface both sides, register peer)
  - [x] SubTask 3.6: Read audio bus info via `IComponent::getBusInfo(kAudio, kInput, ...)`, `getBusInfo(kAudio, kOutput, ...)`; activate default stereo buses via `IComponent::activateBus(kAudio, kInput, 0, true)`
  - [x] SubTask 3.7: Wrap the live component+controller+module in a `PluginInstance` C++ class owned by a node-addon-api `Napi::ObjectWrap<PluginInstance>`

- [x] Task 4: Host-side handler implementation
  - [x] SubTask 4.1: Subclass `IComponentHandler`/`IComponentHandler2`/`IComponentHandler3` as `NstComponentHandler` (held by `PluginInstance`); implement `beginEdit`/`performEdit`/`endEdit` (track active gesture, queue param change), `restartComponent` (emit JS `restart` event via `Napi::ThreadSafeFunction`), `requestOpenEditor` (return `kResultFalse`), `createContextMenu` (return `kNotImplemented`)
  - [x] SubTask 4.2: Register the handler with the controller via `IEditController::setComponentHandler(handler)`
  - [x] SubTask 4.3: Implement `HostApplication::createMessage` returning `VST3::Hosting::HostMessage` (SDK helper) for inter-component messages

- [x] Task 5: Audio processing pipeline
  - [x] SubTask 5.1: Define `ProcessSetup` from `HostOptions` (`sampleRate`, `maxBlockSize`, `kSample32`, `kRealtime`); call `IAudioProcessor::setupProcessing(setup)` before `setActive`
  - [x] SubTask 5.2: Implement `setActive(bool)`: call `IComponent::setActive(true)` then `IAudioProcessor::setActive(true)` (and reverse on deactivate); activate buses as planned in Task 3.6
  - [x] SubTask 5.3: Implement `setProcessing(bool)` calling `IAudioProcessor::setProcessing(bool)`
  - [x] SubTask 5.4: Implement `process(ProcessBlock)`: build `ProcessData` (reuse a member `ProcessData` to avoid per-call alloc), set `numSamples`, `inputs`/`outputs` `AudioBusBuffers` with channel pointers taken directly from JS `Float32Array` `Data()` pointers (zero-copy), `processMode = kRealtime`, `symbolicSampleSize = kSample32`
  - [x] SubTask 5.5: Attach input `IParameterChanges` and `IEventList` (SDK `ParameterChangesContainer` / `EventListContainer`) populated by Tasks 6 & 7; attach output containers to capture plugin output
  - [x] SubTask 5.6: Call `IAudioProcessor::process(data)`, check return code, on failure set faulted state and throw JS error with `code: 'VST3_PROCESSING_ERROR'`
  - [x] SubTask 5.7: Implement `getLatency()` calling `IAudioProcessor::getLatencySamples`
  - [x] SubTask 5.8: Implement speaker arrangement negotiation: query `getBusArrangement(kInput, 0, ...)`, attempt `setBusArrangement(kStereo, kStereo)` for stereo plugins; fall back to `kMono` if the plugin refuses stereo

- [x] Task 6: Parameter management
  - [x] SubTask 6.1: Implement `getParameterCount()` via `IEditController::getParameterCount`
  - [x] SubTask 6.2: Implement `getParameterInfo(index)` returning JS object with all `ParameterInfo` fields (convert `String128` to UTF-8 via `Steinberg::String` or VST3 `UString` helpers)
  - [x] SubTask 6.3: Implement `getParameter(id)` via `IEditController::getParamNormalized`
  - [x] SubTask 6.4: Implement `setParameter(id, value)`: `IEditController::setParamNormalized`, then add point to `ParameterChangesContainer` for next process; if a beginEdit gesture is in flight, also call `IComponentHandler::performEdit` (route to our own handler)
  - [x] SubTask 6.5: Implement `setParameters(changes[])` batching — apply all in a loop, all changes land in the same parameter container
  - [x] SubTask 6.6: Implement `formatParameter(id, value)` calling `IEditController::getParamStringByValue` and returning the resulting `String128` as UTF-8
  - [x] SubTask 6.7: Reset `ParameterChangesContainer` between `process` calls (`removeQueue`/reuse); ensure zero allocations on the steady-state path

- [x] Task 7: MIDI & event handling
  - [x] SubTask 7.1: Reuse SDK `EventListContainer` for input events; clear before each `process`
  - [x] SubTask 7.2: Implement `addMidiEvent(MidiEvent)`: convert JS event union to VST3 `Event` (noteOn/noteOff/polyPressure/cc/programChange/channelPressure/pitchBend/sysEx); `event.sampleOffset` set from JS field
  - [x] SubTask 7.3: Implement `addMidiBytes(sampleOffset, Uint8Array)`: parse status byte high nibble, build the appropriate `Event` per VST3 MIDI 1.0 mapping (use SDK `FromMidiMessage` if available, otherwise hand-implement the small mapping table)
  - [x] SubTask 7.4: Implement output capture: SDK `EventListContainer` for output; after `process`, iterate events, convert back to JS `MidiEvent`, return from `takeOutputEvents()` and clear
  - [x] SubTask 7.5: Implement `clearEvents()` calling `EventListContainer::clear`

- [x] Task 8: State persistence
  - [x] SubTask 8.1: Implement `saveState()`: use SDK `std::vector<uint8>` + `IBStream` adapter (or write a tiny `BufferStream` subclass of `IBStream`); call `IComponent::getState(stream)`, return `Napi::Buffer<uint8>::New(env, data, length)`
  - [x] SubTask 8.2: Implement `loadState(Buffer)`: wrap the buffer in `BufferStream`, call `IComponent::setState(stream)` then (if controller) `IEditController::setComponentState(stream)`
  - [x] SubTask 8.3: Verify round-trip preserves parameter values: save → mutate param → load → `getParameter(id)` returns original

## napi Bindings & API

- [x] Task 9: node-addon-api bindings for Host class
  - [x] SubTask 9.1: `Napi::ObjectWrap<Host>` with constructor accepting `HostOptions` (`sampleRate`, `maxBlockSize`, `audioInputs`, `audioOutputs`); validate and store in `HostContext`
  - [x] SubTask 9.2: Static methods `scanDefaultLocations`, `scanDirectory`, `inspectPlugin` returning JS arrays/objects (use `Napi::Object::New` + `Set` for each `PluginInfo`)
  - [x] SubTask 9.3: Instance method `load(path, opts)` returning a `PluginInstance` JS object (instantiation via `Napi::ObjectWrap<PluginInstance>::Construct`)
  - [x] SubTask 9.4: Implement `Init(env, exports)` registering the `Host` class on the module exports

- [x] Task 10: node-addon-api bindings for PluginInstance class
  - [x] SubTask 10.1: `Napi::ObjectWrap<PluginInstance>` holding the C++ `PluginInstance` (which owns `IPtr<IComponent>`, `IPtr<IAudioProcessor>`, `IPtr<IEditController>`, `NstComponentHandler`, `VST3::Hosting::Module`)
  - [x] SubTask 10.2: Implement `process(ProcessBlock)`: validate `inputs`/`outputs` are `Float32Array` arrays with matching `numSamples` length; pass `Float32Array::Data()` directly into `AudioBusBuffers`
  - [x] SubTask 10.3: Implement `setActive`, `setProcessing`, `getLatency`, `getInfo`, `getParameterCount`, `getParameterInfo`, `getParameter`, `setParameter`, `setParameters`, `formatParameter`
  - [x] SubTask 10.4: Implement `addMidiEvent`, `addMidiBytes`, `takeOutputEvents`, `clearEvents`
  - [x] SubTask 10.5: Implement `saveState` -> `Buffer`, `loadState(Buffer)`
  - [x] SubTask 10.6: Implement `dispose()` calling the C++ destructor logic; idempotent (guard with a `disposed_` flag)
  - [x] SubTask 10.7: Implement `Napi::ObjectWrap<PluginInstance>::Finalize` calling `dispose()` if not already called
  - [x] SubTask 10.8: Implement `on('restart', cb)` via `Napi::ThreadSafeFunction` stored in the instance; `NstComponentHandler::restartComponent` queues a JS callback with the restart flags

- [x] Task 11: Module entry & index.js loader
  - [x] SubTask 11.1: Write `src/addon.cc` `Init`/`InitModule` exporting `Host`, `PluginInstance`, `version()`, and enum constants (`ParameterFlags`, `RestartFlags`, `BusType`, `MediaType`, `PluginCategory`)
  - [x] SubTask 11.2: Write `index.js` using `node-gyp-build` to load `nst3.node` and re-exporting; wrap load failure in `Error` with `code: 'VST3_PLATFORM_UNSUPPORTED'` listing supported triples when no prebuild matches and source build fails
  - [x] SubTask 11.3: Hand-write `index.d.ts` covering all classes, methods, structs, and enums; ship alongside `index.js`

- [x] Task 12: Error handling & fault isolation
  - [x] SubTask 12.1: Define a `NstError` enum/exception with codes: `VST3_LOAD_FAILED`, `VST3_FACTORY_MISSING`, `VST3_COMPONENT_CREATION_FAILED`, `VST3_CONTROLLER_MISSING`, `VST3_NOT_ACTIVE`, `VST3_NOT_PROCESSING`, `VST3_FAULTED`, `VST3_PLATFORM_UNSUPPORTED`, `VST3_INVALID_PARAMETER`, `VST3_INVALID_BUFFER`, `VST3_PROCESSING_ERROR`
  - [x] SubTask 12.2: Wrap every plugin call in `try`/`catch` C++ (since SDK may throw) and translate to `Napi::Error::New(env, message).Set("code", code)` then throw
  - [x] SubTask 12.3: After any process fault, set `faulted_ = true` and reject all subsequent calls with `VST3_FAULTED`
  - [x] SubTask 12.4: Validate all JS arguments (types, ranges) before passing to SDK

## Distribution & CI

- [x] Task 13: Cross-platform CI with prebuildify
  - [x] SubTask 13.1: Write `.github/workflows/CI.yml` matrix: `windows-latest` (x64), `macos-13` (x64), `macos-14` (arm64), `ubuntu-latest` (x64)
  - [x] SubTask 13.2: Each job: checkout submodules (`submodules: recursive`), install Python 3 + Node 20, install deps (`npm ci`), run `npm test`, then `npx prebuildify --napi-version 8 --tag-armv -t 20.0.0`
  - [x] SubTask 13.3: Upload `prebuilds/` artifact from each job (`actions/upload-artifact@v4`)
  - [x] SubTask 13.4: Final job `release`: download all 4 artifacts into one `prebuilds/` dir, optionally `prebuildify --pack` to produce a single tarball, `npm publish` on tag push (use `NPM_TOKEN` secret)
  - [x] SubTask 13.5: Set `MACOSX_DEPLOYMENT_TARGET=10.13` (via `CFLAGS`/`CXXFLAGS`/`LDFLAGS`) for backwards-compatible macOS binaries
  - [x] SubTask 13.6: On Linux install `libasound2-dev` and `libstdc++-12-dev` (or similar) for SDK ALSA-less build; Linux binary uses dlopen so no plugin-runtime deps required

- [x] Task 14: npm packaging configuration
  - [x] SubTask 14.1: Final `package.json` with `main: "index.js"`, `types: "index.d.ts"`, `files: ["index.js", "index.d.ts", "prebuilds/", "binding.gyp", "src/", "third_party/"]`, `binary.module_name: "nst3"`, `binary.module_path: "./"`
  - [x] SubTask 14.2: `scripts.install = "node-gyp-build"` (uses prebuilds; falls back to `node-gyp rebuild` if absent)
  - [x] SubTask 14.3: `scripts.build = "node-gyp configure && node-gyp build"` (dev only)
  - [x] SubTask 14.4: `scripts.prebuild = "prebuildify --napi-version 8 --tag-armv -t 20.0.0"`
  - [x] SubTask 14.5: Add `bundledDependencies: ["node-gyp-build"]` (or include as `dependency`)

## Tests, Mock Plugin & Examples

- [x] Task 15: Mock VST3 plugin in C++ (test fixture)
  - [x] SubTask 15.1: Create `test/plugin/` with `GainProcessor` — a simple stereo gain VST3 plugin built using `public.sdk/source/vst/vstsinglecomponenteffect.h` (SingleComponentEffect base); 1 parameter (Gain, 0..1, default 1.0)
  - [x] SubTask 15.2: Plugin reads parameter, multiplies input by gain in `process`; supports state save/load (just the gain float)
  - [x] SubTask 15.3: Add `test/plugin/binding.gyp` (or CMakeLists) producing `.vst3` bundle in `test/plugin/build/Gain.vst3` for the host platform
  - [ ] SubTask 15.4: CI matrix builds the test plugin for each target platform and uploads as artifact; tests download the matching platform plugin before running
  - [ ] SubTask 15.5: Optionally also implement `TestSynth` — a MIDI-driven oscillator (1 sine voice) for MIDI event tests

- [x] Task 16: JavaScript integration tests
  - [x] SubTask 16.1: `test/discovery.test.js`: scan a fixtures dir with the test `.vst3`, verify `PluginInfo` fields
  - [x] SubTask 16.2: `test/load.test.js`: load + verify `getInfo()` (name, vendor, version, audio bus counts, parameter count)
  - [x] SubTask 16.3: `test/process.test.js`: activate → process silence (gain=0 → silence out; gain=1 → passthrough); verify Float32Array is filled and buffer is zero-copy (compare `Buffer.from(array.buffer).address` to native report if feasible)
  - [x] SubTask 16.4: `test/parameters.test.js`: `getParameterInfo(0)` returns expected fields; `setParameter`/`getParameter` round-trip; `formatParameter(0, 0.5)` returns non-empty
  - [x] SubTask 16.5: `test/midi.test.js`: if `TestSynth` is built, send noteOn/noteOff → output is non-silent; otherwise use the gain plugin and just verify `addMidiEvent` is accepted without error
  - [x] SubTask 16.6: `test/state.test.js`: save → mutate → load → verify params restored; also test loading the saved buffer into a fresh `PluginInstance`
  - [x] SubTask 16.7: `test/lifecycle.test.js`: dispose twice (no throw); load + dispose + load again (no leak/crash); verify GC finalizer does not crash
  - [x] SubTask 16.8: `test/errors.test.js`: nonexistent path → `VST3_LOAD_FAILED`; process before setActive → `VST3_NOT_ACTIVE`; process before setProcessing → `VST3_NOT_PROCESSING`; bad Float32Array length → `VST3_INVALID_BUFFER`

- [x] Task 17: Examples
  - [x] SubTask 17.1: `examples/scan-plugins.js`: list installed plugins with metadata
  - [x] SubTask 17.2: `examples/process-file.js`: read a WAV (`wav-decoder` or raw PCM) → process → write WAV (`wav-encoder`)
  - [x] SubTask 17.3: `examples/midi-synth.js`: schedule MIDI notes → process blocks → write output WAV
  - [x] SubTask 17.4: `examples/parameter-sweep.js`: automate a parameter across blocks → write output WAV

- [x] Task 18: Documentation
  - [x] SubTask 18.1: Replace top-level `README.md` with full feature overview, install instructions, quick start, supported platforms table, link to examples
  - [x] SubTask 18.2: Add `docs/API.md` derived from `index.d.ts` (or rely on TypeScript IntelliSense; document each method's purpose and signature)
  - [x] SubTask 18.3: Add `CHANGELOG.md` with `## 0.1.0` initial release notes
  - [x] SubTask 18.4: Add `CONTRIBUTING.md` with build-from-source instructions and CI flow

## Final Polish

- [x] Task 19: Performance and real-time safety audit
  - [x] SubTask 19.1: Audit `process()` path: no heap allocations after warmup (reuse `ProcessData`, `AudioBusBuffers`, `ParameterChangesContainer`, `EventListContainer`)
  - [x] SubTask 19.2: Verify no locks held during `IAudioProcessor::process` (use `std::atomic` flag for restart notifications instead of mutex)
  - [x] SubTask 19.3: Benchmark throughput on a 60s stereo file at 48kHz / 512 samples (target: <1x realtime on commodity hardware for the gain plugin)

- [ ] Task 20: GitHub repository finalization
  - [x] SubTask 20.1: Add `.gitignore` (build/, node_modules/, *.node, prebuilds/*.node, test/plugin/build/)
  - [x] SubTask 20.2: Add `binding.gyp` includes for SDK submodule sources; verify `git submodule update --init --recursive && npm run build` works on a clean clone
  - [x] SubTask 20.3: Confirm MIT license fields in `package.json` and `LICENSE` covers both our code and the VST3 SDK submodule
  - [ ] SubTask 20.4: Verify `git push origin main` succeeds and CI passes on all 4 platforms — **BLOCKED: sandbox lacks GitHub credentials; commit `3b9304a` ready on branch `trae/agent-AiPjCN`**
  - [ ] SubTask 20.5: Tag `v0.1.0` and confirm Release artifacts + npm package publish successfully — **BLOCKED: requires 20.4**
  - [ ] SubTask 20.6: Verify `npm install nst3` on a clean machine (no toolchain) loads the binary without compilation — **BLOCKED: requires 20.5**

# Task Dependencies

- Task 1 (scaffold) blocks all others.
- Task 2 (loading/discovery) depends on Task 1.
- Task 3 (instantiation/host context) depends on Task 2.
- Task 4 (component handler) depends on Task 3.
- Task 5 (audio processing) depends on Tasks 3, 4.
- Task 6 (parameters) depends on Task 4 (uses component handler).
- Task 7 (MIDI/events) depends on Task 3 (needs live instance); parallelizable with Task 6.
- Task 8 (state) depends on Task 3; parallelizable with Tasks 6, 7.
- Task 9 (Host napi) depends on Task 2.
- Task 10 (PluginInstance napi) depends on Tasks 5, 6, 7, 8.
- Task 11 (entry/loader) depends on Tasks 9, 10.
- Task 12 (errors) cross-cutting; land alongside Tasks 9–11.
- Task 13 (CI) depends on Task 14.
- Task 14 (npm packaging) depends on Task 1.
- Task 15 (mock plugin) is independent — can start in parallel with Task 2.
- Task 16 (JS tests) depends on Tasks 11, 15.
- Task 17 (examples) depends on Task 11.
- Task 18 (docs) depends on Task 11.
- Task 19 (perf audit) depends on Tasks 5, 10.
- Task 20 (finalize) depends on all above.
