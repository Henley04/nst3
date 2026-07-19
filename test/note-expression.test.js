'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const nst3 = require('../');
const { NoteExpressionTypeIds, MidiEventType } = nst3;
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence } = require('./helpers');

describe('INoteExpressionController', { skip: !ensurePluginBuilt() }, () => {
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

  test('getNoteExpressionCount(0, 0) returns 1', () => {
    assert.strictEqual(plugin.getNoteExpressionCount(0, 0), 1);
  });

  test('getNoteExpressionInfo(0, 0, 0) returns the Volume expression', () => {
    const info = plugin.getNoteExpressionInfo(0, 0, 0);
    assert.strictEqual(info.typeId, NoteExpressionTypeIds.Volume);
    assert.strictEqual(info.title, 'Volume');
    assert.strictEqual(info.shortTitle, 'Vol');
    assert.strictEqual(info.unitId, 0);
    assert.strictEqual(info.associatedParameterId, -1);
    assert.strictEqual(info.flags, 0);
  });

  test('getNoteExpressionInfo(0, 0, 1) throws (out of range)', () => {
    assert.throws(
      () => plugin.getNoteExpressionInfo(0, 0, 1),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        return true;
      }
    );
  });

  test('addNoteExpressionEvent({noteId:42, typeId:Volume, value:0.8}) does not throw', () => {
    assert.doesNotThrow(() =>
      plugin.addNoteExpressionEvent({
        noteId: 42,
        typeId: NoteExpressionTypeIds.Volume,
        value: 0.8,
        sampleOffset: 0,
      })
    );
  });

  test('queuing NoteOn + noteExpression + NoteOff and processing does not crash', () => {
    plugin.addMidiEvent({
      type: MidiEventType.NoteOn,
      channel: 0,
      note: 60,
      velocity: 100,
      noteId: 42,
    });
    plugin.addNoteExpressionEvent({
      noteId: 42,
      typeId: NoteExpressionTypeIds.Volume,
      value: 0.8,
      sampleOffset: 0,
    });
    plugin.addMidiEvent({
      type: MidiEventType.NoteOff,
      channel: 0,
      note: 60,
      velocity: 0,
      noteId: 42,
    });
    const inputs = makeTone(2, 64, 440, 48000, 0.5);
    const outputs = makeSilence(2, 64);
    assert.doesNotThrow(() => plugin.process({ inputs, outputs, numSamples: 64 }));
    // takeOutputEvents is accessible (Gain produces no output events, but
    // the call must not crash).
    assert.doesNotThrow(() => plugin.takeOutputEvents());
  });

  test('passing an invalid typeId (e.g. 999) does not throw (host does not validate type IDs)', () => {
    // The host does not validate note-expression type IDs — it queues the
    // event as-is and the plugin is responsible for ignoring unknown types.
    assert.doesNotThrow(() =>
      plugin.addNoteExpressionEvent({
        noteId: 42,
        typeId: 999,
        value: 0.5,
        sampleOffset: 0,
      })
    );
  });
});
