'use strict';
//
// examples/parameter-sweep.js
//
// Demonstrates parameter automation: ramps a single plugin parameter from
// 0.0 → 1.0 over 3 seconds, then back down to 0.0 over another 3 seconds,
// while feeding a continuous 440Hz sine wave through the plugin. The
// rendered output is written to a WAV file.
//
// Usage:
//   node examples/parameter-sweep.js <plugin.vst3> [param-id] [output.wav]
//
// Options:
//   --input <file.wav>   Read the input signal from a WAV file instead of
//                        generating a 440Hz sine wave.
//
// Examples:
//   node examples/parameter-sweep.js test/plugin/build/Gain.vst3
//   node examples/parameter-sweep.js Gain.vst3 0 sweep.wav
//   node examples/parameter-sweep.js Gain.vst3 0 sweep.wav --input drums.wav
//

const fs = require('fs');
const { Host, ParameterFlags, version } = require('../');

const SAMPLE_RATE = 44100;
const BLOCK_SIZE = 512;
const NUM_CHANNELS = 2;
const SWEEP_UP_SEC = 3;
const SWEEP_DOWN_SEC = 3;
const TOTAL_SEC = SWEEP_UP_SEC + SWEEP_DOWN_SEC;

// ---------------------------------------------------------------------------
// Minimal WAV reader (16-bit PCM + 32-bit float) and writer (32-bit float).
// ---------------------------------------------------------------------------
const WAV_FORMAT_PCM = 0x0001;
const WAV_FORMAT_FLOAT = 0x0003;

/**
 * @param {Buffer} buf
 * @returns {{sampleRate:number, numChannels:number, channels:Float32Array[], numSamples:number}}
 */
function readWav(buf) {
    if (buf.length < 12 || buf.toString('ascii', 0, 4) !== 'RIFF' || buf.toString('ascii', 8, 12) !== 'WAVE') {
        throw new Error('Not a valid WAV/RIFF file');
    }
    let offset = 12;
    let sampleRate = 0, numChannels = 0, bitsPerSample = 0, format = WAV_FORMAT_PCM;
    /** @type {Buffer|null} */
    let dataChunk = null;
    while (offset + 8 <= buf.length) {
        const id = buf.toString('ascii', offset, offset + 4);
        const size = buf.readUInt32LE(offset + 4);
        const body = offset + 8;
        if (id === 'fmt ') {
            format = buf.readUInt16LE(body);
            numChannels = buf.readUInt16LE(body + 2);
            sampleRate = buf.readUInt32LE(body + 4);
            bitsPerSample = buf.readUInt16LE(body + 14);
        } else if (id === 'data') {
            const end = Math.min(body + size, buf.length);
            dataChunk = buf.slice(body, end);
        }
        offset = body + size + (size & 1);
    }
    if (!dataChunk) throw new Error('WAV missing data chunk');
    if (!sampleRate || !numChannels || !bitsPerSample) throw new Error('WAV missing/malformed fmt chunk');
    if (format !== WAV_FORMAT_PCM && format !== WAV_FORMAT_FLOAT) {
        throw new Error(`Unsupported WAV format: 0x${format.toString(16)}`);
    }
    const bytesPerSample = (bitsPerSample / 8) | 0;
    const numFrames = (dataChunk.length / (bytesPerSample * numChannels)) | 0;
    /** @type {Float32Array[]} */
    const channels = [];
    for (let c = 0; c < numChannels; c++) channels.push(new Float32Array(numFrames));
    for (let i = 0; i < numFrames; i++) {
        for (let c = 0; c < numChannels; c++) {
            const off = (i * numChannels + c) * bytesPerSample;
            let v = 0;
            if (format === WAV_FORMAT_PCM) {
                if (bitsPerSample === 16) v = dataChunk.readInt16LE(off) / 32768;
                else if (bitsPerSample === 24) {
                    const b0 = dataChunk.readUInt8(off), b1 = dataChunk.readUInt8(off + 1);
                    const b2 = dataChunk.readInt8(off + 2);
                    v = ((b2 << 16) | (b1 << 8) | b0) / 8388608;
                } else if (bitsPerSample === 32) v = dataChunk.readInt32LE(off) / 2147483648;
                else if (bitsPerSample === 8) v = (dataChunk.readUInt8(off) - 128) / 128;
                else throw new Error(`Unsupported PCM bit depth: ${bitsPerSample}`);
            } else {
                if (bitsPerSample === 32) v = dataChunk.readFloatLE(off);
                else if (bitsPerSample === 64) v = dataChunk.readDoubleLE(off);
                else throw new Error(`Unsupported float bit depth: ${bitsPerSample}`);
            }
            channels[c][i] = v;
        }
    }
    return { sampleRate, numChannels, channels, numSamples: numFrames };
}

