'use strict';
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { Host, version } = require('../');
const { loadPlugin, ensurePluginBuilt } = require('./helpers');

describe('Plugin load and info', { skip: !ensurePluginBuilt() }, () => {
  test('host.load succeeds and returns a PluginInstance', () => {
    const { plugin, host } = loadPlugin();
    assert.ok(plugin, 'plugin should be truthy');
    assert.ok(host, 'host should be truthy');
    // PluginInstance should expose the expected API surface.
    assert.strictEqual(typeof plugin.process, 'function');
    assert.strictEqual(typeof plugin.getInfo, 'function');
    assert.strictEqual(typeof plugin.getParameter, 'function');
    assert.strictEqual(typeof plugin.setParameter, 'function');
    assert.strictEqual(typeof plugin.dispose, 'function');
    plugin.dispose();
  });

  test('plugin.getInfo returns expected fields', () => {
    const { plugin } = loadPlugin();
    const info = plugin.getInfo();
    assert.strictEqual(info.name, 'Gain');
    assert.ok(typeof info.vendor === 'string' && info.vendor.length > 0);
    assert.strictEqual(info.version, '1.0.0');
    assert.strictEqual(info.category, 'Audio Module Class');
    assert.strictEqual(info.subCategories, 'Fx');
    assert.ok(typeof info.sdkVersion === 'string' && info.sdkVersion.length > 0);
    assert.ok(/^[0-9a-f]{32}$/.test(info.classId));
    assert.strictEqual(info.numAudioInputs, 2);
    assert.strictEqual(info.numAudioOutputs, 2);
    assert.strictEqual(info.numMidiInputs, 0);
    assert.strictEqual(info.numMidiOutputs, 0);
    assert.strictEqual(info.parameterCount, 1);
    assert.strictEqual(info.hasController, true);
    assert.strictEqual(info.isSingleComponent, true);
    plugin.dispose();
  });

  test('plugin.getLatency returns a non-negative number', () => {
    const { plugin } = loadPlugin();
    const latency = plugin.getLatency();
    assert.strictEqual(typeof latency, 'number');
    assert.ok(latency >= 0, `latency should be >= 0, got ${latency}`);
    // The Gain plugin reports no latency.
    assert.strictEqual(latency, 0);
    plugin.dispose();
  });

  test('host.getOptions returns the configured host options', () => {
    const host = new Host({ sampleRate: 48000, maxBlockSize: 512, audioInputs: 2, audioOutputs: 2 });
    const opts = host.getOptions();
    assert.strictEqual(opts.sampleRate, 48000);
    assert.strictEqual(opts.maxBlockSize, 512);
    assert.strictEqual(opts.audioInputs, 2);
    assert.strictEqual(opts.audioOutputs, 2);
  });

  test('version() returns object with native, vst3sdk, napi fields', () => {
    const v = version();
    assert.ok(v && typeof v === 'object');
    assert.ok(typeof v.native === 'string' && v.native.length > 0);
    assert.ok(typeof v.vst3sdk === 'string' && v.vst3sdk.length > 0);
    assert.ok(typeof v.napi === 'number');
    assert.ok(v.napi > 0);
  });
});
