# Checklist

Each checkpoint maps to a requirement or cross-cutting concern from `spec.md`.
Verify the relevant code/system behavior and check the box when satisfied.

## Foundation & Scaffolding
- [ ] `Cargo.toml` workspace exists with `nst3-host` and `nst3-napi` crates; `crate-type = ["cdylib"]` is set on the napi crate
- [ ] `package.json` declares `name: nst3`, `engines.node >=16.17`, `napi` field with `binaryName: "nst3"` and all 4 target triples
- [ ] `npm run build` produces `nst3.<platform>.<arch>.node` on the host machine
- [ ] `npm test` runs and passes the smoke test

## VST3 FFI Bindings
- [ ] All required VST3 interface vtables are defined in Rust (IPluginFactory{,2,3}, IComponent, IAudioProcessor, IEditController{,2}, IConnectionPoint, IParamValueQueue, IParameterChanges, IEventList, IHostApplication, IComponentHandler{,2,3}, IMessage, IAttributeList, IBStream, IStreamAttributes, IPlugView)
- [ ] TUIDs are defined as `pub const` 16-byte arrays for every interface
- [ ] `VstPtr<T>` smart pointer calls `release()` on drop
- [ ] No GPL-licensed crates are pulled in (verify `cargo tree -e features` shows no `vst3-sys` / `vst3-host`)

## Module Loading & Discovery
- [ ] macOS `.vst3` bundle path resolution works (`/path/X.vst3/Contents/MacOS/X`)
- [ ] Linux `.vst3` bundle path resolution works (`/path/X.vst3/Contents/x86_64-linux/X.so`)
- [ ] Windows `.vst3` file path resolution works (direct DLL load)
- [ ] `Host.scanDefaultLocations()` returns installed plugins on each platform
- [ ] `Host.scanDirectory(path)` recursively finds `.vst3` modules
- [ ] `Host.inspectPlugin(path)` returns metadata without instantiating DSP

## Plugin Instantiation
- [ ] `host.load(path, opts)` returns a `PluginInstance` for a valid plugin
- [ ] `IComponent` is created via factory `createInstance`
- [ ] `IAudioProcessor` is queried from the component
- [ ] `IEditController` is created (separately or via queryInterface) and connected via `IConnectionPoint`
- [ ] `IComponent::setHostApplication` is called with our `HostApplication`
- [ ] `BusInfo` for audio in/out and MIDI in/out is read and exposed via `getInfo()`

## Host Interfaces
- [ ] `HostApplication` returns "Node.js VST3 Host" from `getName`
- [ ] `HostApplication::createMessage` returns a working `IMessage`
- [ ] `ComponentHandler::beginEdit/performEdit/endEdit` cycle works (no crash, queue updated)
- [ ] `ComponentHandler::restartComponent` emits a `restart` JS event with restart flags
- [ ] `BStream` correctly serializes/deserializes bytes for `IBStream::read`/`write`/`seek`
- [ ] Connection-point messages route between component and controller

## Audio Processing
- [ ] `setActive(true)` calls `IAudioProcessor::setActive(true)` with the configured `ProcessSetup`
- [ ] `setProcessing(true)` calls `IAudioProcessor::setProcessing(true)`
- [ ] `process({inputs, outputs, numSamples})` calls `IAudioProcessor::process` and fills `outputs`
- [ ] Audio buffers are zero-copy (no copy between JS Float32Array and Rust process call — verify via address comparison in test)
- [ ] `getLatency()` returns the value reported by the plugin
- [ ] Speaker arrangement is set to Stereo (L R) by default when supported
- [ ] `setProcessing(false)` and `setActive(false)` clean up properly

## Parameter Management
- [ ] `getParameterCount()` matches `IEditController::getParameterCount`
- [ ] `getParameterInfo(i)` returns all fields correctly (title, units, flags, defaultNormalizedValue)
- [ ] `getParameter(id)` returns the current normalized value
- [ ] `setParameter(id, value)` updates controller and queues change for next process
- [ ] `setParameters([...])` applies multiple atomically
- [ ] `formatParameter(id, value)` returns the plugin's display string
- [ ] Parameter change queue is reused across `process` calls (no per-call allocation)

