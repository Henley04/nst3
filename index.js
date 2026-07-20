'use strict';
// nvst3-host — VST3 Host for Node.js
// Loader: resolves the prebuilt native binary (or falls back to a source build)
// via node-gyp-build, then re-exports the addon surface.
//
// End users never need a C++ toolchain: `npm install nvst3-host` ships prebuilt
// .node binaries for win32-x64, darwin-arm64, linux-x64, and linux-arm64.
// darwin-x64 (Intel Macs) and win32-ia32 are supported via source-build
// fallback only — GitHub Actions no longer provides those runners. If no
// prebuilt matches the current runtime, node-gyp-build attempts a
// `node-gyp rebuild` fallback (requires a toolchain); if that also fails we
// throw a structured error with code 'VST3_PLATFORM_UNSUPPORTED'.

const path = require('path');

const SUPPORTED_TRIPLES = [
    'win32-x64',
    'darwin-x64',
    'darwin-arm64',
    'linux-x64',
    'linux-arm64',
];

function describeRuntime() {
    return `${process.platform}-${process.arch}`;
}

function loadNative() {
    // node-gyp-build resolves prebuilds/<triple>/nst3.node or falls back to
    // build/Release/nst3.node (the latter produced by `npm run build`).
    // Note: the .node filename is "nst3.node" because binding.gyp's
    // target_name is "nst3"; this is an internal build artifact name and
    // is independent of the npm package name.
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
            `nvst3-host: unsupported platform '${triple}'. ` +
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
        `nvst3-host: failed to load native binary for '${triple}'. ` +
        `Run 'npm run build' from the package directory, or reinstall ` +
        `to obtain the prebuilt binary. Underlying error: ${err && err.message ? err.message : err}`
    );
    e.code = 'VST3_LOAD_FAILED';
    e.cause = err;
    e.runtimeTriple = triple;
    throw e;
}

// --- Process-exit cleanup ------------------------------------------------
// Track every live PluginInstance so we can synchronously dispose native
// resources (VST3 IComponent::terminate, etc.) before the Node process
// exits. Without this, the native finalizers may run during environment
// teardown where it is unsafe to call into the VST3 SDK — leading to
// crashes or leaked handles.
const liveInstances = new Set();

function _registerInstance(instance) {
    liveInstances.add(instance);
}

function _unregisterInstance(instance) {
    liveInstances.delete(instance);
}

// Wrap each PluginInstance's dispose so the instance is removed from the
// live set when the user disposes it. Idempotent: dispose() may be called
// multiple times (the native side no-ops after the first), and
// _unregisterInstance on a missing entry is a no-op.
function patchDispose(instance) {
    if (!instance || instance.__nst3Patched) return;
    const origDispose = instance.dispose;
    if (typeof origDispose !== 'function') return;
    Object.defineProperty(instance, 'dispose', {
        value: function (...dargs) {
            try {
                return origDispose.apply(this, dargs);
            } finally {
                _unregisterInstance(this);
            }
        },
        writable: true,
        configurable: true,
        enumerable: true,
    });
    Object.defineProperty(instance, '__nst3Patched', {
        value: true,
        writable: false,
        configurable: false,
        enumerable: false,
    });
}

// Wrap the Host constructor itself (not its prototype — N-API prototype
// methods are non-writable, so prototype patching throws). The wrapper
// preserves the `new` semantics and tracks every PluginInstance returned
// from .load(). Host is invoked both with and without `new` by callers, so
// we support both.
if (native.Host) {
    const NativeHost = native.Host;
    function HostWrapper(...args) {
        const host = new NativeHost(...args);
        // Wrap load on the instance: N-API instance methods are also
        // non-writable, but we can shadow with an own property via
        // Object.defineProperty.
        const origLoad = host.load;
        if (typeof origLoad === 'function') {
            Object.defineProperty(host, 'load', {
                value: function (...largs) {
                    const inst = origLoad.apply(this, largs);
                    if (inst) {
                        _registerInstance(inst);
                        patchDispose(inst);
                    }
                    return inst;
                },
                writable: true,
                configurable: true,
                enumerable: true,
            });
        }
        return host;
    }
    // Preserve the static methods (scanDefaultLocations, scanDirectory,
    // inspectPlugin) — N-API static methods live on the constructor function
    // itself and are typically writable/configurable.
    Object.setPrototypeOf(HostWrapper, NativeHost);
    HostWrapper.prototype = NativeHost.prototype;
    // Copy own static properties (non-writable ones skipped silently).
    Object.getOwnPropertyNames(NativeHost).forEach((name) => {
        if (name === 'prototype' || name === 'length' || name === 'name') return;
        const desc = Object.getOwnPropertyDescriptor(NativeHost, name);
        if (desc && (desc.writable || desc.configurable)) {
            Object.defineProperty(HostWrapper, name, desc);
        }
    });
    native.Host = HostWrapper;
}

function disposeAllInstances() {
    for (const inst of liveInstances) {
        try {
            // PluginInstance.dispose() is idempotent — safe to call even if
            // the user already disposed the instance.
            if (typeof inst.dispose === 'function') inst.dispose();
        } catch (_) {
            // Swallow per-instance errors so a single bad plugin doesn't
            // block cleanup of the rest.
        }
    }
    liveInstances.clear();
}

// 'beforeExit' fires when the event loop is empty but the process is still
// alive (gives async work a chance to flush). 'exit' fires synchronously
// just before the process terminates — only synchronous cleanup is allowed.
process.on('beforeExit', disposeAllInstances);
process.on('exit', disposeAllInstances);

module.exports = native;
module.exports.SUPPORTED_TRIPLES = SUPPORTED_TRIPLES;
module.exports._registerInstance = _registerInstance;
module.exports._unregisterInstance = _unregisterInstance;
module.exports.default = native;
