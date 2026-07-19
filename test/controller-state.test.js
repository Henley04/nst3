'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence } = require('./helpers');

function approxEqual(a, b, eps = 1e-6) {
  return Math.abs(a - b) < eps;
}

// Helper: drive a tiny process block so the Gain plugin flushes queued
// parameter changes into its internal currentGain_ state. Without this,
// saveState() serializes the constructor default gain (1.0) rather than
// the controller's normalized value, because the plugin's `process()` is
// what copies the queued value into currentGain_.
function flushParameter(plugin, value) {
  plugin.setParameter(0, value);
  const inputs = makeTone(2, 4, 440, 48000, 0.5);
  const outputs = makeSilence(2, 4);
  plugin.process({ inputs, outputs, numSamples: 4 });
}

// Versioned state envelope layout (Task 21):
//   bytes 0-3:     magic 'NST3'
//   byte  4:       version (1)
//   bytes 5-8:     component-state length (uint32 LE)
//   bytes 9..9+L:  component-state bytes
//   next 4:        controller-state length (uint32 LE)
//   next L2 bytes: controller-state bytes (may be empty)
//
// GainProcessor is a SingleComponentEffect: its controller == component, so
// IEditController::getState may return the same blob OR an empty blob (or
// kResultFalse → empty). The envelope still round-trips either way. The
// split-controller fixture (Task 30.6) is deferred; the versioned envelope
// is fully exercised on the single-component fixture because the host always
// composes the envelope regardless of controller==component.

describe('Controller state persistence (versioned envelope)', { skip: !ensurePluginBuilt() }, () => {
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

  test('saveState() returns a Buffer with the NST3 envelope', () => {
    flushParameter(plugin, 0.7);
    const buf = plugin.saveState();
    assert.ok(Buffer.isBuffer(buf));
    // Minimum envelope: 4 (magic) + 1 (version) + 4 (compLen) + 0+ comp +
    // 4 (ctrlLen) + 0+ ctrl = 13 bytes.
    assert.ok(buf.length >= 13, `expected >= 13 bytes, got ${buf.length}`);
    assert.strictEqual(buf.slice(0, 4).toString('ascii'), 'NST3');
    assert.strictEqual(buf[4], 1);
    const compLen = buf.readUInt32LE(5);
    // The fixture's IComponent::getState writes a 4-byte float (gain).
    assert.strictEqual(compLen, 4);
    // Controller length sits at offset 9 + compLen.
    const ctrlLenOffset = 9 + compLen;
    assert.ok(ctrlLenOffset + 4 <= buf.length, 'controller length prefix should fit');
    const ctrlLen = buf.readUInt32LE(ctrlLenOffset);
    assert.ok(ctrlLen >= 0, `expected ctrlLen >= 0, got ${ctrlLen}`);
  });

  test('saveState → loadState round-trips a mutated parameter', () => {
    flushParameter(plugin, 0.3);
    const buf = plugin.saveState();
    // Mutate the parameter to something else.
    flushParameter(plugin, 1.0);
    assert.ok(approxEqual(plugin.getParameter(0), 1.0));
    // Restore via the versioned envelope.
    plugin.loadState(buf);
    assert.ok(
      approxEqual(plugin.getParameter(0), 0.3, 1e-5),
      `expected ~0.3 after loadState, got ${plugin.getParameter(0)}`
    );
  });

  test('loadState accepts a legacy single-blob Buffer (backward compat)', () => {
    // Build a legacy 4-byte float buffer of 0.5 (no NST3 magic). The host
    // detects the absence of the magic and treats the entire buffer as
    // legacy component state (existing behavior preserved per Task 21).
    const legacy = Buffer.alloc(4);
    legacy.writeFloatLE(0.5, 0);
    assert.doesNotThrow(() => plugin.loadState(legacy));
    assert.ok(
      approxEqual(plugin.getParameter(0), 0.5, 1e-5),
      `expected ~0.5 after legacy loadState, got ${plugin.getParameter(0)}`
    );
  });

  test('loadState with a corrupt buffer does not crash (may throw or no-op)', () => {
    // The buffer 'garbage' has neither the NST3 magic nor valid component
    // bytes; the host treats it as legacy component state and the plugin's
    // setState may fail. Either way, the host must not segfault — throwing
    // a JS error or no-oping are both acceptable.
    assert.doesNotThrow(() => {
      try {
        plugin.loadState(Buffer.from('garbage'));
      } catch (_) {
        // Throwing a JS error is acceptable; a crash is not.
      }
    });
  });
});