## MIDI & Events
- [ ] `addMidiEvent({type, channel, ...})` adds a VST3 `Event` to the input list
- [ ] `addMidiBytes(sampleOffset, [byte1, byte2, byte3])` parses raw MIDI correctly
- [ ] All supported MIDI event types are mapped (noteOn, noteOff, polyPressure, cc, programChange, channelPressure, pitchBend, sysEx)
- [ ] `takeOutputEvents()` returns events emitted by the plugin and clears the output queue
- [ ] `clearEvents()` resets the input list

## State Persistence
- [ ] `saveState()` returns a `Buffer` with the plugin's serialized state
- [ ] `loadState(buffer)` restores the plugin state (component + controller)
- [ ] Round-trip preserves parameter values and internal state

## Error Handling
- [ ] Loading a nonexistent path throws with `code: 'VST3_LOAD_FAILED'`
- [ ] Calling `process` before `setActive(true)` throws with `code: 'VST3_NOT_ACTIVE'`
- [ ] Calling `process` before `setProcessing(true)` throws with `code: 'VST3_NOT_PROCESSING'`
- [ ] Plugin fault during process marks the instance faulted and subsequent calls throw `VST3_FAULTED`
- [ ] `dispose()` is idempotent (safe to call twice)
- [ ] `PluginInstance` `Drop` releases all COM refs and unloads the module

## napi-rs Bindings
- [ ] `Host` class is exported with all discovery methods
- [ ] `PluginInstance` class is exported with all processing/parameter/MIDI/state methods
- [ ] `index.d.ts` is auto-generated by `napi build` and shipped in the package
- [ ] All public types are exported (`HostOptions`, `LoadOptions`, `PluginInfo`, `ParameterInfo`, `MidiEvent`, `ProcessBlock`, enums)
- [ ] `Symbol.dispose` is implemented for `using` syntax support

## Distribution & CI
- [ ] `.github/workflows/CI.yml` builds for windows-x64-msvc, darwin-x64, darwin-arm64, linux-x64-gnu
- [ ] `MACOSX_DEPLOYMENT_TARGET=10.13` is set for macOS builds
- [ ] Linux build installs `libasound2-dev` (or no native deps if not needed)
- [ ] On tag push: GitHub Release is created with `.node` artifacts
- [ ] On tag push: npm publish publishes 4 platform packages + main `nst3` package
- [ ] `optionalDependencies` in main `package.json` matches platform package names and versions

## Tests & Mock Plugin
- [ ] `crates/nst3-test-plugin` compiles to a real `.vst3` module for all 4 targets
- [ ] The test plugin is loadable by `nst3` AND by a real DAW (reaper/ableton) for sanity
- [ ] Integration tests pass on all CI platforms using the platform-specific test plugin
- [ ] State round-trip test passes
- [ ] MIDI event test passes (synth plugin produces non-silent output on noteOn)
- [ ] Error-code tests pass for all documented error scenarios

## Examples & Docs
- [ ] `examples/process-file.js` runs end-to-end and produces a valid output WAV
- [ ] `examples/scan-plugins.js` lists installed plugins on a developer machine
- [ ] `examples/midi-synth.js` plays a MIDI sequence through a synth plugin
- [ ] `examples/parameter-sweep.js` automates a parameter across multiple blocks
- [ ] `README.md` documents install, quick start, supported platforms, and links to examples
- [ ] `docs/API.md` (or auto-generated typedoc) covers every public class and method
- [ ] `CHANGELOG.md` has a `## 0.1.0` entry
- [ ] `CONTRIBUTING.md` covers building from source and the CI flow

## Performance
- [ ] `process()` path allocates zero bytes after warmup (verify with a debug assertion or allocator hook)
- [ ] No locks are held during the `IAudioProcessor::process` call
- [ ] Throughput benchmark: <1x realtime on commodity hardware for the test gain plugin at 48kHz / 512 samples

## Final
- [ ] `.gitignore` excludes `target/`, `node_modules/`, `*.node`, `npm/` platform subdirs
- [ ] `rust-toolchain.toml` pins stable Rust >=1.88
- [ ] License fields in `package.json` and Cargo manifests say MIT
- [ ] `git push origin main` succeeds
- [ ] CI passes on all 4 platforms
- [ ] Tag `v0.1.0` is pushed; release artifacts and npm packages publish successfully
- [ ] `npm install nst3` on a clean machine loads the binary without compilation
