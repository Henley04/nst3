'use strict';
// nst3 — Mock VST3 test plugin builder
// Runs `cmake -B build -S .` and `cmake --build build --config Release` from
// the test/plugin directory, then verifies the .vst3 bundle exists and prints
// its absolute path for the JS integration tests to consume.
//
// Idempotent: if the bundle is already present, the build is skipped unless
// --force is passed.

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const PLUGIN_DIR = __dirname;
const BUILD_DIR = path.join(PLUGIN_DIR, 'build');
const FORCE = process.argv.includes('--force');

function log(msg) {
    process.stdout.write(`[build-plugin] ${msg}\n`);
}

function fail(msg, err) {
    process.stderr.write(`[build-plugin] ERROR: ${msg}\n`);
    if (err && err.message) process.stderr.write(`${err.message}\n`);
    process.exit(1);
}

function hasCmake() {
    const res = spawnSync('cmake', ['--version'], { stdio: 'pipe' });
    return res.status === 0;
}

function run(cmd, args, cwd) {
    const pretty = `${cmd} ${args.join(' ')}`;
    log(`$ ${pretty}` + (cwd ? ` (cwd: ${path.relative(PLUGIN_DIR, cwd)})` : ''));
    const res = spawnSync(cmd, args, {
        cwd: cwd || PLUGIN_DIR,
        stdio: 'inherit',
    });
    if (res.status !== 0) {
        fail(`command failed (exit ${res.status}): ${pretty}`);
    }
}

// Resolve the platform-specific bundle path inside the build directory.
function resolveBundlePath() {
    const bundleDir = path.join(BUILD_DIR, 'Gain.vst3');
    if (process.platform === 'win32') {
        // On Windows the .vst3 file is a DLL. With the Visual Studio multi-config
        // generator + --config Release, the default output dir is build/Release/.
        // Some generator/setting combinations place it under build/Release/Release/
        // (when LIBRARY_OUTPUT_DIRECTORY is explicitly set); check both.
        const candidates = [
            path.join(BUILD_DIR, 'Release', 'Gain.vst3'),
            path.join(BUILD_DIR, 'Release', 'Release', 'Gain.vst3'),
            path.join(BUILD_DIR, 'Gain.vst3'),
        ];
        for (const c of candidates) {
            if (fs.existsSync(c)) return c;
        }
        return null;
    }
    // Linux/macOS use a bundle directory.
    if (!fs.existsSync(bundleDir)) return null;
    return bundleDir;
}

function verifyBundle(bundlePath) {
    if (!bundlePath || !fs.existsSync(bundlePath)) return false;
    if (process.platform === 'win32') return true;
    // Linux: Gain.vst3/Contents/x86_64-linux/Gain.so
    // macOS:  Gain.vst3/Contents/MacOS/Gain
    if (process.platform === 'darwin') {
        const so = path.join(bundlePath, 'Contents', 'MacOS', 'Gain');
        return fs.existsSync(so);
    }
    if (process.platform === 'linux') {
        const so = path.join(bundlePath, 'Contents', 'x86_64-linux', 'Gain.so');
        return fs.existsSync(so);
    }
    return false;
}

function main() {
    if (!hasCmake()) {
        fail('cmake is not available on PATH. Install CMake to build the test plugin.');
    }

    const existing = resolveBundlePath();
    if (existing && verifyBundle(existing) && !FORCE) {
        log(`Plugin already built — skipping (use --force to rebuild).`);
        log(`Plugin path: ${existing}`);
        process.stdout.write(existing + '\n');
        return;
    }

    if (FORCE && fs.existsSync(BUILD_DIR)) {
        log('Removing previous build directory (--force)');
        try {
            fs.rmSync(BUILD_DIR, { recursive: true, force: true });
        } catch (err) {
            fail(`failed to remove build directory: ${BUILD_DIR}`, err);
        }
    }

    // Configure
    const configureArgs = ['-B', 'build', '-S', '.'];
    if (process.platform === 'linux' || process.platform === 'darwin') {
        configureArgs.push('-DCMAKE_BUILD_TYPE=Release');
    }
    run('cmake', configureArgs, PLUGIN_DIR);

    // Build
    const buildArgs = ['--build', 'build'];
    if (process.platform === 'win32') {
        buildArgs.push('--config', 'Release');
    }
    run('cmake', buildArgs, PLUGIN_DIR);

    // Verify
    const bundlePath = resolveBundlePath();
    if (!bundlePath) {
        fail(`Build completed but bundle was not found at ${path.join(BUILD_DIR, 'Gain.vst3')}`);
    }
    if (!verifyBundle(bundlePath)) {
        fail(`Bundle exists at ${bundlePath} but the platform .so/.dll/.dylib is missing.`);
    }

    log(`Plugin built successfully.`);
    log(`Plugin path: ${bundlePath}`);
    // Last line of stdout is the resolved path, for tests to consume.
    process.stdout.write(bundlePath + '\n');
}

main();
