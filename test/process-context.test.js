'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence } = require('./helpers');

// Reference: VST3 ProcessContext state bits (per ivstprocesscontext.h):
//   kPlaying                = 1 << 1
//   kCycleActive            = 1 << 2
//   kRecording              = 1 << 3
//   kSystemTimeValid        = 1 << 8
//   kTempoValid             = 1 << 9
//   kBarPositionValid       = 1 << 10
//   kTimeSigValid           = 1 << 11
//   kProjectTimeMusicValid  = 1 << 12
//   kContinousTimeValid     = 1 << 14   (SDK spells "continous" — one 'u')
//
// projectTimeMusic formula (per VST3 SDK):
//   projectTimeMusic = projectTimeSamples / sampleRate * 2 * tempo / 60
//
// barPositionMusic advances by `quartersPerBar = (numerator * 4) / denominator`
// quarter notes per bar (VST3 convention). For 6/8: 6 * 4 / 8 = 3.0 quarter
// notes per bar (one bar of 6/8 = 3 quarter notes).

describe('Configurable ProcessContext', { skip: !ensurePluginBuilt() }, () => {
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

  test('getProcessContextRequirements() returns a non-negative number (fixture does not implement it → 0)', () => {
    const req = plugin.getProcessContextRequirements();
    assert.strictEqual(typeof req, 'number');
    assert.ok(req >= 0, `expected >= 0, got ${req}`);
  });

  test('setProcessContext({tempo:140, timeSig:4/4, playing:true}) does not throw', () => {
    assert.doesNotThrow(() =>
      plugin.setProcessContext({
        tempo: 140,
        timeSigNumerator: 4,
        timeSigDenominator: 4,
        playing: true,
      })
    );
  });

  test('getProcessContext() reflects the set tempo / time signature / playing state', () => {
    plugin.setProcessContext({
      tempo: 140,
      timeSigNumerator: 4,
      timeSigDenominator: 4,
      playing: true,
    });
    const ctx = plugin.getProcessContext();
    assert.strictEqual(ctx.tempo, 140);
    assert.strictEqual(ctx.timeSigNumerator, 4);
    assert.strictEqual(ctx.timeSigDenominator, 4);
    assert.strictEqual(ctx.playing, true);
    // state should be non-zero — kTempoValid | kTimeSigValid | kPlaying bits
    // are set (kPlaying=1<<1, kTempoValid=1<<9, kTimeSigValid=1<<11). We do
    // NOT hardcode the bit values here; just assert state is non-zero.
    assert.ok(ctx.state > 0, `expected non-zero state, got ${ctx.state}`);
  });

  test('processing 4 blocks of 128 samples at 48kHz after setProcessContext does not crash and returns reasonable values', () => {
    // SDK formula: projectTimeMusic = projectTimeSamples / sampleRate * 2 * tempo / 60.
    // We don't assert exact math here (samplePosition advance is host-tracked
    // and depends on internal logic); we just verify getProcessContext()
    // returns a sensible snapshot after a few blocks have been processed.
    plugin.setProcessContext({ tempo: 140, playing: true });
    for (let i = 0; i < 4; ++i) {
      const inputs = makeTone(2, 128, 440, 48000, 0.5);
      const outputs = makeSilence(2, 128);
      plugin.process({ inputs, outputs, numSamples: 128 });
    }
    const ctx = plugin.getProcessContext();
    assert.strictEqual(typeof ctx, 'object');
    assert.strictEqual(ctx.tempo, 140);
    assert.strictEqual(ctx.playing, true);
  });

  test('setProcessContext({playing:false}) freezes projectTimeSamples across subsequent process() blocks', () => {
    plugin.setProcessContext({ tempo: 140, playing: true });
    // Process one block to advance the transport.
    let inputs = makeTone(2, 128, 440, 48000, 0.5);
    let outputs = makeSilence(2, 128);
    plugin.process({ inputs, outputs, numSamples: 128 });
    const before = plugin.getProcessContext();
    // Stop transport — kPlaying bit is cleared.
    plugin.setProcessContext({ playing: false });
    // Process 2 more blocks; projectTimeSamples should NOT advance.
    for (let i = 0; i < 2; ++i) {
      inputs = makeTone(2, 128, 440, 48000, 0.5);
      outputs = makeSilence(2, 128);
      plugin.process({ inputs, outputs, numSamples: 128 });
    }
    const after = plugin.getProcessContext();
    assert.strictEqual(
      after.samplePosition,
      before.samplePosition,
      `projectTimeSamples should be frozen when playing=false; ` +
        `before=${before.samplePosition} after=${after.samplePosition}`
    );
  });

  test('compound meter (6/8) processes without crashing', () => {
    // barPositionMusic advances by (4 * 6 / 8) = 3.0 quarters per bar.
    assert.doesNotThrow(() =>
      plugin.setProcessContext({
        tempo: 140,
        timeSigNumerator: 6,
        timeSigDenominator: 8,
        playing: true,
      })
    );
    const inputs = makeTone(2, 128, 440, 48000, 0.5);
    const outputs = makeSilence(2, 128);
    assert.doesNotThrow(() => plugin.process({ inputs, outputs, numSamples: 128 }));
    const ctx = plugin.getProcessContext();
    assert.strictEqual(ctx.timeSigNumerator, 6);
    assert.strictEqual(ctx.timeSigDenominator, 8);
  });
});
