'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const nst3 = require('../');
const { KnobMode } = nst3;
const { loadPlugin, ensurePluginBuilt } = require('./helpers');

// The events 'dirty', 'beginGesture', 'endGesture', 'startGroup',
// 'finishGroup' are emitted by the plugin via the host's IComponentHandler /
// IComponentHandler2 interfaces. The GainProcessor fixture does not call
// setDirtyState / beginEdit / endEdit / startGroupExecution /
// finishGroupExecution, so we can only verify listener registration works.
// A future fixture that invokes those host-side calls would exercise the
// emission path.

describe('Host handler events', { skip: !ensurePluginBuilt() }, () => {
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

  test('on("restart", cb) registration does not throw', () => {
    assert.doesNotThrow(() => plugin.on('restart', () => {}));
  });

  test('on("dirty", cb) registration does not throw', () => {
    assert.doesNotThrow(() => plugin.on('dirty', () => {}));
  });

  test('on("beginGesture", cb) registration does not throw', () => {
    assert.doesNotThrow(() => plugin.on('beginGesture', () => {}));
  });

  test('on("endGesture", cb) registration does not throw', () => {
    assert.doesNotThrow(() => plugin.on('endGesture', () => {}));
  });

  test('on("startGroup", cb) registration does not throw', () => {
    assert.doesNotThrow(() => plugin.on('startGroup', () => {}));
  });

  test('on("finishGroup", cb) registration does not throw', () => {
    assert.doesNotThrow(() => plugin.on('finishGroup', () => {}));
  });

  test('on() with an invalid event name throws', () => {
    assert.throws(
      () => plugin.on('notAnEvent', () => {}),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        return true;
      }
    );
  });

  test('setKnobMode(KnobMode.Circular) returns false (fixture does not implement IEditController2)', () => {
    const ok = plugin.setKnobMode(KnobMode.Circular);
    assert.strictEqual(ok, false);
  });

  test('setAudioPresentationLatency(0, 256) returns false (fixture does not implement IAudioPresentationLatency)', () => {
    const ok = plugin.setAudioPresentationLatency(0, 256);
    assert.strictEqual(ok, false);
  });

  test('setChannelContextInfo({trackName:"Test"}) returns false (fixture does not implement IInfoListener)', () => {
    const ok = plugin.setChannelContextInfo({ trackName: 'Test' });
    assert.strictEqual(ok, false);
  });

  test('isPrefetchable() returns false (fixture does not implement IPrefetchableSupport)', () => {
    assert.strictEqual(plugin.isPrefetchable(), false);
  });
});
