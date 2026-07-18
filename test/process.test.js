'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence, maxAbs } = require('./helpers');

function approxEqual(a, b, eps = 1e-5) {
  return Math.abs(a - b) < eps;
}

describe('Audio processing', { skip: !ensurePluginBuilt() }, () => {
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

  test('default gain=1.0 passes audio through (within float epsilon)', () => {
    const inputs = makeTone(2, 256, 440, 48000, 0.5);
    const outputs = makeSilence(2, 256);
    plugin.process({ inputs, outputs, numSamples: 256 });
    for (let c = 0; c < 2; c++) {
      for (let i = 0; i < 256; i++) {
        assert.ok(
          approxEqual(outputs[c][i], inputs[c][i], 1e-6),
          `ch=${c} i=${i}: out=${outputs[c][i]} in=${inputs[c][i]}`
        );
      }
    }
  });

  test('gain=0.0 produces silence', () => {
    plugin.setParameter(0, 0.0);
    const inputs = makeTone(2, 256, 440, 48000, 0.5);
    const outputs = makeSilence(2, 256);
    plugin.process({ inputs, outputs, numSamples: 256 });
    assert.strictEqual(maxAbs(outputs), 0);
  });

  test('gain=0.5 halves the input amplitude', () => {
    plugin.setParameter(0, 0.5);
    const inputs = makeTone(2, 256, 440, 48000, 0.5);
    const outputs = makeSilence(2, 256);
    plugin.process({ inputs, outputs, numSamples: 256 });
    for (let c = 0; c < 2; c++) {
      for (let i = 0; i < 256; i++) {
        assert.ok(
          approxEqual(outputs[c][i], inputs[c][i] * 0.5, 1e-6),
          `ch=${c} i=${i}: out=${outputs[c][i]} expected=${inputs[c][i] * 0.5}`
        );
      }
    }
    // Output amplitude is half the input amplitude (whatever that happens to be
    // within the block — the sine wave may not hit its peak in 256 samples).
    assert.ok(approxEqual(maxAbs(outputs), maxAbs(inputs) * 0.5, 1e-6));
  });

  test('silence input produces silence output', () => {
    const inputs = makeSilence(2, 256);
    const outputs = makeSilence(2, 256);
    plugin.process({ inputs, outputs, numSamples: 256 });
    assert.strictEqual(maxAbs(outputs), 0);
  });

  test('numSamples=1 (minimum block) processes without crashing', () => {
    const inputs = makeTone(2, 1, 440, 48000, 0.5);
    const outputs = makeSilence(2, 1);
    plugin.process({ inputs, outputs, numSamples: 1 });
    assert.ok(approxEqual(outputs[0][0], inputs[0][0], 1e-6));
  });

  test('numSamples=512 (max block) processes without crashing', () => {
    const inputs = makeTone(2, 512, 440, 48000, 0.5);
    const outputs = makeSilence(2, 512);
    plugin.process({ inputs, outputs, numSamples: 512 });
    assert.ok(maxAbs(outputs) > 0);
  });

  test('output Float32Array is the same reference (zero-copy) and gets written', () => {
    const inputs = makeTone(2, 64, 440, 48000, 0.5);
    const outputs = makeSilence(2, 64);
    const originalOutRef = outputs[0];
    plugin.process({ inputs, outputs, numSamples: 64 });
    // Same reference (host writes into the supplied buffers in place).
    assert.strictEqual(outputs[0], originalOutRef);
    // Data was actually written.
    assert.ok(maxAbs(outputs) > 0, 'output buffer should have non-zero samples');
  });
});
