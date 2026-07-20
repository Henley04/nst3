'use strict';
// nst3 — real-time safety smoke test
//
// Asserts that the steady-state process() loop:
//   1. Does not visibly allocate (heapUsed stays bounded across many blocks)
//   2. Does not drift in per-block timing beyond a generous ceiling
//   3. Produces consistent audio output across the run
//
// This test is a *smoke* check, not a full RT-safety proof. A real proof
// requires running the binary under AddressSanitizer (see `npm run build:asan`)
// plus a perf-counter-instrumented build. What this test catches is regressions
// where the steady-state path silently starts allocating or syscall-ing on
// each block — those would show up as heap growth and timing outliers here.

const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const { loadPlugin, ensurePluginBuilt, makeTone } = require('./helpers');

const BLOCK_SIZE = 256;
const NUM_BLOCKS = 1000; // ~5.3s of audio at 48 kHz
// Generous per-block ceiling (microseconds). The audio thread on a modern
// CPU should process a 256-sample Gain block in <100 µs; we use 5 ms as a
// regression-detection ceiling so the test stays robust on shared CI runners.
const PER_BLOCK_CEILING_US = 5000;
// Heap growth allowed across the steady-state loop. Some GC churn is normal;
// we allow up to 2 MB of headroom for V8's internal bookkeeping.
const HEAP_GROWTH_LIMIT_BYTES = 2 * 1024 * 1024;

describe('Real-time safety smoke test', { skip: !ensurePluginBuilt() }, () => {
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

  test('steady-state process() does not grow the heap beyond a small bound', () => {
    const inL = new Float32Array(BLOCK_SIZE);
    const inR = new Float32Array(BLOCK_SIZE);
    const outL = new Float32Array(BLOCK_SIZE);
    const outR = new Float32Array(BLOCK_SIZE);
    // Pre-fill the input once.
    for (let i = 0; i < BLOCK_SIZE; i++) {
      inL[i] = 0.5 * Math.sin((2 * Math.PI * 440 * i) / 48000);
      inR[i] = inL[i];
    }

    // Warmup: flush any lazy allocations / first-call caches.
    for (let i = 0; i < 32; i++) {
      plugin.process({ inputs: [inL, inR], outputs: [outL, outR], numSamples: BLOCK_SIZE });
    }

    // Force a GC snapshot if --expose-gc is available; otherwise fall back
    // to a heap-used delta (less precise, still catches large leaks).
    const gc = typeof global.gc === 'function' ? global.gc : null;
    if (gc) gc();
    const baseline = process.memoryUsage().heapUsed;

    for (let b = 0; b < NUM_BLOCKS; b++) {
      plugin.process({ inputs: [inL, inR], outputs: [outL, outR], numSamples: BLOCK_SIZE });
    }

    if (gc) gc();
    const after = process.memoryUsage().heapUsed;
    const growth = after - baseline;
    // Allow some slack; the assertion is "no runaway growth".
    assert.ok(
      growth < HEAP_GROWTH_LIMIT_BYTES,
      `Heap grew by ${growth} bytes across ${NUM_BLOCKS} blocks (limit ${HEAP_GROWTH_LIMIT_BYTES}). ` +
        `This suggests the steady-state path is allocating.`
    );
  });

  test('per-block timing stays below the regression ceiling', () => {
    const inL = new Float32Array(BLOCK_SIZE);
    const inR = new Float32Array(BLOCK_SIZE);
    const outL = new Float32Array(BLOCK_SIZE);
    const outR = new Float32Array(BLOCK_SIZE);

    // Warmup
    for (let i = 0; i < 32; i++) {
      plugin.process({ inputs: [inL, inR], outputs: [outL, outR], numSamples: BLOCK_SIZE });
    }

    // Measure per-block wall-clock time. process.hrtime.bigint gives nanosecond
    // resolution; we convert to microseconds for the assertion.
    const samples = new Float64Array(NUM_BLOCKS);
    for (let b = 0; b < NUM_BLOCKS; b++) {
      const t0 = process.hrtime.bigint();
      plugin.process({ inputs: [inL, inR], outputs: [outL, outR], numSamples: BLOCK_SIZE });
      const t1 = process.hrtime.bigint();
      samples[b] = Number(t1 - t0) / 1000; // µs
    }

    // p99 timing — most blocks should be fast; a tiny fraction may be hit by
    // GC pauses or OS scheduling. We assert the 99th percentile is below the
    // ceiling, NOT the max, so a single outlier (e.g. a major GC pause)
    // doesn't fail the test on noisy CI runners.
    const sorted = Float64Array.from(samples).sort();
    const p99Idx = Math.floor(sorted.length * 0.99);
    const p99 = sorted[p99Idx];

    // Also compute the median for diagnostic purposes (not asserted, just
    // logged via the error message if the assertion fails).
    const median = sorted[Math.floor(sorted.length / 2)];

    assert.ok(
      p99 < PER_BLOCK_CEILING_US,
      `p99 per-block time ${p99.toFixed(1)} µs exceeds ceiling ${PER_BLOCK_CEILING_US} µs ` +
        `(median ${median.toFixed(1)} µs). The steady-state path may be doing too much work.`
    );
  });

  test('output is deterministic across many blocks of identical input', () => {
    const inL = new Float32Array(BLOCK_SIZE);
    const inR = new Float32Array(BLOCK_SIZE);
    const outL_ref = new Float32Array(BLOCK_SIZE);
    const outR_ref = new Float32Array(BLOCK_SIZE);
    const outL = new Float32Array(BLOCK_SIZE);
    const outR = new Float32Array(BLOCK_SIZE);

    // Identical input every block.
    for (let i = 0; i < BLOCK_SIZE; i++) {
      inL[i] = 0.5 * Math.sin((2 * Math.PI * 440 * i) / 48000);
      inR[i] = inL[i];
    }

    // First block establishes the reference output.
    plugin.process({ inputs: [inL, inR], outputs: [outL_ref, outR_ref], numSamples: BLOCK_SIZE });

    // Subsequent blocks should produce bit-identical output (the Gain fixture
    // is a stateless processor with no internal state that drifts across
    // blocks of identical input).
    let mismatches = 0;
    for (let b = 1; b < 100; b++) {
      plugin.process({ inputs: [inL, inR], outputs: [outL, outR], numSamples: BLOCK_SIZE });
      for (let c = 0; c < 2; c++) {
        const ref = c === 0 ? outL_ref : outR_ref;
        const got = c === 0 ? outL : outR;
        for (let i = 0; i < BLOCK_SIZE; i++) {
          if (got[i] !== ref[i]) mismatches++;
        }
      }
    }
    assert.strictEqual(mismatches, 0, 'Steady-state output is not bit-identical across blocks');
  });
});
