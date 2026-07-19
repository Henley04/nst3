# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
