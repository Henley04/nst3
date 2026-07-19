'use strict';
//
// examples/process-file.js
//
// Reads a WAV file, processes every sample through a VST3 plugin in
// 512-sample blocks, and writes the result to a new WAV file. This is the
// core use case of nvst3-host: offline audio processing through any VST3 effect.
//
// A minimal pure-JS WAV reader/writer is included (no external deps) that
// handles 16-bit PCM and 32-bit float, mono/stereo, any common sample rate.
//
// Usage:
//   node examples/process-file.js <input.wav> <plugin.vst3> [output.wav] [param-id:value] ...
//
// Examples:
//   node examples/process-file.js drums.wav test/plugin/build/Gain.vst3 out.wav 0:0.5
//   node examples/process-file.js --generate sine.wav       # write a 2s 440Hz sine
//   node examples/process-file.js --generate sine.wav Gain.vst3 gained.wav 0:0.25
//
// If <input.wav> does not exist and --generate is NOT supplied, the script
// will offer to generate a test tone into that path instead of failing.
//

const fs = require('fs');
const path = require('path');
const { Host, version } = require('../');

const BLOCK_SIZE = 512;

// ---------------------------------------------------------------------------
// Minimal WAV reader / writer (RIFF, PCM + IEEE float)
// ---------------------------------------------------------------------------
const WAV_FORMAT_PCM = 0x0001;
const WAV_FORMAT_FLOAT = 0x0003;

/**
 * Parse a WAV file from a Buffer.
 * Supports 16-bit PCM and 32-bit float, mono/stereo, any sample rate.
 *
 * @param {Buffer} buf
 * @returns {{
 *   sampleRate: number,
 *   numChannels: number,
 *   bitsPerSample: number,
 *   format: number,
 *   channels: Float32Array[],   // one Float32Array per channel, in [-1,1]
 *   numSamples: number,
 * }}
 */
function readWav(buf) {
    if (buf.length < 12) throw new Error('WAV file too small (truncated RIFF header)');
    if (buf.toString('ascii', 0, 4) !== 'RIFF') throw new Error('Not a RIFF file');
    if (buf.toString('ascii', 8, 12) !== 'WAVE') throw new Error('Not a WAVE file');

    let offset = 12;
    let sampleRate = 0, numChannels = 0, bitsPerSample = 0, format = WAV_FORMAT_PCM;
    /** @type {Buffer|null} */
    let dataChunk = null;

    while (offset + 8 <= buf.length) {
        const id = buf.toString('ascii', offset, offset + 4);
        const size = buf.readUInt32LE(offset + 4);
        const body = offset + 8;
        if (id === 'fmt ') {
            format = buf.readUInt16LE(body + 0);
            numChannels = buf.readUInt16LE(body + 2);
            sampleRate = buf.readUInt32LE(body + 4);
            // bytesPerSecond  @ body+6 (ignored)
            // blockAlign      @ body+10 (ignored)
            bitsPerSample = buf.readUInt16LE(body + 14);
        } else if (id === 'data') {
            // Some writers write a bogus size; clamp to the buffer end.
            const end = Math.min(body + size, buf.length);
            dataChunk = buf.slice(body, end);
        }
        offset = body + size + (size & 1); // pad to even
    }

    if (!dataChunk) throw new Error('WAV missing data chunk');
    if (!sampleRate || !numChannels || !bitsPerSample) {
        throw new Error('WAV missing or malformed fmt chunk');
    }
    if (format !== WAV_FORMAT_PCM && format !== WAV_FORMAT_FLOAT) {
        throw new Error(`Unsupported WAV format: 0x${format.toString(16)} (only PCM 0x1 and float 0x3)`);
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
                if (bitsPerSample === 16) {
                    v = dataChunk.readInt16LE(off) / 32768;
                } else if (bitsPerSample === 24) {
                    const b0 = dataChunk.readUInt8(off);
                    const b1 = dataChunk.readUInt8(off + 1);
                    const b2 = dataChunk.readInt8(off + 2); // sign-extended high byte
                    v = ((b2 << 16) | (b1 << 8) | b0) / 8388608;
                } else if (bitsPerSample === 32) {
                    v = dataChunk.readInt32LE(off) / 2147483648;
                } else if (bitsPerSample === 8) {
                    // 8-bit PCM is unsigned 0..255, midpoint 128
                    v = (dataChunk.readUInt8(off) - 128) / 128;
                } else {
                    throw new Error(`Unsupported PCM bit depth: ${bitsPerSample}`);
                }
            } else { // WAV_FORMAT_FLOAT
                if (bitsPerSample === 32) v = dataChunk.readFloatLE(off);
                else if (bitsPerSample === 64) v = dataChunk.readDoubleLE(off);
                else throw new Error(`Unsupported float bit depth: ${bitsPerSample}`);
            }
            channels[c][i] = v;
        }
    }

    return { sampleRate, numChannels, bitsPerSample, format, channels, numSamples: numFrames };
}

