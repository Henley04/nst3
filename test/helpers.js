'use strict';
const path = require('path');
const { Host } = require('../');

const PLUGIN_PATH = path.join(__dirname, 'plugin', 'build', 'Gain.vst3');

function ensurePluginBuilt() {
  // Check if plugin exists; if not, skip tests with helpful message
  const fs = require('fs');
  if (!fs.existsSync(PLUGIN_PATH)) {
    console.error(`Test plugin not found at ${PLUGIN_PATH}. Run: npm run test:plugin`);
    return false;
  }
  return true;
}

function createHost(opts = {}) {
  return new Host({
    sampleRate: 48000,
    maxBlockSize: 512,
    audioInputs: 2,
    audioOutputs: 2,
    ...opts,
  });
}

function loadPlugin(hostOpts) {
  const host = createHost(hostOpts);
  const plugin = host.load(PLUGIN_PATH);
  return { host, plugin };
}

function makeSilence(numChannels, numSamples) {
  const buffers = [];
  for (let i = 0; i < numChannels; i++) {
    buffers.push(new Float32Array(numSamples));
  }
  return buffers;
}

function makeTone(numChannels, numSamples, freq, sampleRate, amplitude = 0.5) {
  const buffers = [];
  for (let c = 0; c < numChannels; c++) {
    const buf = new Float32Array(numSamples);
    for (let i = 0; i < numSamples; i++) {
      buf[i] = amplitude * Math.sin(2 * Math.PI * freq * i / sampleRate);
    }
    buffers.push(buf);
  }
  return buffers;
}

function maxAbs(buffers) {
  let max = 0;
  for (const buf of buffers) {
    for (let i = 0; i < buf.length; i++) {
      const a = Math.abs(buf[i]);
      if (a > max) max = a;
    }
  }
  return max;
}

module.exports = {
  PLUGIN_PATH,
  ensurePluginBuilt,
  createHost,
  loadPlugin,
  makeSilence,
  makeTone,
  maxAbs,
};