/**
 * @param {Float32Array[]} channels
 * @param {number} sampleRate
 * @returns {Buffer}
 */
function writeWavFloat(channels, sampleRate) {
    const numChannels = channels.length;
    const numFrames = channels[0] ? channels[0].length : 0;
    const bitsPerSample = 32;
    const bytesPerSample = 4;
    const dataSize = numFrames * numChannels * bytesPerSample;
    const fmtChunkSize = 16;
    const buf = Buffer.alloc(12 + 8 + fmtChunkSize + 8 + dataSize);
    buf.write('RIFF', 0);
    buf.writeUInt32LE(buf.length - 8, 4);
    buf.write('WAVE', 8);
    let o = 12;
    buf.write('fmt ', o); o += 4;
    buf.writeUInt32LE(fmtChunkSize, o); o += 4;
    buf.writeUInt16LE(WAV_FORMAT_FLOAT, o); o += 2;
    buf.writeUInt16LE(numChannels, o); o += 2;
    buf.writeUInt32LE(sampleRate, o); o += 4;
    buf.writeUInt32LE(sampleRate * numChannels * bytesPerSample, o); o += 4;
    buf.writeUInt16LE(numChannels * bytesPerSample, o); o += 2;
    buf.writeUInt16LE(bitsPerSample, o); o += 2;
    buf.write('data', o); o += 4;
    buf.writeUInt32LE(dataSize, o); o += 4;
    for (let i = 0; i < numFrames; i++) {
        for (let c = 0; c < numChannels; c++) {
            let v = channels[c][i];
            if (!Number.isFinite(v)) v = 0;
            buf.writeFloatLE(v, o + (i * numChannels + c) * bytesPerSample);
        }
    }
    return buf;
}

/**
 * Generate `numSamples` of a 440Hz sine wave into each channel.
 *
 * @param {number} numSamples
 * @param {number} sampleRate
 * @param {number} numChannels
 * @param {number} [startPhase]
 * @returns {{channels:Float32Array[], nextPhase:number}}
 */
