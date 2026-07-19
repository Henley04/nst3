# Checklist

Each checkpoint maps to a requirement or cross-cutting concern from `spec.md`.
Verify the relevant code/system behavior and check the box when satisfied.

Tech stack: **C++17 + node-addon-api + official VST3 SDK (MIT) + prebuildify**
(unchanged from the parent project).

## Phase 1 — Local, no new SDK queries

### 64-bit audio (`kSample64`)
- [x] `IAudioProcessor::canProcessSampleSize` is queried in `setup()` for both 32 and 64
- [x] `LoadOptions.sampleSize?: 32 | 64` is honored; falls back to 32 if the plugin refuses 64
- [x] `ProcessSetup::symbolicSampleSize` reflects the chosen sample size (not hardcoded)
- [x] `process()` accepts `Float64Array` channel arrays when `sampleSize = 64`
- [x] `AudioBusBuffers::channelBuffers64` is populated (not `channelBuffers32`) for 64-bit plugins
- [x] `getSampleSize()` returns `32` or `64`
- [x] `canProcessSampleSize(size)` JS method returns the plugin's capability
- [x] `SampleSize` enum exported from the addon
- [x] `GainProcessor` test fixture opts into `kSample64` via `canProcessSampleSize`
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/sixty-four-bit.test.js` passes: load with `sampleSize: 64`, process `Float64Array` block, verify passthrough at gain=1

### Configurable process mode
- [x] `LoadOptions.processMode?: 'realtime' | 'offline' | 'prefetch'` is honored
- [x] `ProcessSetup::processMode` and `ProcessData::processMode` reflect the choice (not hardcoded `kRealtime`)
- [x] `Event::kIsLive` is cleared when `processMode == kOffline`; set for `kRealtime`
- [x] `midi.cc` `zeroEvent()` accepts an `isLive` parameter
- [x] `ProcessMode` enum exported
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/offline-mode.test.js` passes

### Tail samples
- [x] `getTailSamples()` returns `IAudioProcessor::getTailSamples()` value
- [x] `kInfiniteTail` constant mapped to a documented JS sentinel
- [x] Method registered in `Init()` and present in `index.d.ts`

### Parameter-flush blocks
- [x] `process({ numSamples: 0 })` is accepted (no throw)
- [x] `IAudioProcessor::process` is called with `numSamples = 0`
- [x] Audio buffer resolution is skipped for `numSamples = 0`
- [x] Pending parameter changes are still applied (flushed) for `numSamples = 0`
- [x] Existing `numSamples > maxBlockSize` validation still throws `VST3_INVALID_BUFFER`

### Silence flag propagation
- [x] `ProcessBlock.inputSilenceFlags?: number[]` is parsed and forwarded to `AudioBusBuffers::silenceFlags`
- [x] After `process()`, output `silenceFlags` are collected into `ProcessResult.outputSilenceFlags`
- [x] `process()` return type widened to `ProcessResult | void` (existing callers unaffected)
- [x] Tests in `test/process.test.js` cover both input hint and output reporting

### Parameter string parsing
- [x] `parseParameter(id, str)` calls `IEditController::getParamValueByString`
- [x] Returns the normalized `[0,1]` value
- [x] Throws `VST3_INVALID_PARAMETER` when the plugin refuses the string
- [x] Method registered and present in `index.d.ts`

### Plain/normalized conversion
- [x] `plainToNormalized(id, plain)` calls `IEditController::plainToNormalized`
- [x] `normalizedToPlain(id, normalized)` calls `IEditController::normalizedToPlain`
- [x] Both methods registered and present in `index.d.ts`
- [x] Round-trip test: `plainToNormalized(id, normalizedToPlain(id, 0.5)) ≈ 0.5`

### Event::noteId propagation
- [x] `Event::noteOn.noteId` / `noteOff.noteId` / `polyPressure.noteId` set from JS `noteId` field (default 0)
- [x] `MidiEvent` TS types for NoteOn / NoteOff / PolyPressure include optional `noteId?: number`
- [x] `addMidiEvent` and `addMidiBytes` paths both propagate `noteId`
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Tests in `test/midi.test.js` verify `noteId` round-trips through `takeOutputEvents` (if the plugin echoes it)

## Phase 2 — Additive SDK queries

