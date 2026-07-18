# Checklist

Each checkpoint maps to a requirement or cross-cutting concern from `spec.md`.
Verify the relevant code/system behavior and check the box when satisfied.

Tech stack: **C++17 + node-addon-api + official VST3 SDK (MIT) + prebuildify**.

## Foundation & Scaffolding
- [ ] `package.json` exists with `node-addon-api`, `node-gyp`, `node-gyp-build`, `prebuildify` declared
- [ ] `engines.node` set to `>=16.17`
- [ ] `third_party/vst3sdk/` is a git submodule pointing to an MIT-licensed Steinberg VST3 SDK release (v3.7.9+)
- [ ] `binding.gyp` configures target `nst3.node`, C++17, includes VST3 SDK headers, compiles required SDK sources (`base/source/*`, `base/thread/source/*`, `public.sdk/source/common/*`, `public.sdk/source/vst/hosting/*`)
- [ ] `npm run build` produces `build/Release/nst3.node` on the host platform
- [ ] `node -e "console.log(require('./').version())"` returns the SDK version string

## Module Loading & Discovery
- [ ] `VST3::Hosting::Module::create(path, errCb)` is used for cross-platform loading
- [ ] macOS `.vst3` bundle layout is handled by the SDK helper (no manual path munging)
- [ ] Linux `.vst3` bundle layout is handled by the SDK helper
- [ ] Windows `.vst3` file is loaded directly
- [ ] `Host.scanDefaultLocations()` returns the platform-specific standard paths
- [ ] `Host.scanDirectory(path)` recursively finds `.vst3` modules
- [ ] `Host.inspectPlugin(path)` returns metadata without instantiating components
- [ ] `PluginInfo` JS object contains `{ path, name, vendor, version, category, subCategories, sdkVersion, factoryInfo, classId }`

## Plugin Instantiation
- [ ] `host.load(path, opts)` returns a `PluginInstance` for a valid plugin
- [ ] `IComponent` is created via `factory->createInstance(cid, IComponent::iid)`
- [ ] `IAudioProcessor` is queried from the component
- [ ] `IComponent::initialize(hostApplication)` is called with our `NstHostApplication`
- [ ] `IEditController` is created and `initialize`d
- [ ] Component and controller are connected via `IConnectionPoint` (both directions)
- [ ] `IEditController::setComponentHandler` is called with our `NstComponentHandler`
- [ ] `BusInfo` is read and exposed via `getInfo()`
- [ ] Default stereo buses are activated via `IComponent::activateBus`

## Host Interfaces
- [ ] `NstHostApplication::getName` returns `"Node.js VST3 Host"`
- [ ] `NstHostApplication::createMessage` returns a working `IMessage`
- [ ] `NstComponentHandler::beginEdit/performEdit/endEdit` cycle works (no crash, queue updated)
- [ ] `NstComponentHandler::restartComponent` emits a JS `restart` event via `Napi::ThreadSafeFunction` with restart flags
- [ ] Connection-point messages route between component and controller

## Audio Processing
- [ ] `setupProcessing` is called with the configured `ProcessSetup` before `setActive`
- [ ] `setActive(true)` calls both `IComponent::setActive(true)` and `IAudioProcessor::setActive(true)`
- [ ] `setProcessing(true)` calls `IAudioProcessor::setProcessing(true)`
- [ ] `process({inputs, outputs, numSamples})` calls `IAudioProcessor::process` and fills `outputs`
- [ ] Audio buffers are zero-copy (`Float32Array::Data()` passed directly to `AudioBusBuffers`)
- [ ] `getLatency()` returns the value reported by `IAudioProcessor::getLatencySamples`
- [ ] Speaker arrangement defaults to Stereo when supported by the plugin
- [ ] `setProcessing(false)` and `setActive(false)` clean up properly

## Parameter Management
- [ ] `getParameterCount()` matches `IEditController::getParameterCount`
- [ ] `getParameterInfo(i)` returns all fields (title, shortTitle, units, stepCount, defaultNormalizedValue, unitId, flags) with `String128` converted to UTF-8
- [ ] `getParameter(id)` returns the current normalized value
- [ ] `setParameter(id, value)` updates controller and queues change for next process
- [ ] `setParameters([...])` applies multiple atomically
- [ ] `formatParameter(id, value)` returns the plugin's display string
- [ ] `ParameterChangesContainer` is reused across `process` calls (no per-call allocation)

## MIDI & Events
- [ ] Input `EventListContainer` is reused and cleared before each `process`
- [ ] `addMidiEvent({type, channel, ...})` adds a VST3 `Event` to the input list
- [ ] `addMidiBytes(sampleOffset, [byte1, byte2, byte3])` parses raw MIDI correctly
- [ ] All supported MIDI event types are mapped (noteOn, noteOff, polyPressure, cc, programChange, channelPressure, pitchBend, sysEx)
- [ ] `takeOutputEvents()` returns events emitted by the plugin and clears the output queue
- [ ] `clearEvents()` resets the input list

## State Persistence
- [ ] `saveState()` returns a `Buffer` with the plugin's serialized state
- [ ] `loadState(buffer)` restores the plugin state (component + controller via `setComponentState`)
- [ ] Round-trip preserves parameter values: save → mutate → load → values match

## Error Handling
- [ ] Loading a nonexistent path throws with `code: 'VST3_LOAD_FAILED'`
- [ ] Calling `process` before `setActive(true)` throws with `code: 'VST3_NOT_ACTIVE'`
- [ ] Calling `process` before `setProcessing(true)` throws with `code: 'VST3_NOT_PROCESSING'`
- [ ] Plugin fault during process marks the instance faulted and subsequent calls throw `VST3_FAULTED`
- [ ] All plugin calls wrapped in C++ try/catch and translated to `Napi::Error` with `code` field
- [ ] `dispose()` is idempotent (safe to call twice)
- [ ] `PluginInstance::Finalize` calls `dispose` if not already called
- [ ] JS arguments are validated (types, ranges) before passing to SDK

