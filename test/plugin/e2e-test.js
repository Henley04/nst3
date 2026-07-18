// End-to-end test for the Gain mock VST3 plugin via the nst3 host addon.
// Verifies: load, info, parameters, processing at multiple gain values,
// state save/load round-trip into a fresh instance, and dispose lifecycle.
const assert = require('assert');
const path = require('path');
const nst3 = require(path.join(__dirname, '..', '..', 'build', 'Release', 'nst3.node'));

const PLUGIN_PATH = path.join(__dirname, 'build', 'Gain.vst3');
const TOL = 1e-6;

function makeBuf(value, n) {
    return new Float32Array(n).fill(value);
}

let failures = 0;
function check(label, cond, extra) {
    if (cond) {
        console.log('  PASS:', label);
    } else {
        console.log('  FAIL:', label, extra || '');
        failures++;
    }
}

const host = new nst3.Host({ sampleRate: 48000, maxBlockSize: 4, audioInputs: 2, audioOutputs: 2 });

// === Step 1: Load + getInfo ===
console.log('\n[1] Load + getInfo');
const p1 = host.load(PLUGIN_PATH);
const info = p1.getInfo();
check('info.name === "Gain"', info.name === 'Gain', info.name);
check('info.version === "1.0.0"', info.version === '1.0.0', info.version);
check('info.subCategories === "Fx"', info.subCategories === 'Fx', info.subCategories);
check('info.numAudioInputs === 2', info.numAudioInputs === 2, info.numAudioInputs);
check('info.numAudioOutputs === 2', info.numAudioOutputs === 2, info.numAudioOutputs);
check('info.parameterCount === 1', info.parameterCount === 1, info.parameterCount);
check('info.hasController === true', info.hasController === true, info.hasController);
check('info.isSingleComponent === true', info.isSingleComponent === true, info.isSingleComponent);

// === Step 2: Parameter info ===
console.log('\n[2] Parameter info');
const paramCount = p1.getParameterCount();
check('getParameterCount() === 1', paramCount === 1, paramCount);
const pinfo = p1.getParameterInfo(0);
check('paramInfo.title === "Gain"', pinfo.title === 'Gain', pinfo.title);
check('paramInfo.id === 0', pinfo.id === 0, pinfo.id);
check('paramInfo.defaultNormalizedValue === 1.0', Math.abs(pinfo.defaultNormalizedValue - 1.0) < TOL, pinfo.defaultNormalizedValue);
check('paramInfo.flags has CanAutomate', (pinfo.flags & nst3.ParameterFlags.CanAutomate) !== 0, pinfo.flags);

// === Step 3: Activate + process at multiple gain values ===
console.log('\n[3] Process at multiple gain values');
p1.setActive(true);
p1.setProcessing(true);

// Default gain (1.0) -> passthrough
let inA = makeBuf(0.5, 4), inB = makeBuf(0.5, 4);
let outA = makeBuf(0.0, 4), outB = makeBuf(0.0, 4);
p1.process({ inputs: [inA, inB], outputs: [outA, outB], numSamples: 4 });
check('default gain 1.0 -> 0.5 passthrough', Math.abs(outA[0] - 0.5) < TOL, outA[0]);

// gain=0.5 -> half
p1.setParameters([{ id: 0, value: 0.5 }]);
inA = makeBuf(0.5, 4); inB = makeBuf(0.5, 4);
outA = makeBuf(0.0, 4); outB = makeBuf(0.0, 4);
p1.process({ inputs: [inA, inB], outputs: [outA, outB], numSamples: 4 });
check('gain 0.5 -> 0.25', Math.abs(outA[0] - 0.25) < TOL, outA[0]);

// gain=0.0 -> silence
p1.setParameters([{ id: 0, value: 0.0 }]);
inA = makeBuf(0.5, 4); inB = makeBuf(0.5, 4);
outA = makeBuf(0.0, 4); outB = makeBuf(0.0, 4);
p1.process({ inputs: [inA, inB], outputs: [outA, outB], numSamples: 4 });
check('gain 0.0 -> 0.0', Math.abs(outA[0] - 0.0) < TOL, outA[0]);

// gain=0.75 -> 0.375
p1.setParameters([{ id: 0, value: 0.75 }]);
inA = makeBuf(0.5, 4); inB = makeBuf(0.5, 4);
outA = makeBuf(0.0, 4); outB = makeBuf(0.0, 4);
p1.process({ inputs: [inA, inB], outputs: [outA, outB], numSamples: 4 });
check('gain 0.75 -> 0.375', Math.abs(outA[0] - 0.375) < TOL, outA[0]);

// === Step 4: formatParameter ===
console.log('\n[4] formatParameter');
const formatted = p1.formatParameter(0, 0.5);
check('formatParameter(0, 0.5) non-empty', typeof formatted === 'string' && formatted.length > 0, formatted);

// === Step 5: State save ===
console.log('\n[5] State save (gain=0.75)');
const state = p1.saveState();
check('state is Buffer', Buffer.isBuffer(state), typeof state);
check('state length is 4 (float)', state.length === 4, state.length);
console.log('  state hex:', state.toString('hex'));

// === Step 6: State load into fresh instance ===
console.log('\n[6] State load into fresh instance');
const p2 = host.load(PLUGIN_PATH);
p2.setActive(true);
p2.setProcessing(true);

// Before loadState, default gain should be 1.0
const preGain = p2.getParameter(0);
check('p2 getParameter(0) before loadState == 1.0', Math.abs(preGain - 1.0) < TOL, preGain);

// Load the saved state (gain=0.75)
p2.loadState(state);

// After loadState, parameter should be 0.75
const postGain = p2.getParameter(0);
check('p2 getParameter(0) after loadState == 0.75', Math.abs(postGain - 0.75) < TOL, postGain);

// Process and verify gain is applied
inA = makeBuf(0.5, 4); inB = makeBuf(0.5, 4);
outA = makeBuf(0.0, 4); outB = makeBuf(0.0, 4);
p2.process({ inputs: [inA, inB], outputs: [outA, outB], numSamples: 4 });
check('p2 after loadState: 0.5 * 0.75 = 0.375', Math.abs(outA[0] - 0.375) < TOL, outA[0]);

// === Step 7: Lifecycle (dispose idempotent) ===
console.log('\n[7] Lifecycle');
p1.dispose();
p1.dispose(); // idempotent - should not throw
check('dispose() idempotent', true);
p2.dispose();
check('p2 dispose ok', true);

// === Step 8: Load again after dispose ===
console.log('\n[8] Load after dispose');
const p3 = host.load(PLUGIN_PATH);
p3.setActive(true);
p3.setProcessing(true);
p3.setParameters([{ id: 0, value: 0.5 }]);
inA = makeBuf(0.5, 4); inB = makeBuf(0.5, 4);
outA = makeBuf(0.0, 4); outB = makeBuf(0.0, 4);
p3.process({ inputs: [inA, inB], outputs: [outA, outB], numSamples: 4 });
check('p3 (fresh after dispose) gain 0.5 -> 0.25', Math.abs(outA[0] - 0.25) < TOL, outA[0]);
p3.dispose();

// === Summary ===
console.log('\n========================================');
if (failures === 0) {
    console.log('ALL TESTS PASSED');
} else {
    console.log(`${failures} TEST(S) FAILED`);
    process.exit(1);
}
