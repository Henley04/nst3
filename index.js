'use strict';
// nst3 — VST3 Host for Node.js
// Loader: resolves the prebuilt native binary (or falls back to a source build)
// via node-gyp-build, then re-exports the addon surface.
//
// End users never need a C++ toolchain: `npm install nst3` ships prebuilt
// .node binaries for win32-x64, darwin-x64, darwin-arm64 and linux-x64 inside
// the npm tarball. If no prebuilt matches the current runtime, node-gyp-build
// attempts a `node-gyp rebuild` fallback (requires a toolchain); if that also
// fails we throw a structured error with code 'VST3_PLATFORM_UNSUPPORTED'.

const path = require('path');

const SUPPORTED_TRIPLES = [
    'win32-x64',
    'darwin-x64',
    'darwin-arm64',
    'linux-x64',
];

function describeRuntime() {
    return `${process.platform}-${process.arch}`;
}

function loadNative() {
    // node-gyp-build resolves prebuilds/<triple>/nst3.node or falls back to
    // build/Release/nst3.node (the latter produced by `npm run build`).
    const binding = require('node-gyp-build')(path.join(__dirname));
    return binding;
}

let native;
try {
    native = loadNative();
} catch (err) {
    const triple = describeRuntime();
    if (SUPPORTED_TRIPLES.indexOf(triple) === -1) {
        // Truly unsupported platform — surface a structured error so callers
        // can detect this case programmatically.
        const e = new Error(
            `nst3: unsupported platform '${triple}'. ` +
            `Supported triples: ${SUPPORTED_TRIPLES.join(', ')}.`
        );
        e.code = 'VST3_PLATFORM_UNSUPPORTED';
        e.supportedTriples = SUPPORTED_TRIPLES;
        e.runtimeTriple = triple;
        throw e;
    }
    // Supported triple but binary still missing — typically a toolchain issue
    // during a source-build fallback. Re-throw with extra context.
    const e = new Error(
        `nst3: failed to load native binary for '${triple}'. ` +
        `Run 'npm run build' from the package directory, or reinstall ` +
        `to obtain the prebuilt binary. Underlying error: ${err && err.message ? err.message : err}`
    );
    e.code = 'VST3_LOAD_FAILED';
    e.cause = err;
    e.runtimeTriple = triple;
    throw e;
}

module.exports = native;
module.exports.SUPPORTED_TRIPLES = SUPPORTED_TRIPLES;
module.exports.default = native;
