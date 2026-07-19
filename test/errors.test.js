'use strict';
const { test, describe, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const {
  loadPlugin,
  createHost,
  ensurePluginBuilt,
  makeTone,
  makeSilence,
} = require('./helpers');

describe('Error handling', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  afterEach(() => {
    if (plugin) {
      try {
        plugin.dispose();
      } catch (_) {
        // ignore
      }
    }
    plugin = null;
  });

  test('host.load on a nonexistent path throws VST3_LOAD_FAILED', () => {
    const host = createHost();
    assert.throws(
      () => host.load('/nonexistent/plugin.vst3'),
      (err) => {
        assert.strictEqual(err.code, 'VST3_LOAD_FAILED');
        assert.ok(err.message && err.message.length > 0, 'error message should be non-empty');
        return true;
      }
    );
  });

  test('process without setActive throws VST3_NOT_ACTIVE', () => {
    plugin = loadPlugin().plugin;
    const inputs = makeTone(2, 4, 440, 48000, 0.5);
    const outputs = makeSilence(2, 4);
    assert.throws(
      () => plugin.process({ inputs, outputs, numSamples: 4 }),
      (err) => {
        assert.strictEqual(err.code, 'VST3_NOT_ACTIVE');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('process after setActive but before setProcessing throws VST3_NOT_PROCESSING', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    const inputs = makeTone(2, 4, 440, 48000, 0.5);
    const outputs = makeSilence(2, 4);
    assert.throws(
      () => plugin.process({ inputs, outputs, numSamples: 4 }),
      (err) => {
        assert.strictEqual(err.code, 'VST3_NOT_PROCESSING');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('process with numSamples=0 is a valid parameter-flush block (no throw)', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    assert.doesNotThrow(() =>
      plugin.process({
        inputs: makeSilence(2, 4),
        outputs: makeSilence(2, 4),
        numSamples: 0,
      })
    );
  });

  test('process with numSamples larger than maxBlockSize throws VST3_INVALID_BUFFER', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    assert.throws(
      () =>
        plugin.process({
          inputs: makeSilence(2, 1024),
          outputs: makeSilence(2, 1024),
          numSamples: 1024, // > default maxBlockSize (512)
        }),
      (err) => {
        assert.strictEqual(err.code, 'VST3_INVALID_BUFFER');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('process with mismatched (too short) input buffers throws VST3_INVALID_BUFFER', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    assert.throws(
      () =>
        plugin.process({
          inputs: [new Float32Array(2), new Float32Array(2)],
          outputs: [new Float32Array(4), new Float32Array(4)],
          numSamples: 4,
        }),
      (err) => {
        assert.strictEqual(err.code, 'VST3_INVALID_BUFFER');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('setParameter with a non-number value throws an error with a code', () => {
    plugin = loadPlugin().plugin;
    assert.throws(
      () => plugin.setParameter(0, 'not a number'),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        assert.strictEqual(typeof err.code, 'string');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('setParameter with a string id throws an error with a code', () => {
    plugin = loadPlugin().plugin;
    assert.throws(
      () => plugin.setParameter('bad-id', 0.5),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        assert.strictEqual(typeof err.code, 'string');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('setParameters with a non-array argument throws an error', () => {
    plugin = loadPlugin().plugin;
    assert.throws(
      () => plugin.setParameters('not an array'),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        assert.strictEqual(typeof err.code, 'string');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('loadState with a non-Buffer argument throws an error', () => {
    plugin = loadPlugin().plugin;
    assert.throws(
      () => plugin.loadState('not a buffer'),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        assert.strictEqual(typeof err.code, 'string');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('on() with an unknown event name throws an error', () => {
    plugin = loadPlugin().plugin;
    assert.throws(
      () => plugin.on('unknown-event', () => {}),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        assert.strictEqual(typeof err.code, 'string');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });

  test('on() with a non-function callback throws an error', () => {
    plugin = loadPlugin().plugin;
    assert.throws(
      () => plugin.on('restart', 'not a function'),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        assert.strictEqual(typeof err.code, 'string');
        assert.ok(err.message && err.message.length > 0);
        return true;
      }
    );
  });
});
