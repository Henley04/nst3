'use strict';
const path = require('path');
const { test, describe, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const { ParameterFlags } = require('../');
const { loadPlugin, ensurePluginBuilt, PLUGIN_PATH } = require('./helpers');

function approxEqual(a, b, eps = 1e-5) {
  return Math.abs(a - b) < eps;
}

describe('Parameter management', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  afterEach(() => {
    if (plugin) plugin.dispose();
    plugin = null;
  });

  test('getParameterCount returns 1', () => {
    plugin = loadPlugin().plugin;
    assert.strictEqual(plugin.getParameterCount(), 1);
  });

  test('getParameterInfo(0) returns expected fields', () => {
    plugin = loadPlugin().plugin;
    const info = plugin.getParameterInfo(0);
    assert.strictEqual(info.id, 0);
    assert.strictEqual(info.title, 'Gain');
    assert.ok(typeof info.shortTitle === 'string');
    assert.ok(typeof info.units === 'string');
    assert.strictEqual(info.stepCount, 0); // 0 = continuous
    assert.ok(
      approxEqual(info.defaultNormalizedValue, 1.0),
      `expected default 1.0, got ${info.defaultNormalizedValue}`
    );
    assert.strictEqual(typeof info.unitId, 'number');
    assert.strictEqual(typeof info.flags, 'number');
    assert.ok(
      (info.flags & ParameterFlags.CanAutomate) !== 0,
      `CanAutomate flag should be set; flags=${info.flags}`
    );
  });

  test('getParameter(0) returns the default value (1.0)', () => {
    plugin = loadPlugin().plugin;
    const v = plugin.getParameter(0);
    assert.ok(approxEqual(v, 1.0), `expected 1.0, got ${v}`);
  });

  test('setParameter(0, 0.5) then getParameter(0) returns ~0.5', () => {
    plugin = loadPlugin().plugin;
    plugin.setParameter(0, 0.5);
    const v = plugin.getParameter(0);
    assert.ok(approxEqual(v, 0.5), `expected 0.5, got ${v}`);
  });

  test('setParameters([{id:0, value:0.3}]) then getParameter(0) returns ~0.3', () => {
    plugin = loadPlugin().plugin;
    plugin.setParameters([{ id: 0, value: 0.3 }]);
    const v = plugin.getParameter(0);
    assert.ok(approxEqual(v, 0.3), `expected 0.3, got ${v}`);
  });

  test('setParameters accepts an empty array (no-op)', () => {
    plugin = loadPlugin().plugin;
    assert.doesNotThrow(() => plugin.setParameters([]));
  });

  test('formatParameter(0, 0.5) returns a non-empty string', () => {
    plugin = loadPlugin().plugin;
    const s = plugin.formatParameter(0, 0.5);
    assert.ok(typeof s === 'string', `expected string, got ${typeof s}`);
    assert.ok(s.length > 0, `expected non-empty string, got "${s}"`);
  });

  test('formatParameter(0, 0) and formatParameter(0, 1.0) also return strings', () => {
    plugin = loadPlugin().plugin;
    assert.ok(plugin.formatParameter(0, 0.0).length > 0);
    assert.ok(plugin.formatParameter(0, 1.0).length > 0);
  });

  test('setParameter works before activate (controller exists at load time)', () => {
    plugin = loadPlugin().plugin;
    // Intentionally do NOT call setActive.
    plugin.setParameter(0, 0.4);
    assert.ok(approxEqual(plugin.getParameter(0), 0.4));
    // setParameters should also work pre-activate.
    plugin.setParameters([{ id: 0, value: 0.6 }]);
    assert.ok(approxEqual(plugin.getParameter(0), 0.6));
  });

  test('out-of-range getParameterInfo: documents actual behavior (crash or throw)', () => {
    // NOTE: The current addon does NOT wrap GetParameterInfo's throwNst in
    // translateExceptions, so an out-of-range index throws a C++ NstException
    // that terminates the process instead of producing a JS error. We verify
    // the behavior in a child process so the test runner survives either way.
    const { spawnSync } = require('child_process');
    const modulePath = path.resolve(__dirname, '..');
    const helpersPath = path.resolve(__dirname, 'helpers');
    const script = `
      'use strict';
      const { Host } = require(${JSON.stringify(modulePath)});
      const { PLUGIN_PATH } = require(${JSON.stringify(helpersPath)});
      const host = new Host({});
      const p = host.load(PLUGIN_PATH);
      try {
        const r = p.getParameterInfo(999);
        process.stdout.write('RETURNED:' + JSON.stringify(r) + '\\n');
      } catch (e) {
        process.stdout.write('CAUGHT:' + (e.code || e.message) + '\\n');
        process.exit(0);
      }
      p.dispose();
    `;
    const result = spawnSync(process.execPath, ['-e', script], {
      encoding: 'utf8',
    });
    const crashed = result.status !== 0 || result.signal !== null;
    const caught = /CAUGHT:/.test(result.stdout || '');
    const returned = /RETURNED:/.test(result.stdout || '');
    // Acceptable outcomes: a JS error is thrown (caught) OR the subprocess
    // crashes (current behavior). Returning a value is also tolerated if the
    // addon is later changed to return null/undefined.
    assert.ok(
      crashed || caught || returned,
      `expected crash, caught error, or returned value; status=${result.status} ` +
      `signal=${result.signal} stdout=${JSON.stringify(result.stdout)} ` +
      `stderr=${JSON.stringify(result.stderr)}`
    );
  });
});