function generateSineBlock(numSamples, sampleRate, numChannels, startPhase) {
    const freq = 440;
    const amp = 0.5;
    const phaseStep = 2 * Math.PI * freq / sampleRate;
    let phase = startPhase == null ? 0 : startPhase;
    /** @type {Float32Array[]} */
    const channels = [];
    for (let c = 0; c < numChannels; c++) channels.push(new Float32Array(numSamples));
    for (let i = 0; i < numSamples; i++) {
        const v = Math.sin(phase) * amp;
        phase += phaseStep;
        for (let c = 0; c < numChannels; c++) channels[c][i] = v;
    }
    // Keep phase bounded to avoid float drift over long renders.
    const twoPi = 2 * Math.PI;
    return { channels, nextPhase: phase % twoPi };
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

function usage() {
    console.log(
        'Usage: node examples/parameter-sweep.js <plugin.vst3> [param-id] [output.wav]\n' +
        '                              [--input <file.wav>]\n\n' +
        'Arguments:\n' +
        '  plugin.vst3       Path to the VST3 plugin module.\n' +
        '  param-id          Parameter index to automate (default 0).\n' +
        '  output.wav        Output WAV path (default: parameter-sweep.wav).\n' +
        '  --input <file>    Optional input WAV (default: generated 440Hz sine).\n'
    );
}

/**
 * @param {string[]} argv
 * @returns {{pluginPath:string|null, paramId:number, outputPath:string, inputPath:string|null}}
 */
function parseArgs(argv) {
    /** @type {string[]} */
    const positional = [];
    let inputPath = null;
    for (let i = 0; i < argv.length; i++) {
        const a = argv[i];
        if (a === '--input') {
            inputPath = argv[++i] || null;
            continue;
        }
        if (a === '-h' || a === '--help') {
            usage();
            process.exit(0);
        }
        positional.push(a);
    }
    const paramIdRaw = positional[1];
    const paramId = paramIdRaw == null ? 0 : parseInt(paramIdRaw, 10);
    return {
        pluginPath: positional[0] || null,
        paramId: Number.isFinite(paramId) ? paramId : 0,
        outputPath: positional[2] || 'parameter-sweep.wav',
        inputPath,
    };
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

/**
 * Linear ramp: 0→1 over SWEEP_UP_SEC, then 1→0 over SWEEP_DOWN_SEC.
 *
 * @param {number} tSec
 * @returns {number}
 */
function sweepValueAt(tSec) {
    if (tSec < SWEEP_UP_SEC) return tSec / SWEEP_UP_SEC;
    if (tSec < TOTAL_SEC) return 1 - (tSec - SWEEP_UP_SEC) / SWEEP_DOWN_SEC;
    return 0;
}

function describeParamFlags(flags) {
    if (!flags) return 'NoFlags';
    const names = [];
    for (const key of Object.keys(ParameterFlags)) {
        const v = ParameterFlags[key];
        if (typeof v === 'number' && v !== 0 && (flags & v) === v) names.push(key);
    }
    return names.length ? names.join('|') : 'NoFlags';
}

function main() {
    const args = parseArgs(process.argv.slice(2));
    if (!args.pluginPath) {
        usage();
        process.exit(1);
    }
    if (!fs.existsSync(args.pluginPath)) {
        console.error(`\nError: plugin not found: ${args.pluginPath}`);
        console.error('Build the bundled test plugin first with:  npm run test:plugin\n');
        process.exit(1);
    }

    // --- Input audio --------------------------------------------------------
    let inputSampleRate = SAMPLE_RATE;
    let inputChannels = null;
    let inputNumSamples = 0;
    if (args.inputPath) {
        if (!fs.existsSync(args.inputPath)) {
            console.error(`\nError: --input file not found: ${args.inputPath}`);
            process.exit(1);
        }
        const wav = readWav(fs.readFileSync(args.inputPath));
        inputSampleRate = wav.sampleRate;
        inputChannels = wav.channels;
        inputNumSamples = wav.numSamples;
        console.log(`Input WAV: ${args.inputPath} — ${wav.numSamples} samples, ${wav.numChannels}ch, ${wav.sampleRate}Hz.`);
        if (inputSampleRate !== SAMPLE_RATE) {
            console.warn(`  ! input sample rate ${inputSampleRate} != host ${SAMPLE_RATE}; using host rate.`);
        }
    }

    const totalSamples = Math.max(TOTAL_SEC * SAMPLE_RATE, inputNumSamples);
    const numBlocks = Math.ceil(totalSamples / BLOCK_SIZE);

    /** @type {import('../').PluginInstance|null} */
    let plugin = null;
    try {
        const host = new Host({
            sampleRate: SAMPLE_RATE,
            maxBlockSize: BLOCK_SIZE,
            audioInputs: NUM_CHANNELS,
            audioOutputs: NUM_CHANNELS,
        });

        plugin = host.load(args.pluginPath);
        const info = plugin.getInfo();
        console.log(
            `\nLoaded plugin: ${info.name} v${info.version} (${info.vendor})\n` +
            `  category:   ${info.category} | ${info.subCategories}\n` +
            `  audio I/O:  ${info.numAudioInputs} in / ${info.numAudioOutputs} out\n` +
            `  latency:    ${plugin.getLatency()} samples`
        );

        const paramCount = plugin.getParameterCount();
        if (args.paramId < 0 || args.paramId >= paramCount) {
            console.error(`\nError: param-id ${args.paramId} out of range [0, ${paramCount - 1}].`);
            process.exit(1);
        }

        const pi = plugin.getParameterInfo(args.paramId);
        console.log(
            `\nAutomating parameter ${pi.id}: "${pi.title}"` +
            (pi.shortTitle ? ` (short: "${pi.shortTitle}")` : '') +
            (pi.units ? ` [${pi.units}]` : '') +
            `\n  default: ${pi.defaultNormalizedValue}` +
            `\n  stepCount: ${pi.stepCount} (0 = continuous)` +
            `\n  flags: 0x${pi.flags.toString(16)} (${describeParamFlags(pi.flags)})`
        );

        plugin.setActive(true);
        plugin.setProcessing(true);

        /** @type {Float32Array[]} */
        const outChannels = [];
        for (let c = 0; c < NUM_CHANNELS; c++) outChannels.push(new Float32Array(totalSamples));

        /** @type {Float32Array[]} */
        const inBuf = [];
        /** @type {Float32Array[]} */
        const outBuf = [];
        for (let c = 0; c < NUM_CHANNELS; c++) {
            inBuf.push(new Float32Array(BLOCK_SIZE));
            outBuf.push(new Float32Array(BLOCK_SIZE));
        }

        let phase = 0;
        let nextReportSample = 0; // first report at t=0
        const halfSecSamples = Math.round(0.5 * SAMPLE_RATE);

        console.log(`\nRendering ${numBlocks} block(s), ${TOTAL_SEC}s sweep (${SWEEP_UP_SEC}s up + ${SWEEP_DOWN_SEC}s down)…\n`);

        for (let b = 0; b < numBlocks; b++) {
            const blockStart = b * BLOCK_SIZE;
            const blockEnd = Math.min(blockStart + BLOCK_SIZE, totalSamples);
            const n = blockEnd - blockStart;
            if (n <= 0) break;

            // Param value at the start of the block (sample-accurate ramping
            // within a block would require setParameters with sub-block
            // offsets, which the nvst3-host API does not expose — one value per
            // process() call is the standard approximation used here).
            const tSec = blockStart / SAMPLE_RATE;
            const value = sweepValueAt(tSec);
            plugin.setParameter(args.paramId, value);

            // Build the input block: from file if provided, else sine.
            if (inputChannels) {
                for (let c = 0; c < NUM_CHANNELS; c++) {
                    const src = inputChannels[c % inputChannels.length];
                    for (let i = 0; i < n; i++) {
                        inBuf[c][i] = (blockStart + i < src.length) ? src[blockStart + i] : 0;
                    }
                    for (let i = n; i < BLOCK_SIZE; i++) inBuf[c][i] = 0;
                }
            } else {
                const { channels, nextPhase } = generateSineBlock(n, SAMPLE_RATE, NUM_CHANNELS, phase);
                phase = nextPhase;
                for (let c = 0; c < NUM_CHANNELS; c++) {
                    inBuf[c].set(channels[c]);
                    for (let i = n; i < BLOCK_SIZE; i++) inBuf[c][i] = 0;
                }
            }

            for (let c = 0; c < NUM_CHANNELS; c++) outBuf[c].fill(0);
            plugin.process({ inputs: inBuf, outputs: outBuf, numSamples: n });

            for (let c = 0; c < NUM_CHANNELS; c++) {
                outChannels[c].set(outBuf[c].subarray(0, n), blockStart);
            }

            // Report every 0.5 seconds, using the param value at the scheduled
            // report time. The report fires from whichever block crosses the
            // scheduled sample position.
            while (nextReportSample < blockEnd) {
                const reportT = nextReportSample / SAMPLE_RATE;
                const reportValue = sweepValueAt(reportT);
                console.log(`t=${reportT.toFixed(2)}s param=${reportValue.toFixed(3)}`);
                nextReportSample += halfSecSamples;
            }
            if (b === numBlocks - 1) {
                // Final flush: report the very end if not already covered.
                while (nextReportSample <= totalSamples) {
                    const reportT = nextReportSample / SAMPLE_RATE;
                    const reportValue = sweepValueAt(reportT);
                    console.log(`t=${reportT.toFixed(2)}s param=${reportValue.toFixed(3)}`);
                    nextReportSample += halfSecSamples;
                }
            }
        }

        plugin.setProcessing(false);
        plugin.setActive(false);

        const wav = writeWavFloat(outChannels, SAMPLE_RATE);
        fs.writeFileSync(args.outputPath, wav);
        console.log(`\nWrote ${args.outputPath} (${wav.length} bytes, 32-bit float, ${NUM_CHANNELS}ch, ${SAMPLE_RATE}Hz).`);

        const outEvents = plugin.takeOutputEvents();
        if (outEvents && outEvents.length > 0) {
            console.log(`Plugin emitted ${outEvents.length} output MIDI event(s).`);
        }
    } finally {
        if (plugin) {
            try { plugin.dispose(); } catch (_) { /* idempotent */ }
        }
    }
}

try {
    const v = version();
    console.log(`nvst3-host ${v.native} (VST3 SDK ${v.vst3sdk}, N-API v${v.napi})`);
    main();
} catch (err) {
    console.error(`\nFatal: ${err && err.message ? err.message : err}`);
    if (err && err.code) console.error(`  code: ${err.code}`);
    process.exit(1);
}