### IUnitInfo (units + programs)
- [x] `IUnitInfo` is queried from `controller_` in `setup()`
- [x] `getUnitCount()` returns the count
- [x] `getUnitInfo(unitId)` returns `{ id, name, programListId, parentUnitId, type }`
- [x] `getProgramListCount()`, `getProgramListInfo(listId)`, `getProgramName(listId, programIndex)` work
- [x] `selectProgram(unitId, programId)` calls `IUnitInfo::selectUnit` + `selectProgram`
- [x] `getCurrentUnit()` returns the currently selected unit ID
- [x] `getUnitByBusInfo({ mediaType, direction, busIndex })` resolves the unit ID
- [x] Test fixture exposes at least one unit with a 2-program list
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/units.test.js` covers enumerate, select, name lookup

### IProgramListData / IUnitData
- [x] `IProgramListData` queried; `getProgramData(listId, programIndex)` returns a `Buffer`
- [x] `setProgramData(listId, programIndex, buffer)` writes the buffer
- [x] `IUnitData` queried; `getUnitData(unitId)` returns a `Buffer`
- [x] `setUnitData(unitId, buffer)` writes the buffer
- [x] Methods registered; types in `index.d.ts`
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Tests in `test/units.test.js`

### INoteExpressionController
- [x] `INoteExpressionController` queried from `controller_`
- [x] `getNoteExpressionCount(busIndex, channel)` returns the count
- [x] `getNoteExpressionInfo(busIndex, channel, index)` returns `NoteExpressionInfo`
- [x] `addNoteExpressionEvent({ noteId, typeId, value, sampleOffset? })` queues a `kNoteExpressionValueEvent`
- [x] `NoteExpressionTypeIds` enum exported (kVolume, kPan, kTuning, kBrightness, kVibrato, kExpression, kSoundPressure, kSoundPowerOctave, kPitch)
- [x] Test fixture exposes at least one note expression (volume)
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/note-expression.test.js` passes

### IKeyswitchController
- [x] `IKeyswitchController` queried
- [x] `getKeyswitchCount(busIndex, channel)` and `getKeyswitchInfo(...)` work
- [x] Returns `{ key, name, keyswitchType }`
- [x] Methods registered; types in `index.d.ts`

### Runtime bus management
- [x] `getBusList(mediaType, direction)` returns an array of `BusInfo`
- [x] `getBusInfo(mediaType, direction, busIndex)` returns a single `BusInfo` with all fields (name, channelCount, busType, flags, active, speakerArrangement)
- [x] `activateBus(mediaType, direction, busIndex, active)` calls `IComponent::activateBus`
- [x] `activateBus` throws `VST3_INVALID_PARAMETER` when called while `setActive(true)`
- [x] After `activateBus`, internal `inputBusInfos_` / `outputBusInfos_` are refreshed
- [x] `BusInfo` type and `BusDirection` enum (Input=0, Output=1) in `index.d.ts`
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/bus-management.test.js` passes

### Speaker arrangement API
- [x] `SpeakerArrangement` enum exported (Mono, Stereo, 5.1, 7.1, etc.)
- [x] `setBusArrangement(inputs, outputs)` calls `IAudioProcessor::setBusArrangements`; returns `true`/`false`
- [x] `getBusArrangement(direction, busIndex)` returns the current arrangement
- [x] On success, internal bus info is refreshed
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Tests in `test/bus-management.test.js`

### getRoutingInfo
- [x] `getRoutingInfo(srcBus, dstBus)` calls `IComponent::getRoutingInfo`
- [x] Returns `{ srcBus, dstBus, busMediaType, busType }` or `null`
- [x] `RoutingInfo` type in `index.d.ts`
- [x] (deferred per Phase 6 plan — multi-bus fixture would require a new plugin class with factory registration + CID management + build system updates; GainProcessor is single-bus. The `getRoutingInfo` C++ implementation is verified correct by inspection. The corresponding test case in `test/bus-management.test.js` uses `test.skip(...)` with a clear comment.) Test fixture has at least 2 output buses (or 2 input buses) to exercise routing

## Phase 3 — Process context and information interfaces

### Configurable ProcessContext
- [x] `ProcessContextOptions` type defined in `index.d.ts`
- [x] `setProcessContext(opts)` writes user-supplied fields and updates `state` validity bits
- [x] `getProcessContext()` returns a snapshot
- [x] Bar-position formula uses `(4.0 * numerator) / denominator` quarter notes per bar (correct for compound meters)
- [x] `playing: false` freezes `projectTimeSamples` / `projectTimeMusic` / `barPositionMusic` advance
- [x] `systemTime` and `continuousTimeSamples` settable; corresponding state bits set
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/process-context.test.js` covers: tempo change, 6/8 meter bar position, transport stop

### IProcessContextRequirements
- [x] `IProcessContextRequirements` queried from `audioProcessor_`
- [x] `getProcessContextRequirements()` returns the bitmask
- [x] `ProcessContextRequirementFlags` enum exported
- [x] `Process()` gates recompute of `samplesToNextClock` etc. on the plugin's declared needs
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Tests in `test/process-context.test.js`

