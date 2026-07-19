'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const {
  loadPlugin,
  ensurePluginBuilt,
  makeTone,
  makeSilence,
} = require('./helpers');

function approxEqual(a, b, eps = 1e-12) {
  return Math.abs(a - b) < eps;
}

// Helpers that produce Float64Array channel buffers (for the sampleSize=64
// path). Mirrors the helpers.js Float32 variants but uses Float64Array so
// the host's kSample64 buffer-resolution code path is exercised.
function makeTone64(numChannels, numSamples, freq, sampleRate, amplitude = 0.5) {
  const buffers = [];
  for (let c = 0; c < numChannels; c++) {
    const buf = new Float64Array(numSamples);
    for (let i = 0; i < numSamples; i++) {
      buf[i] = (amplitude * Math.sin((2 * Math.PI * freq * i) / sampleRate));
    }
    buffers.push(buf);
  }
  return buffers;
}

function makeSilence64(numChannels, numSamples) {
  const buffers = [];
  for (let i = 0; i < numChannels; i++) {
    buffers.push(new Float64Array(numSamples));
  }
  return buffers;
}

describe('64-bit audio processing (kSample64)', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  beforeEach(() => {
    plugin = loadPlugin({ sampleSize: 64 }).plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
  });
  afterEach(() => {
    if (plugin) plugin.dispose();
    plugin = null;
  });

  test('getSampleSize() returns 64 after load with sampleSize:64', () => {
    assert.strictEqual(plugin.getSampleSize(), 64);
  });

  test('canProcessSampleSize(64) returns true (GainProcessor fixture supports it)', () => {
    assert.strictEqual(plugin.canProcessSampleSize(64), true);
  });

  test('canProcessSampleSize(32) returns true (VST3 spec mandates 32-bit support)', () => {
    assert.strictEqual(plugin.canProcessSampleSize(32), true);
  });

  test('gain=1.0 passes Float64Array audio through (within epsilon 1e-12)', () => {
    const inputs = makeTone64(2, 256, 440, 48000, 0.5);
    const outputs = makeSilence64(2, 256);
    plugin.process({ inputs, outputs, numSamples: 256 });
    for (let c = 0; c < 2; c++) {
      for (let i = 0; i < 256; i++) {
        assert.ok(
          approxEqual(outputs[c][i], inputs[c][i], 1e-12),
          `ch=${c} i=${i}: out=${outputs[c][i]} in=${inputs[c][i]}`
        );
      }
    }
  });

  test('gain=0.5 halves Float64Array input', () => {
    plugin.setParameter(0, 0.5);
    const inputs = makeTone64(2, 256, 440, 48000, 0.5);
    const outputs = makeSilence64(2, 256);
    plugin.process({ inputs, outputs, numSamples: 256 });
    for (let c = 0; c < 2; c++) {
      for (let i = 0; i < 256; i++) {
        assert.ok(
          approxEqual(outputs[c][i], inputs[c][i] * 0.5, 1e-12),
          `ch=${c} i=${i}: out=${outputs[c][i]} expected=${inputs[c][i] * 0.5}`
        );
      }
    }
  });

  test('passing Float32Array buffers when sampleSize=64 throws VST3_INVALID_BUFFER', () => {
    // The host validates that each channel TypedArray matches the active
    // sample size; passing Float32Array when the host is in kSample64 mode
    // must surface a VST3_INVALID_BUFFER error to JS.
    assert.throws(
      () =>
        plugin.process({
          inputs: makeTone(2, 64, 440, 48000, 0.5),
          outputs: makeSilence(2, 64),
          numSamples: 64,
        }),
      (err) => err.code === 'VST3_INVALID_BUFFER'
    );
  });
});
