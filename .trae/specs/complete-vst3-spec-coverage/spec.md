# Complete VST3 Host Spec Coverage Spec

## Why
The existing `implement-vst3-host-node-library` spec delivered a working VST3 host for Node.js, but an audit against the official VST3 SDK interface set reveals ~30 missing or stubbed host behaviors that affect correctness, interoperability, and spec compliance. Real-world plugins — particularly instruments, mastering processors, surround effects, and offline renderers — depend on these behaviors (64-bit audio, `IUnitInfo` preset switching, `INoteExpressionController`, separate controller state, `kOffline` mode, configurable `ProcessContext`, bus activation API, tail samples, `IEditController::getParamValueByString`/`plainToNormalized`, group execution, etc.). Without them the host silently mis-negotiates with these plugins, drops state, or returns wrong results. This spec closes those gaps so `nst3` can be considered a complete VST3 host.

## What Changes
- **Audio processing extensions**:
  - Add 64-bit (`kSample64`) audio path with `Float64Array` zero-copy buffers and `IAudioProcessor::canProcessSampleSize` negotiation
  - Add `kOffline` and `kPrefetch` process modes alongside the existing `kRealtime`
  - Add `IAudioProcessor::getTailSamples()` exposure
  - Allow `numSamples === 0` "parameter-flush" blocks per the spec
  - Propagate `AudioBusBuffers::silenceFlags` to/from JS
- **Parameter management extensions**:
  - Add `IEditController::getParamValueByString` (parse string → normalized) as `parseParameter`
  - Add `IEditController::normalizedParameterToPlain` and `plainToNormalized`
  - Add `IComponentHandler2::startGroupExecution`/`finishGroupExecution` (host→plugin atomic batches)
  - Track `beginEdit`/`endEdit` gesture state and emit JS `beginGesture`/`endGesture` events
- **State persistence extensions**:
  - Save and restore **both** component state (`IComponent::getState`/`setState`) and controller state (`IEditController::getState`/`setState`) using a versioned, backward-compatible envelope
- **Bus management extensions**:
  - Add `getBusList` / `getBusInfo` / `activateBus` API for runtime bus toggling (requires `setActive(false)` first)
  - Add `setBusArrangement` / `getBusArrangement` with exported `SpeakerArrangement` enum (Mono/Stereo/5.1/7.1/…)
  - Add `IComponent::getRoutingInfo` exposure for multi-bus plugins
- **Process context API**:
  - Add `setProcessContext(opts)` / `getProcessContext()` so JS users can drive tempo, time signature, sample position, bar position, transport state, cycle, recording, `samplesToNextClock`, `systemTime`, `continousTimeSamples`
  - Fix bar-position computation for non-4/4 meters (use `4 * numerator / denominator` quarter notes per bar correctly for compound meters)
  - Honor `kCycleActive`, `kRecording`, `kSystemTimeValid`, `kContinousTimeValid` state bits
- **Unit info & presets** (`IUnitInfo`):
  - Add `getUnitCount` / `getUnitInfo` / `getProgramListCount` / `getProgramListInfo` / `getProgramName` / `selectProgram` / `getCurrentUnit` / `getUnitByBusInfo`
  - Add `IProgramListData` (`getProgramData` / `setProgramData`) and `IUnitData` (`getUnitData` / `setUnitData`) for bulk data persistence
- **Note expression** (`INoteExpressionController`):
  - Add `getNoteExpressionCount` / `getNoteExpressionInfo` / `addNoteExpressionEvent`
  - Fix `Event::noteId` propagation on note on/off/polyPressure (currently always 0)
  - Make `Event::kIsLive` flag settable (auto-clear in `kOffline` mode)
- **MIDI controller mappings** (`IKeyswitchController`):
  - Add `getKeyswitchCount` / `getKeyswitchInfo`
- **Edit controller extensions** (`IEditController2`):
  - Forward `setKnobMode`, `openHelp`, `openAboutBox` to JS events
  - Track `setDirty` state and emit `dirty` event
