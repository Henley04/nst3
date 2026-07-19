'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const nst3 = require('../');
const { RestartFlags } = nst3;
const { loadPlugin, ensurePluginBuilt } = require('./helpers');

// The GainProcessor fixture does NOT trigger `IComponentHandler::restartComponent`
// itself, so we cannot test the auto-react path automatically with this fixture.
// The tests below cover the manual `applyRestartFlags` API and event
// subscription. A future fixture that calls
// `IComponentHandler::restartComponent(kLatencyChanged)` after changing its
// reported latency would exercise the auto-react path; the current
// GainProcessor does not do this.

describe('Restart auto-react', { skip: !ensurePluginBuilt() }, () => {
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

  test('applyRestartFlags(0) does not throw', () => {
    assert.doesNotThrow(() => plugin.applyRestartFlags(0));
  });

  test('applyRestartFlags(RestartFlags.IoChanged) does not throw (re-reads bus info)', () => {
    assert.doesNotThrow(() => plugin.applyRestartFlags(RestartFlags.IoChanged));
  });

  test('applyRestartFlags(RestartFlags.LatencyChanged) does not throw', () => {
    assert.doesNotThrow(() => plugin.applyRestartFlags(RestartFlags.LatencyChanged));
  });

  test('on("restart", cb) registration does not throw', () => {
    assert.doesNotThrow(() => plugin.on('restart', (flags) => {}));
  });

  test.skip('fixture does not emit restartComponent automatically (Task 30.5 deferred)', () => {
    // A future fixture that calls
    // IComponentHandler::restartComponent(kLatencyChanged) after changing
    // its reported latency would exercise the auto-react path (the host
    // re-queries IAudioProcessor::getLatencySamples() and emits the JS
    // 'restart' event with the kLatencyChanged flag). The current
    // GainProcessor fixture does not do this, so we cannot verify the
    // auto-react flow end-to-end here.
  });
});
