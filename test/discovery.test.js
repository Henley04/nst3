'use strict';
const path = require('path');
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { Host } = require('../');
const { PLUGIN_PATH, ensurePluginBuilt } = require('./helpers');

const BUILD_DIR = path.join(__dirname, 'plugin', 'build');

describe('Plugin discovery', { skip: !ensurePluginBuilt() }, () => {
  test('Host.scanDirectory finds the Gain plugin with all expected fields', () => {
    const infos = Host.scanDirectory(BUILD_DIR);
    assert.ok(Array.isArray(infos));
    assert.ok(infos.length > 0, 'expected at least one plugin in scan results');

    const gain = infos.find((i) => i.name === 'Gain');
    assert.ok(gain, 'Gain plugin not found in scan results');

    // Verify all expected PluginInfo fields.
    assert.strictEqual(gain.path, PLUGIN_PATH);
    assert.strictEqual(gain.name, 'Gain');
    assert.strictEqual(typeof gain.vendor, 'string');
    assert.ok(gain.vendor.length > 0);
    assert.strictEqual(gain.version, '1.0.0');
    assert.strictEqual(gain.category, 'Audio Module Class');
    assert.strictEqual(gain.subCategories, 'Fx');
    assert.ok(typeof gain.sdkVersion === 'string' && gain.sdkVersion.length > 0);
    assert.ok(
      /^[0-9a-f]{32}$/.test(gain.classId),
      `classId should be 32-char lowercase hex, got: ${gain.classId}`
    );
    assert.strictEqual(typeof gain.cardinality, 'number');
    assert.ok(gain.cardinality > 0);

    // factoryInfo sub-object
    assert.ok(gain.factoryInfo && typeof gain.factoryInfo === 'object');
    assert.ok(typeof gain.factoryInfo.vendor === 'string');
    assert.ok(typeof gain.factoryInfo.url === 'string');
    assert.ok(typeof gain.factoryInfo.email === 'string');
  });

  test('Host.inspectPlugin returns a single PluginInfo object (not array)', () => {
    const info = Host.inspectPlugin(PLUGIN_PATH);
    assert.ok(!Array.isArray(info), 'expected a single object, not an array');
    assert.strictEqual(info.name, 'Gain');
    assert.strictEqual(info.version, '1.0.0');
    assert.strictEqual(info.subCategories, 'Fx');
    assert.ok(/^[0-9a-f]{32}$/.test(info.classId));
    assert.ok(info.factoryInfo && typeof info.factoryInfo.vendor === 'string');
  });

  test('Host.scanDirectory returns empty array for nonexistent path (no crash)', () => {
    const infos = Host.scanDirectory('/nonexistent/path/should/not/exist');
    assert.ok(Array.isArray(infos));
    assert.strictEqual(infos.length, 0);
  });

  test('Host.scanDefaultLocations returns an array (may be empty in CI)', () => {
    const infos = Host.scanDefaultLocations();
    assert.ok(Array.isArray(infos));
  });
});
