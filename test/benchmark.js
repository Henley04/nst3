'use strict';
// nst3 — process() throughput benchmark (Task 19.3)
//
// Loads the mock Gain plugin, activates it, processes a 60-second stereo
// stream at 48 kHz / 512 samples per block, and reports the realtime
// multiplier (lower is better; <1x means faster than realtime).
//
// Buffers are pre-allocated once and reused across all blocks — the steady-
// state loop must NOT allocate.
//
// Run from the repo root:  node test/benchmark.js
// Or from anywhere (path is resolved via __dirname).

const path = require('path');
const { Host } = require('../');

const PLUGIN_PATH = path.join(__dirname, 'plugin', 'build', 'Gain.vst3');

const SAMPLE_RATE = 48000;
const DURATION_SEC = 60;
const BLOCK_SIZE = 512;
const TOTAL_SAMPLES = SAMPLE_RATE * DURATION_SEC;
const NUM_BLOCKS = Math.ceil(TOTAL_SAMPLES / BLOCK_SIZE);
const RUNS = 3; // Run 3 times, report the best (lowest) realtime multiplier.

const host = new Host({
  sampleRate: SAMPLE_RATE,
  maxBlockSize: BLOCK_SIZE,
  audioInputs: 2,
  audioOutputs: 2,
});
const plugin = host.load(PLUGIN_PATH);
plugin.setActive(true);
plugin.setProcessing(true);

// Pre-allocate buffers (reused across blocks — NOT allocated in the loop).
const inL = new Float32Array(BLOCK_SIZE);
const inR = new Float32Array(BLOCK_SIZE);
const outL = new Float32Array(BLOCK_SIZE);
const outR = new Float32Array(BLOCK_SIZE);

// Fill input with a 440 Hz tone.
for (let i = 0; i < BLOCK_SIZE; i++) {
  inL[i] = 0.5 * Math.sin((2 * Math.PI * 440 * i) / SAMPLE_RATE);
  inR[i] = inL[i];
}

// Warmup: process a few blocks first so any lazy allocations / caches
// are flushed before the timed loop.
for (let i = 0; i < 10; i++) {
  plugin.process({ inputs: [inL, inR], outputs: [outL, outR], numSamples: BLOCK_SIZE });
}

console.log(`nst3 process() benchmark`);
console.log(`Plugin:      ${PLUGIN_PATH}`);
console.log(`Sample rate: ${SAMPLE_RATE} Hz`);
console.log(`Block size:  ${BLOCK_SIZE} samples`);
console.log(`Duration:    ${DURATION_SEC}s of stereo audio (${NUM_BLOCKS} blocks)`);
console.log(`Runs:        ${RUNS} (best result reported)`);
console.log(``);

const results = [];
for (let run = 1; run <= RUNS; run++) {
  const start = process.hrtime.bigint();
  for (let b = 0; b < NUM_BLOCKS; b++) {
    plugin.process({ inputs: [inL, inR], outputs: [outL, outR], numSamples: BLOCK_SIZE });
  }
  const elapsedNs = Number(process.hrtime.bigint() - start);
  const elapsedSec = elapsedNs / 1e9;
  const rtMult = elapsedSec / DURATION_SEC;
  results.push({ run, elapsedSec, rtMult });
  console.log(
    `Run ${run}: ${elapsedSec.toFixed(3)}s  →  ${rtMult.toFixed(4)}x realtime  (${(DURATION_SEC / elapsedSec).toFixed(2)}x throughput)`
  );
}

const best = results.reduce((a, b) => (a.rtMult < b.rtMult ? a : b));
console.log(``);
console.log(`--- Best (run ${best.run}) ---`);
console.log(`Processed ${NUM_BLOCKS} blocks (${DURATION_SEC}s of stereo audio at ${SAMPLE_RATE}Hz)`);
console.log(`Block size: ${BLOCK_SIZE} samples`);
console.log(`Elapsed: ${best.elapsedSec.toFixed(3)}s`);
console.log(
  `Realtime multiplier: ${best.rtMult.toFixed(4)}x (lower is better, <1x = faster than realtime)`
);
console.log(`Throughput: ${(DURATION_SEC / best.elapsedSec).toFixed(2)}x realtime`);

plugin.dispose();
