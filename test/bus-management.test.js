'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const nst3 = require('../');
const { MediaType, BusDirection, BusType, SpeakerArrangement } = nst3;
const { loadPlugin, ensurePluginBuilt } = require('./helpers');

// NOTE: The GainProcessor fixture has exactly 1 audio input bus (stereo) and
// 1 audio output bus (stereo). A true multi-bus `getRoutingInfo` test would
// require a fixture with at least 2 input buses (e.g. main + sidechain) or
// 2 output buses (main + aux). Such a fixture is deferred (Task 30.4); the
// multi-bus routing test below is skipped with `test.skip(...)`.

describe('Runtime bus management', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  beforeEach(() => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
  });
  afterEach(() => {
    if (plugin) plugin.dispose();
    plugin = null;
  });

  test('getBusList(Audio, Input) returns 1 entry (stereo in)', () => {
    const list = plugin.getBusList(MediaType.Audio, BusDirection.Input);
    assert.ok(Array.isArray(list));
    assert.strictEqual(list.length, 1);
  });

  test('getBusList(Audio, Output) returns 1 entry (stereo out)', () => {
    const list = plugin.getBusList(MediaType.Audio, BusDirection.Output);
    assert.ok(Array.isArray(list));
    assert.strictEqual(list.length, 1);
  });

  test('getBusInfo(Audio, Input, 0) returns stereo Main active bus', () => {
    const info = plugin.getBusInfo(MediaType.Audio, BusDirection.Input, 0);
    assert.strictEqual(info.mediaType, MediaType.Audio);
    assert.strictEqual(info.direction, BusDirection.Input);
    assert.strictEqual(info.busIndex, 0);
    assert.strictEqual(info.channelCount, 2);
    assert.strictEqual(info.busType, BusType.Main);
    assert.strictEqual(info.active, true);
    assert.strictEqual(info.speakerArrangement, SpeakerArrangement.Stereo);
  });

  test('getBusInfo for an out-of-range index throws', () => {
    assert.throws(
      () => plugin.getBusInfo(MediaType.Audio, BusDirection.Input, 99),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        return true;
      }
    );
  });

  test('activateBus while setActive(true) throws VST3_INVALID_PARAMETER', () => {
    // plugin is currently active (setActive(true) in beforeEach).
    assert.throws(
      () => plugin.activateBus(MediaType.Audio, BusDirection.Input, 0, false),
      /VST3_INVALID_PARAMETER/
    );
  });

  test('deactivate then activateBus succeeds; getBusInfo reflects active state', () => {
    plugin.setProcessing(false);
    plugin.setActive(false);
    assert.doesNotThrow(() =>
      plugin.activateBus(MediaType.Audio, BusDirection.Input, 0, false)
    );
    let info = plugin.getBusInfo(MediaType.Audio, BusDirection.Input, 0);
    assert.strictEqual(info.active, false);
    // Re-activate.
    assert.doesNotThrow(() =>
      plugin.activateBus(MediaType.Audio, BusDirection.Input, 0, true)
    );
    info = plugin.getBusInfo(MediaType.Audio, BusDirection.Input, 0);
    assert.strictEqual(info.active, true);
    plugin.setActive(true);
    plugin.setProcessing(true);
  });

  test('getBusArrangement(Input, 0) returns Stereo', () => {
    const arr = plugin.getBusArrangement(BusDirection.Input, 0);
    assert.strictEqual(arr, SpeakerArrangement.Stereo);
  });

  test('setBusArrangement([Stereo], [Stereo]) returns true (no-op for the fixture)', () => {
    const ok = plugin.setBusArrangement(
      [SpeakerArrangement.Stereo],
      [SpeakerArrangement.Stereo]
    );
    assert.strictEqual(ok, true);
  });

  test('setBusArrangement([Mono], [Mono]) returns a boolean (fixture may refuse mono)', () => {
    const ok = plugin.setBusArrangement(
      [SpeakerArrangement.Mono],
      [SpeakerArrangement.Mono]
    );
    assert.strictEqual(typeof ok, 'boolean');
  });

  test('getRoutingInfo(0, 0) on the single-bus fixture returns null or a RoutingInfo object', () => {
    // GainProcessor has only 1 input and 1 output bus; the base
    // SingleComponentEffect does not override getRoutingInfo, so the SDK
    // typically returns kResultFalse. The host returns null in that case.
    // Some SingleComponentEffect variants may return {srcBus:0, dstBus:0, ...};
    // either is acceptable — we just assert null-or-object.
    const info = plugin.getRoutingInfo(0, 0);
    assert.ok(
      info === null || (typeof info === 'object' && info !== null),
      `expected null or object, got ${info}`
    );
  });

  test.skip('multi-bus getRoutingInfo requires a fixture with 2+ buses (Task 30.4 deferred)', () => {
    // A future fixture that adds an aux output bus (or sidechain input bus)
    // would let us verify getRoutingInfo(0, 0) returns a real RoutingInfo and
    // getRoutingInfo(0, 1) routes input 0 to aux output 1. The current
    // GainProcessor fixture has only 1 input bus and 1 output bus, so a true
    // multi-bus routing test cannot be expressed here.
  });
});