/**
 * Serialize per-channel Float32 sample data back to a WAV Buffer.
 * Preserves the input format (PCM 16-bit or 32-bit float).
 *
 * @param {Float32Array[]} channels
 * @param {number} sampleRate
 * @param {number} bitsPerSample  16 or 32
 * @param {number} format         WAV_FORMAT_PCM or WAV_FORMAT_FLOAT
 * @returns {Buffer}
 */
function writeWav(channels, sampleRate, bitsPerSample, format) {
    const numChannels = channels.length;
    const numFrames = channels[0] ? channels[0].length : 0;
    const bytesPerSample = bitsPerSample / 8 | 0;
    const dataSize = numFrames * numChannels * bytesPerSample;
    const fmtChunkSize = 16;
    const buf = Buffer.alloc(12 + 8 + fmtChunkSize + 8 + dataSize);

    // RIFF header
    buf.write('RIFF', 0);
    buf.writeUInt32LE(buf.length - 8, 4); // file size minus 'RIFF' + size
    buf.write('WAVE', 8);

    // fmt chunk
    let o = 12;
    buf.write('fmt ', o); o += 4;
    buf.writeUInt32LE(fmtChunkSize, o); o += 4;
    buf.writeUInt16LE(format, o); o += 2;
    buf.writeUInt16LE(numChannels, o); o += 2;
    buf.writeUInt32LE(sampleRate, o); o += 4;
    buf.writeUInt32LE(sampleRate * numChannels * bytesPerSample, o); o += 4; // byte rate
    buf.writeUInt16LE(numChannels * bytesPerSample, o); o += 2;              // block align
    buf.writeUInt16LE(bitsPerSample, o); o += 2;

    // data chunk
    buf.write('data', o); o += 4;
    buf.writeUInt32LE(dataSize, o); o += 4;

    for (let i = 0; i < numFrames; i++) {
        for (let c = 0; c < numChannels; c++) {
            let v = channels[c][i];
            // Soft clip to guard against NaN / extreme out-of-range values
            // writing garbage into integer PCM.
            if (!Number.isFinite(v)) v = 0;
            if (v > 1) v = 1; else if (v < -1) v = -1;
            const off = o + (i * numChannels + c) * bytesPerSample;
            if (format === WAV_FORMAT_PCM) {
                if (bitsPerSample === 16) {
                    buf.writeInt16LE((v * 32768) | 0, off);
                } else if (bitsPerSample === 32) {
                    buf.writeInt32LE((v * 2147483648) | 0, off);
                } else if (bitsPerSample === 8) {
                    buf.writeUInt8(((v * 128) + 128) | 0, off);
                } else if (bitsPerSample === 24) {
                    const s = (v * 8388608) | 0;
                    buf.writeUInt8(s & 0xff, off);
                    buf.writeUInt8((s >> 8) & 0xff, off + 1);
                    buf.writeUInt8((s >> 16) & 0xff, off + 2);
                }
            } else { // float
                if (bitsPerSample === 32) buf.writeFloatLE(v, off);
                else if (bitsPerSample === 64) buf.writeDoubleLE(v, off);
            }
        }
    }
    return buf;
}

/**
 * Generate a 2-second 440Hz sine wave as Float32 channel data.
 *
 * @param {number} sampleRate
 * @param {number} numChannels
 * @param {number} [durationSec]
 * @returns {Float32Array[]}
 */
function generateSineWave(sampleRate, numChannels, durationSec) {
    const dur = durationSec == null ? 2 : durationSec;
    const numSamples = sampleRate * dur;
    /** @type {Float32Array[]} */
    const channels = [];
    for (let c = 0; c < numChannels; c++) channels.push(new Float32Array(numSamples));
    const freq = 440;
    const amp = 0.6;
    for (let i = 0; i < numSamples; i++) {
        const v = Math.sin(2 * Math.PI * freq * (i / sampleRate)) * amp;
        for (let c = 0; c < numChannels; c++) channels[c][i] = v;
    }
    return channels;
}

// ---------------------------------------------------------------------------
// CLI argument handling
// ---------------------------------------------------------------------------

/**
 * Parse argv. Recognises `--generate` anywhere and `param-id:value` tokens
 * (where param-id is an integer and value is a float in [0,1]).
 *
 * @param {string[]} argv
 * @returns {{
 *   generate: boolean,
 *   inputPath: string|null,
 *   pluginPath: string|null,
 *   outputPath: string|null,
 *   params: {id:number,value:number}[],
 * }}
 */