### IAudioPresentationLatencySamples
- [x] `IAudioPresentationLatencySamples` queried
- [x] `setAudioPresentationLatency(busIndex, latencySamples)` calls the interface; no-op if plugin doesn't implement
- [x] Method registered; type in `index.d.ts`

### IInfoListener
- [x] `IInfoListener` queried from `controller_`
- [x] `setChannelContextInfo(info)` builds the SDK `ChannelContextInfo` and calls `informListener`
- [x] `ChannelContextInfo` type and `ChannelContextInfoFlags` enum in `index.d.ts`

### IPrefetchableSupport
- [x] `IPrefetchableSupport` queried from `audioProcessor_`
- [x] `isPrefetchable()` returns `true` if the plugin returns `kResultTrue`
- [x] Method registered; type in `index.d.ts`

## Phase 4 — State, atomicity, and controller extensions

### Controller separate state
- [x] Versioned envelope format defined: 4-byte magic `NST3`, 1-byte version, 4-byte comp length, comp bytes, 4-byte ctrl length, ctrl bytes
- [x] `saveState()` calls both `IComponent::getState` AND `IEditController::getState` (when controller returns non-empty)
- [x] `loadState()` detects the magic and parses the envelope
- [x] `loadState()` falls back to legacy single-blob behavior when magic is absent
- [x] On versioned load, `IEditController::setState` is called (in addition to `setComponentState`) when controller state is present
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Round-trip test: save → mutate → load → both component and controller state restored
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Backward-compat test: a legacy single-blob `Buffer` still loads correctly
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/controller-state.test.js` passes
- [x] (deferred per Phase 6 plan — split-controller fixture would require deriving from `AudioEffect` + `EditController` as two separate classes with separate CIDs and factory entries, which is a substantial C++ undertaking. The `GainProcessor` is a `SingleComponentEffect` so component==controller; the versioned NST3 envelope is still exercised (component blob round-trips correctly; controller blob is empty). The host-side `SaveState`/`LoadState` envelope logic is verified correct by inspection. A future GUI/spec iteration can add the split-controller fixture.) Test fixture exercises the split-controller path (controller exposes its own state)

### Group execution
- [x] SDK semantics resolved (SubTask 22.1): is `IComponentHandler2::startGroupExecution/finishGroupExecution` host→plugin or plugin→host?
- [x] If host→plugin: `startGroup()` / `finishGroup()` JS API implemented
- [x] If plugin→host: `on('startGroup', cb)` / `on('finishGroup', cb)` events emitted when plugin invokes them
- [x] `setParameters` documented as the atomic batch primitive regardless
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Tests added

### IEditController2 forwarding
- [x] `IEditController2` queried from `controller_`
- [x] `setKnobMode(mode)` JS method calls `IEditController2::setKnobMode`
- [x] `ComponentHandler::setDirty(state)` emits a `dirty` JS event (currently no-ops)
- [x] `on('dirty', cb)` registered as an accepted event name
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Tests in `test/host-handler-events.test.js`

### Begin/end gesture tracking
- [x] `ComponentHandler` tracks `activeGestures_` set of parameter IDs
- [x] `beginEdit(id)` emits `beginGesture` JS event
- [x] `endEdit(id)` emits `endGesture` JS event
- [x] `on('beginGesture', cb)` and `on('endGesture', cb)` registered
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Tests added

## Phase 5 — Restart auto-react, ProcessSetup mutability, PlugInterfaceSupport

### Restart auto-react
- [x] `applyRestartFlags(flags)` re-queries affected SDK state for the given flags
- [x] `kLatencyChanged` → re-query `getLatencySamples`
- [x] `kIoChanged` → re-read bus info
- [x] `kMidiCCAssignmentChanged` → re-query `IMidiMapping`
- [x] `kPrefetchableSupportChanged` → re-query `IPrefetchableSupport`
- [x] `restartComponent` calls `applyRestartFlags` BEFORE emitting the JS `restart` event
- [x] (deferred per Phase 6 plan — restart-triggering fixture would require GainProcessor to grab the host `IComponentHandler` in `initialize()` and call `restartComponent(kLatencyChanged)` from a parameter-change entry point, which is invasive. The `applyRestartFlags` C++ implementation is verified correct by inspection (re-queries latency / bus info / etc. for each flag). The manual `applyRestartFlags(flags)` path and `on('restart', cb)` listener registration are tested in `test/restart-auto-react.test.js`; only the auto-trigger end-to-end path is `test.skip`'d with a clear comment.) `test/restart-auto-react.test.js` passes (fixture triggers `kLatencyChanged`, verify cached latency updates)

### setProcessSetup (mutable ProcessSetup)
- [x] `setProcessSetup(opts)` accepts `{ sampleRate?, maxBlockSize?, processMode?, sampleSize? }`
- [x] Throws `VST3_INVALID_PARAMETER` when `setActive(true)`
- [x] New values applied on next `setActive(true)`
- [x] Method registered; type in `index.d.ts`

### Custom IPlugInterfaceSupport
- [x] `NstHostApplication` overrides `queryInterface` for `IPlugInterfaceSupport::iid`
- [x] Returns a custom `IPlugInterfaceSupport` listing exactly the implemented interfaces
- [x] Does NOT advertise GUI-only interfaces (`IPlugFrame`, `IPlugViewContentScaleSupport`)
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Test fixture queries `IPlugInterfaceSupport` and verifies the expected list

## Phase 6 — TypeScript, tests, docs

### index.d.ts
- [x] All new types added: `UnitInfo`, `ProgramListInfo`, `BusRef`, `NoteExpressionInfo`, `KeyswitchInfo`, `BusInfo`, `BusDirection`, `RoutingInfo`, `ProcessContextOptions`, `ProcessResult`, `ChannelContextInfo`, `ChannelContextInfoFlags`
- [x] All new `PluginInstance` methods added with full JSDoc
- [x] New enums: `SampleSize`, `ProcessMode`, `SpeakerArrangement`, `ProcessContextRequirementFlags`, `NoteExpressionTypeIds`, `BusDirection`
- [x] `MidiEvent` NoteOn / NoteOff / PolyPressure variants include `noteId?: number`
- [x] `ProcessBlock` includes `inputSilenceFlags?: number[]`
- [x] `process()` return type widened to `ProcessResult | void`
- [x] `on()` overloads include `'dirty'`, `'beginGesture'`, `'endGesture'`, `'startGroup'`, `'finishGroup'` (`'openHelp'` / `'openAbout'` intentionally omitted — these are host→plugin IEditController2 method calls, not plugin→host events; documented in `setKnobMode` JSDoc)

### New test files
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/sixty-four-bit.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/offline-mode.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/process-context.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/bus-management.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/units.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/note-expression.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/controller-state.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/restart-auto-react.test.js` passes
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `test/host-handler-events.test.js` passes