- **Host-plugin information interfaces**:
  - Add `IAudioPresentationLatencySamples::setAudioPresentationLatencySamples` exposure
  - Query `IProcessContextRequirements` and gate `ProcessContext` field updates accordingly
  - Add `IInfoListener::setChannelContextInfo` exposure
  - Add `IPrefetchableSupport::isPrefetchable` query
- **Restart auto-react**:
  - When `restartComponent` is called with `kLatencyChanged` / `kIoChanged` / `kMidiCCAssignmentChanged` / `kRoutingInfoChanged` / `kParamTitlesChanged` / `kNoteExpressionChanged`, automatically re-query the corresponding SDK state before delivering the JS `restart` event, and expose `applyRestartFlags(flags)` for explicit user handling
- **ProcessSetup mutability**:
  - Add `setProcessSetup(opts)` to allow changing sample rate / block size / process mode / sample size without reloading (requires `setActive(false)` first)
- **PlugInterfaceSupport accuracy**:
  - Override `NstHostApplication` to install a custom `IPlugInterfaceSupport` advertising exactly the interfaces the host implements
- **TypeScript surface**:
  - Extend `index.d.ts` with all new types: `UnitInfo`, `ProgramListInfo`, `NoteExpressionInfo`, `KeyswitchInfo`, `BusInfo`, `RoutingInfo`, `ProcessContextOptions`, `ProcessMode`, `SampleSize`, `SpeakerArrangement` enum, `ChannelContextInfo`, `ProcessResult`, plus all new method signatures and event names
- **Test plugin enhancements**:
  - Extend the `GainProcessor` test fixture to opt into `kSample64` so the 64-bit path is exercised
  - Add a second test fixture (or extend the existing one) that exposes `IUnitInfo` (one program list, two programs) so the unit-info path is exercised
  - Add an `INoteExpressionController`-implementing fixture (small synth) for note-expression tests
- **Documentation**:
  - Update `index.d.ts`, `docs/API.md`, and `README.md` to cover the new API surface

**Out of scope** (deferred until GUI support is added in a separate spec):
- `IPlugView` / `IPlugFrame` / `IPlugViewContentScaleSupport` (window-handle embedding)
- `IComponentHandler3::createContextMenu` returning a real `IContextMenu` (only meaningful with a visible editor)
- `IComponentHandler2::requestOpenEditor` / `requestZoomFactor` / `notifyZoom` (GUI lifecycle)
- `IStreamAttributes` extension on `BufferStream` (only meaningful for `.vstpreset` file loading)
- MPE zone management beyond what `IMidiMapping` already routes

## Impact
- Affected specs: `implement-vst3-host-node-library` (extends its API surface; **no breaking changes** — all new APIs are additive; existing `saveState`/`loadState` Buffer shape is preserved via a versioned envelope)
- Affected code:
  - `src/plugin_instance.{h,cc}` — largest delta: 64-bit path, ProcessSetup mutability, bus activation, routing, ProcessContext API, restart auto-react, group execution, parameter parse/plain conversions, tail samples, silenceFlags, numSamples=0, units, note expression, keyswitches, prefetchable, audio presentation latency, info listener, process context requirements
  - `src/midi.{h,cc}` — `Event::noteId` propagation, `kIsLive` flag settable, `INoteExpressionController` event construction
  - `src/component_handler.{h,cc}` — group execution, setDirty event, begin/end gesture tracking, IEditController2 forwarding
  - `src/host_application.{h,cc}` — custom `IPlugInterfaceSupport` registering the actual host interface set
  - `src/buffer_stream.{h,cc}` — controller-state envelope helpers (length-prefixed framing)
  - `src/addon.cc` — new enum exports (`SpeakerArrangement`, `ProcessMode`, `SampleSize`, `ProcessContextRequirementFlags`, `NoteExpressionTypeIds`, `ChannelContextInfoFlags`)
  - `index.d.ts` — full TS surface for all new APIs and types
  - `test/plugin/source/gainprocessor.{h,cpp}` — opt into `kSample64`
  - `test/plugin/source/` — new test fixtures for `IUnitInfo` and `INoteExpressionController`
  - `test/*.test.js` — new test files: `units.test.js`, `note-expression.test.js`, `process-context.test.js`, `bus-management.test.js`, `sixty-four-bit.test.js`, `offline-mode.test.js`, `controller-state.test.js`, `group-execution.test.js`, `keyswitches.test.js`, `restart-auto-react.test.js`
  - `docs/API.md` — document new APIs