function parseArgs(argv) {
    const positional = [];
    let generate = false;
    /** @type {{id:number,value:number}[]} */
    const params = [];

    for (const arg of argv) {
        if (arg === '--generate') { generate = true; continue; }
        const colon = arg.indexOf(':');
        if (colon > 0 && /^\d+$/.test(arg.slice(0, colon))) {
            const id = parseInt(arg.slice(0, colon), 10);
            const value = parseFloat(arg.slice(colon + 1));
            if (Number.isFinite(value)) {
                params.push({ id, value });
                continue;
            }
        }
        positional.push(arg);
    }

    return {
        generate,
        inputPath: positional[0] || null,
        pluginPath: positional[1] || null,
        outputPath: positional[2] || null,
        params,
    };
}

function usage() {
    console.log(
        'Usage: node examples/process-file.js <input.wav> <plugin.vst3> [output.wav] [param-id:value] ...\n' +
        '       node examples/process-file.js --generate <output.wav>\n\n' +
        'Parameters:\n' +
        '  --generate        Write a 2-second 440Hz sine wave test WAV and exit\n' +
        '                    (or, if a plugin is also given, process that test tone).\n' +
        '  param-id:value    Set plugin parameter `param-id` to `value` (0..1) before\n' +
        '                    processing. May be repeated. e.g. 0:0.5\n'
    );
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

function main() {
    const args = parseArgs(process.argv.slice(2));

    if (!args.inputPath && !args.generate) {
        usage();
        process.exit(1);
    }

    // --- Determine input audio ----------------------------------------------
    let inputChannels;
    let sampleRate = 44100;
    let inputNumChannels = 2;
    let inputBitsPerSample = 16;
    let inputFormat = WAV_FORMAT_PCM;
    /** @type {string|null} */
    let inputPath = args.inputPath;

    if (args.generate && (!inputPath || !fs.existsSync(inputPath))) {
        // Generate the test tone inline (no file needed).
        inputPath = inputPath || '<generated>';
        inputNumChannels = 2;
        sampleRate = 44100;
        console.log(`Generating 2-second 440Hz sine wave @ ${sampleRate}Hz, stereo.`);
        inputChannels = generateSineWave(sampleRate, inputNumChannels, 2);
    } else {
        if (!inputPath || !fs.existsSync(inputPath)) {
            console.error(`\nError: input file not found: ${inputPath || '(none)'}\n`);
            console.error('Tip: pass --generate to create a test tone, e.g.:');
            console.error('  node examples/process-file.js --generate sine.wav');
            process.exit(1);
        }
        const buf = fs.readFileSync(inputPath);
        const wav = readWav(buf);
        sampleRate = wav.sampleRate;
        inputNumChannels = wav.numChannels;
        inputBitsPerSample = wav.bitsPerSample;
        inputFormat = wav.format;
        inputChannels = wav.channels;
        console.log(
            `Read ${inputPath}: ${wav.numSamples} samples, ${inputNumChannels}ch, ` +
            `${sampleRate}Hz, ${inputBitsPerSample}-bit ` +
            `${inputFormat === WAV_FORMAT_FLOAT ? 'float' : 'PCM'}.`
        );
    }

    // --- If only --generate was requested, write the test WAV and exit ------
    if (args.generate && !args.pluginPath) {
        // Standalone --generate: the first positional arg (if any) is the
        // output path; otherwise default to 'generated.wav'.
        const outPath = args.inputPath || args.outputPath || 'generated.wav';
        const outBuf = writeWav(inputChannels, sampleRate, 16, WAV_FORMAT_PCM);
        fs.writeFileSync(outPath, outBuf);
        console.log(`Wrote ${outPath} (${outBuf.length} bytes).`);
        return;
    }

    // --- We need a plugin from here on --------------------------------------
    if (!args.pluginPath) {
        console.error('\nError: plugin path required when processing audio.\n');
        usage();
        process.exit(1);
    }
    if (!fs.existsSync(args.pluginPath)) {
        console.error(`\nError: plugin not found: ${args.pluginPath}`);
        console.error('\nBuild the bundled test plugin first with:  npm run test:plugin');
        process.exit(1);
    }

    // Ensure stereo: nvst3-host default host uses 2 in / 2 out. If the source is
    // mono, duplicate to both channels. (The host can be configured for mono
    // too, but stereo is the most common case.)
    if (inputNumChannels === 1) {
        console.log('Input is mono — duplicating to stereo.');
        inputChannels = [inputChannels[0], inputChannels[0].slice()];
        inputNumChannels = 2;
    }

    const numChannels = inputNumChannels;
    const numSamples = inputChannels[0].length;
    const outputPath = args.outputPath || (inputPath && inputPath !== '<generated>'
        ? inputPath.replace(/\.wav$/i, '') + '.processed.wav'
        : 'output.wav');

    /** @type {import('../').PluginInstance|null} */
    let plugin = null;
    /** @type {Host|null} */
    let host = null;

    try {
        host = new Host({
            sampleRate,
            maxBlockSize: BLOCK_SIZE,
            audioInputs: numChannels,
            audioOutputs: numChannels,
        });

        plugin = host.load(args.pluginPath);
        const info = plugin.getInfo();
        console.log(
            `\nLoaded plugin: ${info.name} v${info.version} (${info.vendor})\n` +
            `  classId:    ${info.classId}\n` +
            `  category:   ${info.category} | ${info.subCategories}\n` +
            `  audio I/O:  ${info.numAudioInputs} in / ${info.numAudioOutputs} out\n` +
            `  latency:    ${plugin.getLatency()} samples`
        );

        // Apply CLI parameter overrides before activation.
        if (args.params.length > 0) {
            const count = plugin.getParameterCount();
            console.log(`\nSetting ${args.params.length} parameter(s) (plugin has ${count}):`);
            for (const p of args.params) {
                if (p.id < 0 || p.id >= count) {
                    console.warn(`  ! param ${p.id} out of range [0,${count - 1}] — skipped`);
                    continue;
                }
                plugin.setParameter(p.id, p.value);
                const pi = plugin.getParameterInfo(p.id);
                console.log(`  param ${p.id} (${pi.title}) = ${p.value}`);
            }
        }

        plugin.setActive(true);
        plugin.setProcessing(true);

        // --- Process in blocks ------------------------------------------------
        const numBlocks = Math.ceil(numSamples / BLOCK_SIZE);
        console.log(`\nProcessing ${numSamples} samples in ${numBlocks} block(s) of up to ${BLOCK_SIZE}…`);

        /** @type {Float32Array[]} */
        const outChannels = [];
        for (let c = 0; c < numChannels; c++) {
            outChannels.push(new Float32Array(numSamples));
        }

        /** @type {Float32Array[]} */
        const inBuf = [];
        /** @type {Float32Array[]} */
        const outBuf = [];
        for (let c = 0; c < numChannels; c++) {
            inBuf.push(new Float32Array(BLOCK_SIZE));
            outBuf.push(new Float32Array(BLOCK_SIZE));
        }

        for (let b = 0; b < numBlocks; b++) {
            const start = b * BLOCK_SIZE;
            const n = Math.min(BLOCK_SIZE, numSamples - start);

            for (let c = 0; c < numChannels; c++) {
                inBuf[c].set(inputChannels[c].subarray(start, start + n));
                // Zero the tail of the buffer if this is the last partial block.
                for (let i = n; i < BLOCK_SIZE; i++) inBuf[c][i] = 0;
                outBuf[c].fill(0);
            }

            plugin.process({ inputs: inBuf, outputs: outBuf, numSamples: n });

            for (let c = 0; c < numChannels; c++) {
                outChannels[c].set(outBuf[c].subarray(0, n), start);
            }

            process.stdout.write(`\rProcessing block ${b + 1}/${numBlocks}…`);
        }
        process.stdout.write('\n');

        plugin.setProcessing(false);
        plugin.setActive(false);

        // Write output WAV — keep the same bit depth / format as the input,
        // unless it was something exotic, in which case fall back to 16-bit PCM.
        let outBits = inputBitsPerSample;
        let outFormat = inputFormat;
        if (outFormat !== WAV_FORMAT_PCM && outFormat !== WAV_FORMAT_FLOAT) {
            outFormat = WAV_FORMAT_PCM;
            outBits = 16;
        } else if (outFormat === WAV_FORMAT_PCM && outBits !== 16 && outBits !== 8 && outBits !== 24 && outBits !== 32) {
            outBits = 16;
        } else if (outFormat === WAV_FORMAT_FLOAT && outBits !== 32 && outBits !== 64) {
            outBits = 32;
        }

        const outBuf2 = writeWav(outChannels, sampleRate, outBits, outFormat);
        fs.writeFileSync(outputPath, outBuf2);
        console.log(
            `\nWrote ${outputPath} (${outBuf2.length} bytes, ${outBits}-bit ` +
            `${outFormat === WAV_FORMAT_FLOAT ? 'float' : 'PCM'}).`
        );

        // Drain any output MIDI events the plugin may have emitted.
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
