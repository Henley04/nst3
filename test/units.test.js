'use strict';
const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const nst3 = require('../');
const { MediaType, BusDirection } = nst3;
const { loadPlugin, ensurePluginBuilt, makeTone, makeSilence } = require('./helpers');

function approxEqual(a, b, eps = 1e-6) {
  return Math.abs(a - b) < eps;
}

describe('IUnitInfo — units and programs', { skip: !ensurePluginBuilt() }, () => {
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

  test('getUnitCount() returns 1', () => {
    assert.strictEqual(plugin.getUnitCount(), 1);
  });

  test('getUnitInfo(0) returns the Root unit', () => {
    const info = plugin.getUnitInfo(0);
    assert.strictEqual(info.id, 0);
    assert.strictEqual(info.name, 'Root');
    assert.strictEqual(info.programListId, 0);
    assert.strictEqual(info.parentUnitId, -1);
    assert.strictEqual(info.type, 0);
  });

  test('getUnitInfo(1) throws (out of range)', () => {
    assert.throws(
      () => plugin.getUnitInfo(1),
      (err) => {
        assert.ok(err.code, 'error should have a code property');
        return true;
      }
    );
  });

  test('getProgramListCount() returns 1', () => {
    assert.strictEqual(plugin.getProgramListCount(), 1);
  });

  test('getProgramListInfo(0) returns the Presets list with 2 programs', () => {
    const info = plugin.getProgramListInfo(0);
    assert.strictEqual(info.id, 0);
    assert.strictEqual(info.name, 'Presets');
    assert.strictEqual(info.programCount, 2);
  });

  test('getProgramName(0, 0) returns "Init"', () => {
    assert.strictEqual(plugin.getProgramName(0, 0), 'Init');
  });

  test('getProgramName(0, 1) returns "Bright"', () => {
    assert.strictEqual(plugin.getProgramName(0, 1), 'Bright');
  });

  test('getCurrentUnit() returns 0 (default)', () => {
    assert.strictEqual(plugin.getCurrentUnit(), 0);
  });

  test('selectProgram(0, 1) selects "Bright" and process() applies 0.7 gain', () => {
    plugin.selectProgram(0, 1);
    const inputs = makeTone(2, 128, 440, 48000, 0.5);
    const outputs = makeSilence(2, 128);
    plugin.process({ inputs, outputs, numSamples: 128 });
    for (let c = 0; c < 2; c++) {
      for (let i = 0; i < 128; i++) {
        assert.ok(
          approxEqual(outputs[c][i], inputs[c][i] * 0.7, 1e-6),
          `ch=${c} i=${i}: out=${outputs[c][i]} expected=${inputs[c][i] * 0.7}`
        );
      }
    }
  });

  test('selectProgram(0, 0) reverts to Init and follows the Gain parameter (default 1.0)', () => {
    plugin.selectProgram(0, 1); // Bright first
    plugin.selectProgram(0, 0); // Back to Init
    const inputs = makeTone(2, 64, 440, 48000, 0.5);
    const outputs = makeSilence(2, 64);
    plugin.process({ inputs, outputs, numSamples: 64 });
    // Default gain is 1.0 → output should match input.
    for (let c = 0; c < 2; c++) {
      for (let i = 0; i < 64; i++) {
        assert.ok(
          approxEqual(outputs[c][i], inputs[c][i], 1e-6),
          `ch=${c} i=${i}: out=${outputs[c][i]} expected=${inputs[c][i]}`
        );
      }
    }
  });

  test('getUnitByBusInfo(Audio, Input, 0) returns null (fixture returns kResultFalse)', () => {
    const result = plugin.getUnitByBusInfo({
      mediaType: MediaType.Audio,
      direction: BusDirection.Input,
      busIndex: 0,
    });
    assert.strictEqual(result, null);
  });

  // The GainProcessor fixture does NOT implement IProgramListData or IUnitData
  // (setUnitProgramData returns kResultFalse). The host's getProgramData /
  // setProgramData / getUnitData / setUnitData throw VST3_UNKNOWN in that case
  // (per index.d.ts JSDoc).

  test('getProgramData(0, 0) throws VST3_UNKNOWN (fixture does not implement IProgramListData)', () => {
    assert.throws(() => plugin.getProgramData(0, 0), /VST3_UNKNOWN/);
  });

  test('setProgramData(0, 0, buffer) throws VST3_UNKNOWN (fixture does not implement IProgramListData)', () => {
    const buf = Buffer.alloc(4);
    assert.throws(() => plugin.setProgramData(0, 0, buf), /VST3_UNKNOWN/);
  });

  test('getUnitData(0) throws VST3_UNKNOWN (fixture does not implement IUnitData)', () => {
    assert.throws(() => plugin.getUnitData(0), /VST3_UNKNOWN/);
  });

  test('setUnitData(0, buffer) throws VST3_UNKNOWN (fixture does not implement IUnitData)', () => {
    const buf = Buffer.alloc(4);
    assert.throws(() => plugin.setUnitData(0, buf), /VST3_UNKNOWN/);
  });
});