- No breaking changes to existing public API. The `saveState()`/`loadState()` Buffer format gains a 4-byte magic+version header so existing buffers continue to load (legacy format detected by absence of magic).
- No native binary distribution impact. No new SDK submodule needed — all required interfaces are already in the `vst3sdk` submodule that ships with the project.

## ADDED Requirements

### Requirement: 64-bit Audio Processing
The system SHALL support processing audio in 64-bit (`kSample64`) precision when the plugin advertises support via `IAudioProcessor::canProcessSampleSize(kSample64)`, accepting `Float64Array` channel buffers from JS and routing them through `AudioBusBuffers::channelBuffers64`.

#### Scenario: Plugin supports 64-bit
- **WHEN** the user calls `host.load(path, { sampleSize: 64 })` and the plugin's `canProcessSampleSize(kSample64)` returns `kResultTrue`
- **THEN** `setupProcessing` is called with `symbolicSampleSize = kSample64`, `process()` accepts `Float64Array` channel arrays, and `getSampleSize()` returns `64`

#### Scenario: Plugin does not support 64-bit
- **WHEN** the user requests `sampleSize: 64` but the plugin returns `kResultFalse`
- **THEN** the host falls back to `kSample32` and `getSampleSize()` returns `32`

#### Scenario: Query sample size capability
- **WHEN** the user calls `plugin.canProcessSampleSize(64)`
- **THEN** the system returns `true` if `IAudioProcessor::canProcessSampleSize(kSample64) == kResultTrue`, `false` otherwise

### Requirement: Configurable Process Mode
The system SHALL allow the user to select the VST3 process mode (`kRealtime`, `kOffline`, `kPrefetch`) at load time and at runtime via `setProcessSetup`.

#### Scenario: Offline rendering
- **WHEN** the user calls `host.load(path, { processMode: 'offline' })` and processes audio
- **THEN** `ProcessSetup::processMode = kOffline`, `ProcessData::processMode = kOffline`, and `Event::kIsLive` is cleared on all queued events

#### Scenario: Switch process mode at runtime
- **WHEN** the user calls `plugin.setProcessSetup({ processMode: 'offline' })` while active
- **THEN** the system throws `VST3_NOT_ACTIVE` if `setActive(false)` has not been called first; otherwise the new setup is applied on the next `setActive(true)`

### Requirement: Tail Sample Reporting
The system SHALL expose `IAudioProcessor::getTailSamples()` so callers know how many samples the plugin produces after input goes silent.

