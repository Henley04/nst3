'use strict';
//
// examples/midi-synth.js
//
// Schedules a simple MIDI arpeggio (C4–E4–G4–C5) into a VST3 instrument
// plugin and renders the resulting audio to a WAV file. This demonstrates
// sample-accurate MIDI event scheduling on top of nst3's block-based
// process() loop.
//
// The bundled test Gain plugin is an *effect* (not an instrument), so it
// won't actually produce sound from MIDI — but the host still accepts and
// dispatches the events, which is what this example demonstrates. Plug in
// any real VST3 instrument (e.g. a synth) to hear the arpeggio.
//
// Usage:
//   node examples/midi-synth.js [plugin.vst3] [output.wav]
//
// Examples:
//   node examples/midi-synth.js                                     # use Gain.vst3 if built
//   node examples/midi-synth.js /path/to/Synth.vst3 arpeggio.wav
//

const fs = require('fs');
const path = require('path');
const { Host, MidiEventType, version } = require('../');

const SAMPLE_RATE = 44100;
const BLOCK_SIZE = 512;
const NUM_CHANNELS = 2;       // stereo output
const NOTE_DURATION_SEC = 0.5;
const MELODY = [
    { note: 60, name: 'C4' },  // middle C
    { note: 64, name: 'E4' },
    { note: 67, name: 'G4' },
    { note: 72, name: 'C5' },
];

// ---------------------------------------------------------------------------
// Minimal WAV writer (32-bit float, stereo) — enough for synth render output.
// ---------------------------------------------------------------------------
const WAV_FORMAT_FLOAT = 0x0003;

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

// ---------------------------------------------------------------------------
// MIDI scheduling
// ---------------------------------------------------------------------------

/**
 * Build a sorted list of MIDI events (noteOn / noteOff) for the arpeggio.
 * Each event carries an absolute `sampleOffset` from the start of the render.
 *
 * @param {number} sampleRate
 * @returns {{type:number, channel:number, note:number, velocity:number, sampleOffset:number, label:string}[]}
 */