## napi Bindings
- [ ] `Host` class is exported with constructor and all discovery methods (`scanDefaultLocations`, `scanDirectory`, `inspectPlugin`, `load`)
- [ ] `PluginInstance` class is exported with all processing/parameter/MIDI/state methods
- [ ] `index.d.ts` is hand-written and shipped in the package, covering all public types
- [ ] All public types are exported (`HostOptions`, `LoadOptions`, `PluginInfo`, `ParameterInfo`, `MidiEvent`, `ProcessBlock`, enums)
- [ ] `on('restart', cb)` works via `Napi::ThreadSafeFunction`
- [ ] `version()` static function returns the VST3 SDK version string

## Distribution & CI
- [ ] `.github/workflows/CI.yml` builds for windows-x64, darwin-x64, darwin-arm64, linux-x64
- [ ] Each CI job runs `npm ci`, `npm test`, then `prebuildify`
- [ ] `MACOSX_DEPLOYMENT_TARGET=10.13` is set for macOS builds
- [ ] Linux build installs required dev packages (libasound2-dev, libstdc++-12-dev)
- [ ] `prebuilds/` artifacts are uploaded from each job and collected in a final release job
- [ ] On tag push: a single npm package is published containing all 4 prebuilds
- [ ] `package.json` `files` field includes `index.js`, `index.d.ts`, `prebuilds/`, `binding.gyp`, `src/`, `third_party/`
- [ ] `scripts.install` is `node-gyp-build` (uses prebuilds; falls back to source build)

## Tests & Mock Plugin
- [x] `test/plugin/` contains a `GainProcessor` C++ VST3 plugin using `SingleComponentEffect`
- [x] Test plugin has 1 parameter (Gain 0..1) and stereo in/out
- [ ] Test plugin is built by CI for all 4 target platforms and uploaded as artifact
- [ ] `test/discovery.test.js` passes (PluginInfo fields verified)
- [ ] `test/load.test.js` passes (info matches expected)
- [ ] `test/process.test.js` passes (gain=0 → silence; gain=1 → passthrough; zero-copy verified)
- [ ] `test/parameters.test.js` passes (info, get/set round-trip, formatParameter non-empty)
- [ ] `test/midi.test.js` passes (events accepted; synth output non-silent if TestSynth built)
- [ ] `test/state.test.js` passes (round-trip preserves params)
- [ ] `test/lifecycle.test.js` passes (dispose idempotent; load+dispose+load works; GC finalizer safe)
- [ ] `test/errors.test.js` passes (all documented error codes triggered correctly)

## Examples & Docs
- [ ] `examples/scan-plugins.js` runs end-to-end
- [ ] `examples/process-file.js` reads WAV, processes, writes WAV
- [ ] `examples/midi-synth.js` schedules MIDI notes and writes output WAV
- [ ] `examples/parameter-sweep.js` automates a parameter across blocks
- [ ] `README.md` documents install, quick start, supported platforms, links to examples
- [ ] `docs/API.md` (or typedoc) covers every public class and method
- [ ] `CHANGELOG.md` has a `## 0.1.0` entry
- [ ] `CONTRIBUTING.md` covers building from source and CI flow

## Performance
- [x] `process()` path allocates zero bytes after warmup (no `new`/`malloc`/`std::vector` resize on steady state)
  - Verified (Task 19 audit + fix, 2026-07-18): `ProcessData`, `AudioBusBuffers`,
    `ParameterChanges` (`clearQueue()` is zero-alloc), `EventList` (`clear()` is
    zero-alloc), and the channel pointer vectors are all reused members. The two
    local `std::vector<Napi::Float32Array>` that were previously constructed on
    every call have been eliminated — `Process()` now writes `Float32Array::Data()`
    pointers directly into the reused `inputChannelPtrs_`/`outputChannelPtrs_`
    members. No heap allocations remain on the steady-state path.
- [x] No locks held during the `IAudioProcessor::process` call (use `std::atomic` for restart flag)
  - Verified (Task 19 audit, 2026-07-18): `ComponentHandler` uses
    `std::atomic<int32_t> lastRestartFlags_` and a `std::function` callback (set
    once during `on()`); no `std::mutex` anywhere in `component_handler.{h,cc}`.
    `restartTsfnValid_` and `disposed_` are `std::atomic<bool>` with acquire/release
    ordering. TSFN `NonBlockingCall` is async and does not block the audio thread.
    The `new int32_t(flags)` in `emitRestart` only runs on restart notifications,
    not on the steady-state process path.
- [x] Throughput benchmark: <1x realtime on commodity hardware for the gain plugin at 48kHz / 512 samples
  - Verified (Task 19 audit, 2026-07-18): `test/benchmark.js` processes 60s of
    stereo audio at 48 kHz / 512 samples. Best of 3 runs: 0.012s elapsed =
    0.0002x realtime (~4818x throughput). Target <1x met by a wide margin.

## Final
- [x] `.gitignore` excludes `build/`, `node_modules/`, `*.node`, `prebuilds/*.node`, `test/plugin/build/`
- [x] `binding.gyp` references SDK submodule sources so `git submodule update --init --recursive && npm run build` works on a clean clone
- [x] License fields in `package.json` and `LICENSE` say MIT; VST3 SDK submodule is also MIT
- [ ] `git push origin main` succeeds
- [ ] CI passes on all 4 platforms
- [ ] Tag `v0.1.0` is pushed; release artifacts and npm package publish successfully
- [ ] `npm install nst3` on a clean machine (no toolchain) loads the binary without compilation