#### Scenario: Reverb plugin reports tail
- **WHEN** the user calls `plugin.getTailSamples()` after `setActive(true)`
- **THEN** the system returns the value reported by `IAudioProcessor::getTailSamples()` (a non-negative sample count, or the SDK's `kInfiniteTail` constant)

### Requirement: Parameter-Flush Blocks
The system SHALL accept `numSamples === 0` in `process()` to flush pending parameter changes without processing audio, per the VST3 spec.

#### Scenario: Flush parameter changes
- **WHEN** the user calls `plugin.process({ inputs: [], outputs: [], numSamples: 0 })`
- **THEN** the system calls `IAudioProcessor::process` with `numSamples = 0`, no audio buffer resolution is performed, and pending parameter changes are still applied

### Requirement: Silence Flag Propagation
The system SHALL propagate `AudioBusBuffers::silenceFlags` from JS input to the plugin, and from plugin output back to JS, so plugins can skip silent channels and hosts can skip encoding silent output.

#### Scenario: Input silence hint
- **WHEN** the user calls `plugin.process({ inputs, outputs, numSamples, inputSilenceFlags: [0b11, 0] })`
- **THEN** `AudioBusBuffers::silenceFlags` for input bus 0 is set to `0b11` and bus 1 to `0`

#### Scenario: Output silence reported
- **WHEN** the plugin sets `silenceFlags` on output buses during `process()`
- **THEN** `process()` returns `{ outputSilenceFlags: number[] }` (the return type widens from `void` to `ProcessResult | void`; existing callers ignoring the return value are unaffected)

### Requirement: Parameter String Parsing
The system SHALL expose `IEditController::getParamValueByString` so users can parse plugin-formatted parameter strings (e.g. `"440 Hz"`) back to normalized `[0,1]` values.

#### Scenario: Parse cutoff string
- **WHEN** the user calls `plugin.parseParameter(id, "440 Hz")`
- **THEN** the system calls `IEditController::getParamValueByString(id, "440 Hz", &value)` and returns the resulting normalized value

### Requirement: Plain/Normalized Conversion
The system SHALL expose `IEditController::plainToNormalized` and `IEditController::normalizedToPlain` so users can convert between the engine's `[0,1]` representation and plugin-defined plain values (Hz, dB, %, etc.).

#### Scenario: Convert Hz to normalized
- **WHEN** the user calls `plugin.plainToNormalized(id, 8000)`
- **THEN** the system returns the normalized `[0,1]` value the plugin computes from `8000`

#### Scenario: Convert normalized to Hz
- **WHEN** the user calls `plugin.normalizedToPlain(id, 0.5)`
- **THEN** the system returns the plain value (e.g. `8000`) the plugin computes from `0.5`

### Requirement: Group Execution
The system SHALL expose `IComponentHandler2::startGroupExecution` / `finishGroupExecution` so JS users can batch multiple `setParameter` calls into an atomic transaction from the plugin's perspective.

#### Scenario: Atomic preset switch
- **WHEN** the user calls `plugin.startGroup()`, then `plugin.setParameters([...20 changes...])`, then `plugin.finishGroup()`
- **THEN** the plugin's `IComponentHandler2::startGroupExecution()` and `finishGroupExecution()` are invoked, bracketing the parameter updates as one transaction

### Requirement: Begin/End Gesture Tracking
The system SHALL track active `beginEdit`/`endEdit` gestures on the host side and emit JS `beginGesture`/`endGesture` events when the plugin's controller starts or ends a parameter gesture.

#### Scenario: Plugin turns a knob in its GUI
- **WHEN** the plugin's controller calls `IComponentHandler::beginEdit(id)` followed by `performEdit(id, value)` calls and finally `endEdit(id)`
- **THEN** the host emits `beginGesture` (with `id`), `performEdit` updates propagate to `inputParams_`, and `endGesture` (with `id`) is emitted on the JS side

### Requirement: Controller State Persistence
The system SHALL save and restore **both** component state and controller state, using a versioned envelope that is backward-compatible with the existing single-blob `Buffer` format.

#### Scenario: Save full state (split-component plugin)
- **WHEN** the user calls `plugin.saveState()` on a split-component plugin whose controller exposes its own state
- **THEN** the returned `Buffer` contains a 4-byte magic (`NST3`), 1-byte version, a 4-byte component-state length, the component-state bytes, a 4-byte controller-state length, and the controller-state bytes

#### Scenario: Load legacy single-blob state
- **WHEN** the user calls `plugin.loadState(buffer)` where `buffer` does NOT start with the `NST3` magic
- **THEN** the system treats the entire buffer as component state (legacy behavior) and calls only `IComponent::setState` + `IEditController::setComponentState`, preserving backward compatibility

#### Scenario: Load versioned state
- **WHEN** the user calls `plugin.loadState(buffer)` where `buffer` starts with `NST3` magic
- **THEN** the system parses the envelope, calls `IComponent::setState(componentStream)`, `IEditController::setComponentState(componentStream)`, and (if controller state is present) `IEditController::setState(controllerStream)`

### Requirement: Runtime Bus Management
The system SHALL expose `getBusList`, `getBusInfo`, and `activateBus` so JS users can enumerate buses and toggle individual bus activation at runtime.

#### Scenario: Disable sidechain input
- **WHEN** the user calls `plugin.activateBus(MediaType.Audio, BusDirection.Input, 1, false)` while `setActive(false)`
- **THEN** the system calls `IComponent::activateBus(kAudio, kInput, 1, false)` and updates internal bus-info state

#### Scenario: Activate while active throws
- **WHEN** the user calls `plugin.activateBus(...)` while `setActive(true)`
- **THEN** the system throws `VST3_INVALID_PARAMETER` with a message explaining the bus state may only change while inactive

### Requirement: Speaker Arrangement API
The system SHALL export a `SpeakerArrangement` enum and expose `setBusArrangement` / `getBusArrangement` so users can negotiate surround layouts beyond the built-in stereo/mono fallback.

#### Scenario: Request 5.1 surround
- **WHEN** the user calls `plugin.setBusArrangement([SpeakerArrangement._51], [SpeakerArrangement._51])` and the plugin supports it
- **THEN** the system calls `IAudioProcessor::setBusArrangements` with `k51` and returns `true`; subsequent `getBusArrangement(0, 0)` returns `SpeakerArrangement._51`

#### Scenario: Plugin refuses arrangement
- **WHEN** the user calls `plugin.setBusArrangement(...)` and the plugin returns `kResultFalse`
- **THEN** the system returns `false` and the previous arrangement is unchanged

### Requirement: Bus Routing Info
The system SHALL expose `IComponent::getRoutingInfo` so users can determine how input buses route to output buses in multi-bus plugins.

#### Scenario: Query routing
- **WHEN** the user calls `plugin.getRoutingInfo(srcBusIndex, dstBusIndex)`
- **THEN** the system returns `{ srcBus, dstBus, busMediaType, busType }` if `getRoutingInfo` returns `kResultTrue`, otherwise `null`

### Requirement: Configurable Process Context
The system SHALL expose `setProcessContext(opts)` / `getProcessContext()` so JS users can drive tempo, time signature, transport state, and other `ProcessContext` fields instead of the hardcoded 120 BPM / 4/4 defaults.

#### Scenario: Set tempo to 140 BPM
- **WHEN** the user calls `plugin.setProcessContext({ tempo: 140, timeSigNumerator: 4, timeSigDenominator: 4, playing: true })`
- **THEN** the next `process()` call sees `ProcessContext::tempo = 140`, `kTempoValid | kTimeSigValid | kPlaying` set, and `projectTimeMusic` advances at the new tempo

#### Scenario: Compound meter (6/8)
- **WHEN** the user sets `timeSigNumerator: 6, timeSigDenominator: 8`
- **THEN** `barPositionMusic` advances by `3.0` quarter notes per bar (one bar of 6/8 = 3 quarter notes), not the previous incorrect `6*4/8 = 3.0` which happened to coincide but was computed via a wrong formula for non-4/4 meters — the formula is corrected to `(numerator * 4) / denominator` quarter notes per bar, which is the VST3 convention

#### Scenario: Stop transport
- **WHEN** the user calls `plugin.setProcessContext({ playing: false })`
- **THEN** `ProcessContext::state` has `kPlaying` cleared; `projectTimeSamples` does not advance on subsequent `process()` calls

#### Scenario: System time and continuous time
- **WHEN** the user calls `plugin.setProcessContext({ systemTime: <nanoseconds>, continuousTimeSamples: <samples> })`
- **THEN** `ProcessContext::systemTime` and `continousTimeSamples` are set and `kSystemTimeValid` / `kContinousTimeValid` state bits are set accordingly

### Requirement: Unit Info and Programs
The system SHALL expose `IUnitInfo` so JS users can enumerate units, program lists, program names, switch programs, and resolve which unit a bus belongs to.

#### Scenario: Enumerate units
- **WHEN** the user calls `plugin.getUnitCount()` and `plugin.getUnitInfo(unitId)`
- **THEN** the system returns the count and `UnitInfo { id, name, programListId, parentUnitId, type }` from `IUnitInfo`

#### Scenario: Enumerate program lists
- **WHEN** the user calls `plugin.getProgramListCount()`, `plugin.getProgramListInfo(listId)`, and `plugin.getProgramName(listId, programIndex)`
- **THEN** the system returns the program list metadata and program names from `IUnitInfo`

#### Scenario: Switch program
- **WHEN** the user calls `plugin.selectProgram(unitId, programId)`
- **THEN** the system calls `IUnitInfo::selectUnit(unitId)` followed by `selectProgram(unitId, programId)` and the plugin's parameters update to the selected program

#### Scenario: Resolve unit for a bus
- **WHEN** the user calls `plugin.getUnitByBusInfo({ mediaType, direction, busIndex })`
- **THEN** the system calls `IUnitInfo::getUnitByBusInfo` and returns the corresponding `unitId`

### Requirement: Program and Unit Bulk Data
The system SHALL expose `IProgramListData` and `IUnitData` for per-program and per-unit bulk data persistence.

#### Scenario: Read program data
- **WHEN** the user calls `plugin.getProgramData(listId, programIndex)`
- **THEN** the system calls `IProgramListData::getProgramData(listId, programIndex, stream)` and returns a `Buffer`

#### Scenario: Write unit data
- **WHEN** the user calls `plugin.setUnitData(unitId, buffer)`
- **THEN** the system calls `IUnitData::setUnitData(unitId, stream)` with the buffer contents

### Requirement: Note Expression
The system SHALL expose `INoteExpressionController` so JS users can enumerate per-note expression types and queue `kNoteExpressionValueEvent`s targeting specific notes by `noteId`.

#### Scenario: Enumerate note expressions
- **WHEN** the user calls `plugin.getNoteExpressionCount(busIndex, channel)` and `plugin.getNoteExpressionInfo(busIndex, channel, index)`
- **THEN** the system returns the count and `NoteExpressionInfo { typeId, title, shortTitle, unitId, associatedParameterId, flags }` from `INoteExpressionController`

#### Scenario: Send note expression
- **WHEN** the user calls `plugin.addNoteExpressionEvent({ noteId, typeId, value, sampleOffset })`
- **THEN** a `kNoteExpressionValueEvent` is added to the input event list with the supplied `noteId`, `typeId`, `value`, and `sampleOffset`

#### Scenario: Note identity propagated
- **WHEN** the user calls `plugin.addMidiEvent({ type: 'noteOn', channel, note, velocity, noteId: 42 })`
- **THEN** the resulting VST3 `Event::noteOn.noteId = 42`; subsequent `addNoteExpressionEvent({ noteId: 42, ... })` targets that note instance

### Requirement: MIDI Keyswitches
The system SHALL expose `IKeyswitchController` so JS users can enumerate a plugin's static keyswitch assignments.

#### Scenario: Enumerate keyswitches
- **WHEN** the user calls `plugin.getKeyswitchCount(busIndex, channel)` and `plugin.getKeyswitchInfo(busIndex, channel, index)`
- **THEN** the system returns the count and `{ key, name, keyswitchType }` from `IKeyswitchController`

### Requirement: Edit Controller 2 Forwarding
The system SHALL query `IEditController2` and forward `setKnobMode`, `openHelp`, `openAboutBox`, and `setDirty` calls to JS events.

#### Scenario: Plugin requests help
- **WHEN** the plugin calls `IEditController2::openHelp()` (or `IComponentHandler2::setDirty(state)` is invoked)
- **THEN** the host emits `openHelp` (or `dirty` with the state boolean) on the JS side via `ThreadSafeFunction`

### Requirement: Host-Plugin Information Interfaces
The system SHALL query and expose `IAudioPresentationLatencySamples`, `IProcessContextRequirements`, `IInfoListener`, and `IPrefetchableSupport`.

#### Scenario: Audio presentation latency
- **WHEN** the user calls `plugin.setAudioPresentationLatency(busIndex, latencySamples)`
- **THEN** the system calls `IAudioPresentationLatencySamples::setAudioPresentationLatencySamples(busIndex, latencySamples)` if the plugin implements it

#### Scenario: Process context requirements
- **WHEN** the user calls `plugin.getProcessContextRequirements()`
- **THEN** the system returns the bitmask from `IProcessContextRequirements::getProcessContextRequirements()`; internally, the host only fills `ProcessContext` fields the plugin declared it needs

#### Scenario: Channel context info
- **WHEN** the user calls `plugin.setChannelContextInfo(info)`
- **THEN** the system calls `IInfoListener::informListener(info)` if the plugin implements it

#### Scenario: Prefetchable query
- **WHEN** the user calls `plugin.isPrefetchable()`
- **THEN** the system returns `true` if `IPrefetchableSupport::isPrefetchable() == kResultTrue`

### Requirement: Restart Auto-React
The system SHALL automatically re-query affected SDK state when the plugin calls `restartComponent` with specific flags, in addition to emitting the JS `restart` event.

#### Scenario: Latency changed
- **WHEN** the plugin calls `restartComponent(kLatencyChanged)`
- **THEN** the host re-queries `IAudioProcessor::getLatencySamples()` (caching the new value), then emits `restart` with the `kLatencyChanged` flag

#### Scenario: IO changed
- **WHEN** the plugin calls `restartComponent(kIoChanged)`
- **THEN** the host re-reads all bus info via `IComponent::getBusCount` + `getBusInfo` and updates `inputBusInfos_` / `outputBusInfos_`, then emits `restart` with the `kIoChanged` flag

#### Scenario: MIDI CC assignment changed
- **WHEN** the plugin calls `restartComponent(kMidiCCAssignmentChanged)`
- **THEN** the host re-queries `IMidiMapping` for all known controller numbers (no-op if the plugin doesn't implement `IMidiMapping`), then emits `restart` with the flag

#### Scenario: Routing info changed
- **WHEN** the plugin calls `restartComponent(kRoutingInfoChanged)`
- **THEN** the host invalidates its cached routing info, then emits `restart` with the flag

#### Scenario: Manual application
- **WHEN** the user calls `plugin.applyRestartFlags(flags)` after handling the `restart` event themselves
- **THEN** the system re-queries the affected SDK state for the given flags

### Requirement: ProcessSetup Mutability
The system SHALL allow changing `sampleRate`, `maxBlockSize`, `processMode`, and `sampleSize` at runtime via `setProcessSetup(opts)`.

#### Scenario: Change sample rate
- **WHEN** the user calls `plugin.setProcessSetup({ sampleRate: 96000 })` while `setActive(false)`
- **THEN** the next `setActive(true)` calls `setupProcessing` with the new sample rate

#### Scenario: Change while active throws
- **WHEN** the user calls `plugin.setProcessSetup(...)` while `setActive(true)`
- **THEN** the system throws `VST3_INVALID_PARAMETER` instructing the user to call `setActive(false)` first

### Requirement: Accurate PlugInterfaceSupport
The system SHALL install a custom `IPlugInterfaceSupport` on `NstHostApplication` that advertises exactly the host interfaces the addon implements, so plugins querying host capabilities get accurate results.

#### Scenario: Plugin queries for IComponentHandler2
- **WHEN** a plugin queries `IHostApplication::queryInterface(IPlugInterfaceSupport::iid)` and asks `isPlugInterfaceSupported(IComponentHandler2::iid)`
- **THEN** the host returns `kResultTrue` (since the host implements `IComponentHandler2`)

#### Scenario: Plugin queries for an unimplemented interface
- **WHEN** a plugin asks `isPlugInterfaceSupported(IPlugViewContentScaleSupport::iid)` (out-of-scope, not implemented)
- **THEN** the host returns `kResultFalse`

## MODIFIED Requirements

### Requirement: Audio Processing (modified)
The system SHALL process audio through the plugin with zero-copy buffer handling, supporting configurable channel counts, sample rates, block sizes, **sample sizes (32 or 64 bit)**, and **process modes (realtime, offline, prefetch)**.

#### Scenario: Process a 32-bit realtime block (unchanged)
- **WHEN** the user calls `plugin.process({ inputs: [left, right], outputs: [outLeft, outRight], numSamples: 512 })` with default load options
- **THEN** `IAudioProcessor::process` is called with `symbolicSampleSize = kSample32`, `processMode = kRealtime`, and `outputs` Float32Arrays are filled

#### Scenario: Process a 64-bit block (new)
- **WHEN** the user loaded with `sampleSize: 64` and calls `plugin.process({ inputs: [left64, right64], outputs: [outL64, outR64], numSamples: 512 })` with `Float64Array` channels
- **THEN** `IAudioProcessor::process` is called with `symbolicSampleSize = kSample64` and `AudioBusBuffers::channelBuffers64` is set to the Float64Array data pointers

#### Scenario: Process with silence hint (new)
- **WHEN** the user calls `plugin.process({ inputs, outputs, numSamples, inputSilenceFlags: [3, 0] })`
- **THEN** `AudioBusBuffers::silenceFlags` for input bus 0 is set to `3` (both channels silent) and bus 1 to `0`; the result includes `outputSilenceFlags` reflecting the plugin's output silence flags

### Requirement: State Persistence (modified)
The system SHALL save and restore the plugin's full state — **component state via `IComponent::getState`/`setState` and, when the controller exposes its own state, controller state via `IEditController::getState`/`setState`** — using a versioned envelope that is backward-compatible with the existing single-blob `Buffer` format.

#### Scenario: Save state (modified)
- **WHEN** the user calls `plugin.saveState()`
- **THEN** the system calls `IComponent::getState(IBStream)`, then (if the controller implements `IEditController::getState` and returns a non-empty blob) calls `IEditController::getState(IBStream)`; the returned `Buffer` is a versioned envelope containing both blobs. For plugins whose controller returns an empty blob, the envelope still contains the controller-state length prefix set to `0`.

#### Scenario: Load state (modified)
- **WHEN** the user calls `plugin.loadState(buffer)`
- **THEN** the system detects the format: if the first 4 bytes are the `NST3` magic, parse the versioned envelope and call `IComponent::setState` + `IEditController::setComponentState` + (if controller state is present) `IEditController::setState`. Otherwise, treat the entire buffer as legacy component state and call only `IComponent::setState` + `IEditController::setComponentState`.

#### Scenario: Round-trip preserves state (unchanged)
- **WHEN** the user saves state, modifies parameters, then loads the saved state
- **THEN** the parameters and internal state are restored to their saved values

### Requirement: Host Application Support (modified)
The system SHALL implement `IHostApplication`, `IComponentHandler`, `IComponentHandler2`, and `IComponentHandler3` (subclassing the VST3 SDK helpers where available) so the plugin can communicate with the host, **and shall install a custom `IPlugInterfaceSupport` advertising exactly the interfaces the host implements**.

#### Scenario: Parameter edit gesture (modified to include JS events)
- **WHEN** the plugin's controller calls `IComponentHandler::beginEdit(id)`
- **THEN** the host tracks the active gesture in an internal set AND emits a JS `beginGesture` event with the parameter ID
- **WHEN** `performEdit(id, value)` is called during the gesture
- **THEN** the host updates the parameter queue (existing behavior)
- **WHEN** `endEdit(id)` is called
- **THEN** the host removes the gesture from the set AND emits a JS `endGesture` event with the parameter ID

#### Scenario: Group execution (new)
- **WHEN** the plugin's controller calls `IComponentHandler2::startGroupExecution()` / `finishGroupExecution()`
- **THEN** the host emits `startGroup` / `finishGroup` JS events (in addition to the host-initiated group API in the Group Execution requirement)

#### Scenario: Dirty state (new)
- **WHEN** the plugin's controller calls `IComponentHandler2::setDirty(state)`
- **THEN** the host emits a `dirty` JS event with the state boolean

## REMOVED Requirements
(None — this spec is purely additive.)
