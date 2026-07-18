'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const { MidiEventType } = require('../');
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence } = require('./helpers');

describe('MIDI event handling', { skip: !ensurePluginBuilt() }, () => {
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

  test('addMidiEvent accepts NoteOn', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({ type: MidiEventType.NoteOn, channel: 0, note: 60, velocity: 100 })
    );
  });

  test('addMidiEvent accepts NoteOff', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({ type: MidiEventType.NoteOff, channel: 0, note: 60, velocity: 0 })
    );
  });

  test('addMidiEvent accepts PolyPressure', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({ type: MidiEventType.PolyPressure, channel: 0, note: 60, pressure: 64 })
    );
  });

  test('addMidiEvent accepts Controller', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({
        type: MidiEventType.Controller,
        channel: 0,
        controllerNumber: 1,
        controllerValue: 64,
      })
    );
  });

  test('addMidiEvent accepts ProgramChange', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({ type: MidiEventType.ProgramChange, channel: 0, programNumber: 5 })
    );
  });

  test('addMidiEvent accepts ChannelPressure', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({ type: MidiEventType.ChannelPressure, channel: 0, pressure: 64 })
    );
  });

  test('addMidiEvent accepts PitchBend', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({ type: MidiEventType.PitchBend, channel: 0, pitchBend: 1000 })
    );
  });

  test('addMidiEvent accepts negative PitchBend', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({ type: MidiEventType.PitchBend, channel: 0, pitchBend: -8192 })
    );
  });

  test('addMidiEvent accepts SysEx (Uint8Array)', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiEvent({
        type: MidiEventType.SysEx,
        sysEx: Uint8Array.from([0xf0, 0x7e, 0xf7]),
      })
    );
  });

  test('processing a block with multiple queued events does not crash', () => {
    plugin.addMidiEvent({ type: MidiEventType.NoteOn, channel: 0, note: 60, velocity: 100 });
    plugin.addMidiEvent({ type: MidiEventType.NoteOff, channel: 0, note: 60, velocity: 0 });
    plugin.addMidiEvent({ type: MidiEventType.Controller, channel: 0, controllerNumber: 1, controllerValue: 64 });
    plugin.addMidiEvent({ type: MidiEventType.PitchBend, channel: 0, pitchBend: 1000 });
    plugin.addMidiEvent({
      type: MidiEventType.SysEx,
      sysEx: Uint8Array.from([0xf0, 0x7e, 0xf7]),
    });
    const inputs = makeTone(2, 64, 440, 48000, 0.5);
    const outputs = makeSilence(2, 64);
    assert.doesNotThrow(() => plugin.process({ inputs, outputs, numSamples: 64 }));
  });

  test('takeOutputEvents returns an array (likely empty for Gain)', () => {
    const events = plugin.takeOutputEvents();
    assert.ok(Array.isArray(events));
    // Gain plugin produces no MIDI output.
    assert.strictEqual(events.length, 0);
  });

  test('clearEvents does not throw', () => {
    plugin.addMidiEvent({ type: MidiEventType.NoteOn, channel: 0, note: 60, velocity: 100 });
    assert.doesNotThrow(() => plugin.clearEvents());
  });

  test('addMidiBytes accepts note-on raw bytes', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiBytes(0, Uint8Array.from([0x90, 60, 100]))
    );
  });

  test('addMidiBytes accepts note-off raw bytes', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiBytes(0, Uint8Array.from([0x80, 60, 0]))
    );
  });

  test('addMidiBytes accepts controller raw bytes', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiBytes(0, Uint8Array.from([0xb0, 7, 100]))
    );
  });

  test('addMidiBytes accepts pitch-bend raw bytes', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiBytes(0, Uint8Array.from([0xe0, 0, 64]))
    );
  });

  test('addMidiBytes accepts a SysEx raw byte sequence', () => {
    assert.doesNotThrow(() =>
      plugin.addMidiBytes(0, Uint8Array.from([0xf0, 0x7e, 0xf7]))
    );
  });

  test('addMidiBytes queued events survive through a process call', () => {
    plugin.addMidiBytes(0, Uint8Array.from([0x90, 60, 100]));
    const inputs = makeTone(2, 16, 440, 48000, 0.5);
    const outputs = makeSilence(2, 16);
    assert.doesNotThrow(() => plugin.process({ inputs, outputs, numSamples: 16 }));
  });
});