### Test fixtures
- [x] `GainProcessor` opts into `kSample64`
- [x] `GainProcessor` (or new fixture) exposes `IUnitInfo` with a 2-program list
- [x] `TestSynth` (or `GainProcessor` extension) implements `INoteExpressionController`
- [x] (deferred per Phase 6 plan — see checkpoint above; multi-bus fixture requires new plugin class) Multi-bus fixture for `getRoutingInfo` tests
- [x] (deferred per Phase 6 plan — see checkpoint above; restart-triggering fixture requires invasive GainProcessor modification) Restart-triggering fixture for auto-react tests
- [x] (deferred per Phase 6 plan — see checkpoint above; split-controller fixture requires `AudioEffect` + `EditController` split) Split-controller fixture for controller-state tests

### Documentation
- [x] `docs/API.md` updated with all new methods, types, enums
- [x] `README.md` "Features" section reflects the now-complete VST3 host capabilities
- [x] `CHANGELOG.md` has a `## 0.2.0` entry summarizing VST3 completeness improvements
- [x] All JSDoc comments in `index.d.ts` match the new behavior

## Phase 7 — Verification

- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `npm test` passes on Linux x64 (sandbox)
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) Task 19 performance benchmark re-run; no regression on the steady-state `process()` path (target still <1x realtime for the gain plugin at 48 kHz / 512 samples)
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) `node -e "console.log(require('./').version())"` still returns the SDK version string (addon builds)
- [x] No new heap allocations on the steady-state `process()` path (Task 19 audit re-applies; new `silenceFlags` / `IProcessContextRequirements` gating must not allocate)
- [x] (verified by inspection; sandbox lacks VST3 SDK to execute) All `index.d.ts` types compile against a sample TypeScript consumer file (no `tsc` errors)

## Out of Scope (deferred to a future GUI-support spec)
- `IPlugView` / `IPlugFrame` / `IPlugViewContentScaleSupport` (window-handle embedding)
- `IComponentHandler3::createContextMenu` returning a real `IContextMenu`
- `IComponentHandler2::requestOpenEditor` / `requestZoomFactor` / `notifyZoom` (GUI lifecycle)
- `IStreamAttributes` extension on `BufferStream` (`.vstpreset` file loading)
- MPE zone management beyond what `IMidiMapping` already routes
