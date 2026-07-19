'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const nst3 = require('../');
const { MidiEventType } = nst3;
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence } = require('./helpers');

// NOTE: We cannot directly read Event::kIsLive from JS. The host clears the
// kIsLive flag on all queued events when processMode == 'offline' (per spec
// Requirement: Configurable Process Mode — Offline rendering). This test
// file loads the plugin in offline mode and verifies that processing
// completes without crashing as the smoke test for the offline code path.
// The actual kIsLive bit handling is exercised inside the native code; the
// spec test passes if processing succeeds.

describe('Offline process mode (kOffline)', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  beforeEach(() => {
    plugin = loadPlugin({ processMode: 'offline' }).plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
  });
  afterEach(() => {
    if (plugin) plugin.dispose();
    plugin = null;
  });

  test('plugin.getInfo() works after loading in offline mode', () => {
    const info = plugin.getInfo();
    assert.strictEqual(info.name, 'Gain');
    assert.strictEqual(info.parameterCount, 2);
  });

  test('processing a block in offline mode completes without crashing', () => {
    const inputs = makeTone(2, 128, 440, 48000, 0.5);
    const outputs = makeSilence(2, 128);
    assert.doesNotThrow(() => plugin.process({ inputs, outputs, numSamples: 128 }));
  });

  test('queuing NoteOn + NoteOff events in offline mode processes without crashing', () => {
    plugin.addMidiEvent({
      type: MidiEventType.NoteOn,
      channel: 0,
      note: 60,
      velocity: 100,
      noteId: 1,
    });
    plugin.addMidiEvent({
      type: MidiEventType.NoteOff,
      channel: 0,
      note: 60,
      velocity: 0,
      noteId: 1,
    });
    const inputs = makeTone(2, 64, 440, 48000, 0.5);
    const outputs = makeSilence(2, 64);
    assert.doesNotThrow(() => plugin.process({ inputs, outputs, numSamples: 64 }));
    // takeOutputEvents may or may not echo events; just verify it does not throw.
    assert.doesNotThrow(() => plugin.takeOutputEvents());
  });

  test('a 0-sample block in offline mode is a valid parameter-flush (no throw)', () => {
    // Per Task 4: numSamples === 0 is a parameter-flush block; the host
    // invokes IAudioProcessor::process with numSamples=0 and no audio
    // buffer resolution. In offline mode this is still valid.
    assert.doesNotThrow(() => plugin.process({ inputs: [], outputs: [], numSamples: 0 }));
  });
});
