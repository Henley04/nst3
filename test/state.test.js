'use strict';
const { test, describe, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence, maxAbs } = require('./helpers');

function approxEqual(a, b, eps = 1e-5) {
  return Math.abs(a - b) < eps;
}

// Helper: drive a tiny process block so the Gain plugin flushes queued
// parameter changes into its internal currentGain_ state. Without this,
// saveState() serializes the *constructor default* gain (1.0) rather than
// the controller's normalized value, because the plugin's `process()`
// is what copies the queued value into currentGain_.
function flushParameter(plugin, value) {
  plugin.setParameter(0, value);
  const inputs = makeTone(2, 4, 440, 48000, 0.5);
  const outputs = makeSilence(2, 4);
  plugin.process({ inputs, outputs, numSamples: 4 });
}

describe('State save/load', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  afterEach(() => {
    if (plugin) plugin.dispose();
    plugin = null;
  });

  test('saveState returns a non-empty Buffer', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    flushParameter(plugin, 0.7);
    const state = plugin.saveState();
    assert.ok(Buffer.isBuffer(state), `expected Buffer, got ${state && typeof state}`);
    assert.ok(state.length > 0, 'state buffer should be non-empty');
  });

  test('saveState serializes a 4-byte float for the Gain plugin', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    flushParameter(plugin, 0.7);
    const state = plugin.saveState();
    assert.ok(state.length >= 4);
    // The state is wrapped in a versioned envelope:
    //   bytes 0-3: 'NST3' magic, byte 4: version, bytes 5-8: compLen (uint32 LE),
    //   bytes 9..9+compLen: component-state bytes.
    // The Gain plugin's component state is a single 4-byte float (gain).
    assert.strictEqual(state.slice(0, 4).toString('ascii'), 'NST3');
    const compLen = state.readUInt32LE(5);
    assert.strictEqual(compLen, 4);
    const gain = state.readFloatLE(9);
    assert.ok(
      approxEqual(gain, 0.7, 1e-4),
      `expected ~0.7, got ${gain}`
    );
  });

  test('loadState restores the parameter value after mutation', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    flushParameter(plugin, 0.7);
    const state = plugin.saveState();

    // Mutate.
    plugin.setParameter(0, 0.1);
    assert.ok(approxEqual(plugin.getParameter(0), 0.1));

    // Restore.
    plugin.loadState(state);
    const restored = plugin.getParameter(0);
    assert.ok(
      approxEqual(restored, 0.7, 1e-4),
      `expected ~0.7 after loadState, got ${restored}`
    );
  });

  test('loadState into a FRESH plugin instance restores the parameter', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    flushParameter(plugin, 0.7);
    const state = plugin.saveState();

    // Tear down and create a brand new instance.
    plugin.dispose();
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);

    // Default before loadState should be 1.0.
    assert.ok(approxEqual(plugin.getParameter(0), 1.0));

    plugin.loadState(state);
    const restored = plugin.getParameter(0);
    assert.ok(
      approxEqual(restored, 0.7, 1e-4),
      `fresh instance should have ~0.7 after loadState, got ${restored}`
    );
  });

  test('processing still works after loadState and applies the restored gain', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    flushParameter(plugin, 0.7);
    const state = plugin.saveState();

    // Fresh instance, load state, then process a tone.
    plugin.dispose();
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    plugin.loadState(state);

    const inputs = makeTone(2, 64, 440, 48000, 0.5);
    const outputs = makeSilence(2, 64);
    plugin.process({ inputs, outputs, numSamples: 64 });

    const expected = maxAbs(inputs) * 0.7;
    assert.ok(
      approxEqual(maxAbs(outputs), expected, 1e-5),
      `expected output amplitude ~${expected}, got ${maxAbs(outputs)}`
    );
  });

  test('saveState after loadState round-trips the same bytes', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    flushParameter(plugin, 0.7);
    const state1 = plugin.saveState();

    // Mutate and then re-load the saved state.
    flushParameter(plugin, 0.3);
    plugin.loadState(state1);

    const state2 = plugin.saveState();
    assert.ok(
      approxEqual(state2.readFloatLE(9), state1.readFloatLE(9), 1e-6),
      'round-trip state should match'
    );
  });
});
