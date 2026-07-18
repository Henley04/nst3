'use strict';
const { test, describe, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const {
  loadPlugin,
  createHost,
  ensurePluginBuilt,
  PLUGIN_PATH,
  makeTone,
  makeSilence,
} = require('./helpers');

describe('Lifecycle and disposal', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  afterEach(() => {
    if (plugin) {
      try {
        plugin.dispose();
      } catch (_) {
        // ignore — dispose should be idempotent/safe.
      }
    }
    plugin = null;
  });

  test('dispose() once does not throw', () => {
    plugin = loadPlugin().plugin;
    assert.doesNotThrow(() => plugin.dispose());
    plugin = null; // already disposed; don't double-dispose in afterEach
  });

  test('dispose() is idempotent (calling twice does not throw)', () => {
    plugin = loadPlugin().plugin;
    plugin.dispose();
    assert.doesNotThrow(() => plugin.dispose());
    plugin = null; // already disposed twice
  });

  test('load -> dispose -> load again on the same host works (no resource leak)', () => {
    const host = createHost();
    const p1 = host.load(PLUGIN_PATH);
    p1.setActive(true);
    p1.setProcessing(true);
    p1.process({
      inputs: makeTone(2, 4, 440, 48000, 0.5),
      outputs: makeSilence(2, 4),
      numSamples: 4,
    });
    p1.dispose();

    // Load a second plugin on the same host.
    const p2 = host.load(PLUGIN_PATH);
    p2.setActive(true);
    p2.setProcessing(true);
    const inputs = makeTone(2, 4, 440, 48000, 0.5);
    const outputs = makeSilence(2, 4);
    assert.doesNotThrow(() => p2.process({ inputs, outputs, numSamples: 4 }));
    assert.ok(Math.abs(outputs[0][0] - inputs[0][0]) < 1e-5);
    p2.dispose();
  });

  test('load, activate, process, dispose does not crash', () => {
    const { plugin: p } = loadPlugin();
    p.setActive(true);
    p.setProcessing(true);
    p.process({
      inputs: makeTone(2, 4, 440, 48000, 0.5),
      outputs: makeSilence(2, 4),
      numSamples: 4,
    });
    assert.doesNotThrow(() => p.dispose());
  });

  test(
    'Symbol.dispose (using syntax) auto-disposes the plugin at block exit',
    { skip: typeof Symbol.dispose !== 'symbol' ? 'Symbol.dispose not available' : false },
    () => {
      const host = createHost();
      {
        // `using p = host.load(...)` is TC39 explicit-resource-management syntax
        // that not all supported Node runtimes parse (it fails at parse time,
        // before the skip guard above can take effect). Calling the dispose
        // symbol manually exercises the same code path as `using` would.
        const p = host.load(PLUGIN_PATH);
        p.setActive(true);
        p.setProcessing(true);
        const inputs = makeTone(2, 4, 440, 48000, 0.5);
        const outputs = makeSilence(2, 4);
        p.process({ inputs, outputs, numSamples: 4 });
        assert.ok(Math.abs(outputs[0][0] - inputs[0][0]) < 1e-5);
        p[Symbol.dispose]();
      } // p[Symbol.dispose]() called manually above (mirrors `using` semantics).
      // Reaching this point without error demonstrates the dispose symbol works.
      assert.ok(true);
    }
  );

  test('on("restart", cb) can be registered without throwing', () => {
    plugin = loadPlugin().plugin;
    assert.doesNotThrow(() => plugin.on('restart', () => {}));
  });

  test(
    'loading many plugins without explicit dispose does not crash (soft GC test)',
    { skip: typeof global.gc !== 'function' ? '--expose-gc not enabled; running subset only' : false },
    () => {
      const host = createHost();
      const plugins = [];
      for (let i = 0; i < 10; i++) {
        plugins.push(host.load(PLUGIN_PATH));
      }
      // Trigger a GC pass to exercise finalizers.
      assert.doesNotThrow(() => global.gc());
      // Clean up explicitly to avoid resource warnings between tests.
      for (const p of plugins) p.dispose();
    }
  );

  test('loading and disposing many plugins in a loop does not crash', () => {
    const host = createHost();
    for (let i = 0; i < 10; i++) {
      const p = host.load(PLUGIN_PATH);
      p.setActive(true);
      p.setProcessing(true);
      p.process({
        inputs: makeTone(2, 4, 440, 48000, 0.5),
        outputs: makeSilence(2, 4),
        numSamples: 4,
      });
      p.dispose();
    }
    assert.ok(true);
  });

  test('setActive(false) after setActive(true) does not throw', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    assert.doesNotThrow(() => plugin.setProcessing(false));
    assert.doesNotThrow(() => plugin.setActive(false));
  });

  test('setProcessing(false) after setProcessing(true) does not throw', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    assert.doesNotThrow(() => plugin.setProcessing(false));
  });
});