function buildMelodyEvents(sampleRate) {
    const noteSamples = Math.round(NOTE_DURATION_SEC * sampleRate);
    /** @type {{type:number, channel:number, note:number, velocity:number, sampleOffset:number, label:string}[]} */
    const events = [];
    MELODY.forEach((m, i) => {
        const onOffset = i * noteSamples;
        const offOffset = onOffset + noteSamples;
        events.push({
            type: MidiEventType.NoteOn,
            channel: 0,
            note: m.note,
            velocity: 100,
            sampleOffset: onOffset,
            label: `noteOn  ${m.name} (MIDI ${m.note})`,
        });
        events.push({
            type: MidiEventType.NoteOff,
            channel: 0,
            note: m.note,
            velocity: 0,
            sampleOffset: offOffset,
            label: `noteOff ${m.name} (MIDI ${m.note})`,
        });
    });
    events.sort((a, b) => a.sampleOffset - b.sampleOffset);
    return events;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

function resolvePluginPath(arg) {
    if (arg) {
        if (!fs.existsSync(arg)) {
            console.error(`\nError: plugin not found: ${arg}`);
            console.error('Build the bundled test plugin first with:  npm run test:plugin\n');
            process.exit(1);
        }
        return arg;
    }
    // Fall back to the test Gain plugin if it has been built.
    const candidates = [
        path.join(__dirname, '..', 'test', 'plugin', 'build', 'Gain.vst3'),
        path.join(process.cwd(), 'test', 'plugin', 'build', 'Gain.vst3'),
    ];
    for (const c of candidates) {
        if (fs.existsSync(c)) return c;
    }
    console.error(
        '\nNo plugin path given and the bundled test plugin was not found.\n' +
        'Either pass a plugin path:\n' +
        '  node examples/midi-synth.js /path/to/Synth.vst3\n' +
        'or build the test plugin first:\n' +
        '  npm run test:plugin\n'
    );
    process.exit(1);
}

function main() {
    const argv = process.argv.slice(2);
    const pluginPath = resolvePluginPath(argv[0]);
    const outputPath = argv[1] || 'midi-synth-output.wav';

    const events = buildMelodyEvents(SAMPLE_RATE);
    const totalSamples = events[events.length - 1].sampleOffset; // last noteOff
    const numBlocks = Math.ceil(totalSamples / BLOCK_SIZE);

    console.log(`\nPlugin:      ${pluginPath}`);
    console.log(`Output:      ${outputPath}`);
    console.log(`Sample rate: ${SAMPLE_RATE} Hz`);
    console.log(`Block size:  ${BLOCK_SIZE} samples`);
    console.log(`Melody:      ${MELODY.map(m => m.name).join(' – ')} ` +
                `(each ${NOTE_DURATION_SEC}s, total ${(totalSamples / SAMPLE_RATE).toFixed(2)}s)`);
    console.log(`Blocks:      ${numBlocks}`);
    console.log(`MIDI events: ${events.length}\n`);

    /** @type {import('../').PluginInstance|null} */
    let plugin = null;
    try {
        const host = new Host({
            sampleRate: SAMPLE_RATE,
            maxBlockSize: BLOCK_SIZE,
            audioInputs: NUM_CHANNELS,
            audioOutputs: NUM_CHANNELS,
        });

        plugin = host.load(pluginPath);
        const info = plugin.getInfo();
        console.log(
            `Loaded plugin: ${info.name} v${info.version} (${info.vendor})\n` +
            `  category:   ${info.category} | ${info.subCategories}\n` +
            `  audio I/O:  ${info.numAudioInputs} in / ${info.numAudioOutputs} out\n` +
            `  MIDI  I/O:  ${info.numMidiInputs} in / ${info.numMidiOutputs} out\n` +
            `  latency:    ${plugin.getLatency()} samples`
        );

        plugin.setActive(true);
        plugin.setProcessing(true);

        // Output buffers (one Float32Array per channel for the whole render).
        /** @type {Float32Array[]} */
        const outChannels = [];
        for (let c = 0; c < NUM_CHANNELS; c++) {
            outChannels.push(new Float32Array(totalSamples));
        }

        // Reusable per-block buffers. Instruments generally have no audio
        // input, but nst3 still requires non-null input buffers matching
        // the configured channel count — silence them.
        /** @type {Float32Array[]} */
        const inBuf = [];
        /** @type {Float32Array[]} */
        const outBuf = [];
        for (let c = 0; c < NUM_CHANNELS; c++) {
            inBuf.push(new Float32Array(BLOCK_SIZE));
            outBuf.push(new Float32Array(BLOCK_SIZE));
        }

        let eventIdx = 0;
        let lastReportedNote = -1;

        for (let b = 0; b < numBlocks; b++) {
            const blockStart = b * BLOCK_SIZE;
            const blockEnd = Math.min(blockStart + BLOCK_SIZE, totalSamples);
            const n = blockEnd - blockStart;
            if (n <= 0) break;

            // Schedule any MIDI events whose sampleOffset falls within
            // [blockStart, blockEnd). The plugin receives the offset
            // relative to the start of the current block.
            while (eventIdx < events.length &&
                   events[eventIdx].sampleOffset < blockEnd) {
                const ev = events[eventIdx];
                if (ev.sampleOffset < blockStart) {
                    // Defensive: skip events that fell behind (shouldn't happen).
                    eventIdx++;
                    continue;
                }
                const offsetInBlock = ev.sampleOffset - blockStart;
                plugin.addMidiEvent({
                    type: ev.type,
                    channel: ev.channel,
                    note: ev.note,
                    velocity: ev.velocity,
                    sampleOffset: offsetInBlock,
                });
                if (ev.type === MidiEventType.NoteOn) {
                    const noteIndex = Math.floor(ev.sampleOffset / (NOTE_DURATION_SEC * SAMPLE_RATE));
                    console.log(`Rendering note ${noteIndex + 1}/${MELODY.length} at sample ${ev.sampleOffset}  (${ev.label})`);
                    lastReportedNote = noteIndex;
                }
                eventIdx++;
            }

            // Silence input, zero output, process.
            for (let c = 0; c < NUM_CHANNELS; c++) {
                inBuf[c].fill(0);
                outBuf[c].fill(0);
            }
            plugin.process({ inputs: inBuf, outputs: outBuf, numSamples: n });

            for (let c = 0; c < NUM_CHANNELS; c++) {
                outChannels[c].set(outBuf[c].subarray(0, n), blockStart);
            }

            process.stdout.write(`\rBlock ${b + 1}/${numBlocks} (t=${(blockEnd / SAMPLE_RATE).toFixed(2)}s)…`);
        }
        process.stdout.write('\n');

        plugin.setProcessing(false);
        plugin.setActive(false);

        // Drain any output MIDI events (e.g. NoteOff from a synth's own voice
        // stealing, controller feedback, SysEx dumps).
        const outEvents = plugin.takeOutputEvents();
        if (outEvents && outEvents.length > 0) {
            console.log(`\nPlugin emitted ${outEvents.length} output MIDI event(s):`);
            for (const e of outEvents) {
                console.log(`  type=${e.type} ch=${e.channel} note=${e.note} vel=${e.velocity} off=${e.sampleOffset}`);
            }
        } else {
            console.log('\nPlugin emitted no output MIDI events.');
        }

        const wav = writeWavFloat(outChannels, SAMPLE_RATE);
        fs.writeFileSync(outputPath, wav);
        console.log(`\nWrote ${outputPath} (${wav.length} bytes, 32-bit float, ${NUM_CHANNELS}ch, ${SAMPLE_RATE}Hz).`);

        void lastReportedNote;
    } finally {
        if (plugin) {
            try { plugin.dispose(); } catch (_) { /* idempotent */ }
        }
    }
}

try {
    const v = version();
    console.log(`nst3 ${v.native} (VST3 SDK ${v.vst3sdk}, N-API v${v.napi})`);
    main();
} catch (err) {
    console.error(`\nFatal: ${err && err.message ? err.message : err}`);
    if (err && err.code) console.error(`  code: ${err.code}`);
    process.exit(1);
}
