# Contributing to nst3

Thanks for your interest in contributing to `nst3`! This document covers everything you need to get a local development environment running, build the project, run the tests, and submit a pull request.

## Prerequisites

- **Node.js 20+** — the runtime supports `>= 16.17`, but development uses Node 20 for `node --test` and the prebuild toolchain.
- **Python 3** — required by `node-gyp`.
- **Git** — with submodule support.
- **C++17 compiler**:
  - **macOS**: Xcode Command Line Tools (`xcode-select --install`).
  - **Linux**: `g++` ≥ 11 or `clang++` ≥ 13, plus `libasound2-dev` and `libstdc++-12-dev` (or equivalent).
  - **Windows**: Visual Studio 2022 with the "Desktop development with C++" workload.
- **CMake ≥ 3.22** — only needed to build the test VST3 plugin (see [Building the Test Plugin](#building-the-test-plugin)).

## Getting the Source

`nst3` bundles the official Steinberg VST3 SDK as a git submodule under `third_party/vst3sdk/`. Always clone with submodules:

```bash
git clone --recursive https://github.com/Henley04/nst3.git
cd nst3
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

## Installing Dependencies

```bash
npm ci
```

This installs `node-addon-api`, `node-gyp`, `node-gyp-build`, and `prebuildify` as dev dependencies.

## Building the Native Addon

```bash
npm run build      # node-gyp configure && node-gyp build
```

The compiled addon is written to `build/Release/nst3.node`. Verify it loads:

```bash
node -e "console.log(require('./').version())"
# { native: '0.1.0', vst3sdk: 'VST 3.8.0', napi: 8 }
```

To do a clean rebuild:

```bash
npm run clean && npm run build
```

Or in one step:

```bash
npm run rebuild
```

## Building the Test Plugin

The integration tests rely on a small mock VST3 plugin (`Gain`) built from C++ source under `test/plugin/`. Build it with:

```bash
npm run test:plugin
```

This invokes `node test/plugin/build-plugin.js`, which drives CMake to produce `test/plugin/build/Gain.vst3` for the current platform. CMake ≥ 3.22 and a C++17 compiler are required.

The test plugin is a stereo gain effect with one parameter (`Gain`, normalized 0..1, default 1.0). It supports state save/load (a single `float` for the gain value).

## Running Tests

```bash
npm test
```

This runs `node --test test/`, which executes the JavaScript integration tests against the locally-built addon and the locally-built test plugin. Make sure both are present (see the two sections above).

Individual test files:

```bash
node --test test/discovery.test.js
node --test test/load.test.js
node --test test/process.test.js
node --test test/parameters.test.js
node --test test/midi.test.js
node --test test/state.test.js
node --test test/lifecycle.test.js
node --test test/errors.test.js
```

The test suite covers:

- **discovery** — `scanDirectory` / `inspectPlugin` return correct `PluginInfo` fields.
- **load** — `getInfo()` returns expected name, vendor, version, bus counts, parameter count.
- **process** — gain=0 produces silence; gain=1 produces passthrough; `Float32Array` is filled and zero-copy.
- **parameters** — `getParameterInfo(0)` returns expected fields; `setParameter`/`getParameter` round-trip; `formatParameter` returns non-empty.
- **midi** — `addMidiEvent` accepts all event types without error; if the `TestSynth` is built, note on/off produces non-silent output.
- **state** — save → mutate → load → params restored; loading a saved buffer into a fresh `PluginInstance` works.
- **lifecycle** — `dispose()` twice (no throw); load + dispose + load again (no leak/crash); GC finalizer does not crash.
- **errors** — nonexistent path → `VST3_LOAD_FAILED`; process before `setActive` → `VST3_NOT_ACTIVE`; process before `setProcessing` → `VST3_NOT_PROCESSING`; bad `Float32Array` length → `VST3_INVALID_BUFFER`.

## Creating Prebuilds

To produce prebuilt `.node` binaries for the current platform:

```bash
npm run prebuild
```

This invokes `prebuildify --napi-version 8 --tag-armv -t 20.0.0` and writes files into `prebuilds/<triple>/`. To produce binaries for all four supported platforms, use the CI pipeline (see [CI Pipeline](#ci-pipeline)).

To pack the prebuilds into a tarball for inspection:

```bash
npx prebuildify --pack
```

## Code Style

### C++

- **Standard**: C++17.
- **Indentation**: 4 spaces (no tabs).
- **Brace style**: Allman (opening brace on its own line) for class and function definitions; K&R inside function bodies is acceptable if it matches surrounding code.
- **Naming**: `PascalCase` for classes, structs, and methods; `snake_case_` for member variables (trailing underscore); `kConstantName` for constants; `UPPER_SNAKE_CASE` for preprocessor macros.
- **RTTI and exceptions**: Enabled (the VST3 SDK requires both).
- **Header guards**: Use `#pragma once`.
- **Includes**: Group as (1) own headers, (2) standard library, (3) VST3 SDK, (4) node-addon-api. Sort alphabetically within groups.
- **No `using namespace` in headers** — fully qualify or use narrow `using` declarations inside implementation files only.
- Run `clang-format` (config: `.clang-format` at repo root) before committing if present.

### JavaScript

- **Module system**: CommonJS (`require`/`module.exports`) for runtime code; ESM (`import`/`export`) is acceptable in `index.d.ts` declarations.
- **Indentation**: 4 spaces.
- **Quotes**: Single quotes.
- **Semicolons**: Always.
- **Strict mode**: `'use strict';` at the top of every file (or use ESM which is strict by default).
- Run `npx prettier --write .` if a `.prettierrc` is present.

### TypeScript

- The `index.d.ts` file is hand-written and authoritative — do not auto-generate it.
- Mirror the native surface 1:1; if you add or change a native method, update `index.d.ts` in the same PR.

## Submitting Pull Requests

1. Fork the repo and create a feature branch: `git checkout -b feat/my-feature`.
2. Make your changes. Add or update tests under `test/` as appropriate.
3. Ensure the full suite passes locally: `npm run build && npm run test:plugin && npm test`.
4. Ensure `npm run build` produces no compiler warnings.
5. If you changed the public API, update `index.d.ts`, `docs/API.md`, `CHANGELOG.md` (under an `[Unreleased]` heading), and the relevant section of `README.md`.
6. Commit with a clear message; reference the issue number if applicable (`Fixes #123`).
7. Open a pull request against `main`. Describe what changed and why.
8. CI will run on your branch across all four supported platforms — make sure all checks pass before requesting review.

## CI Pipeline

CI runs on every push and pull request via GitHub Actions (`.github/workflows/CI.yml`). The matrix covers:

| Runner           | OS                | Triple          |
|------------------|-------------------|-----------------|
| `windows-latest` | Windows Server    | `win32-x64`     |
| `macos-14`       | macOS (Apple Si)  | `darwin-arm64`  |
| `ubuntu-latest`  | Ubuntu            | `linux-x64`     |

Each job:

1. Checks out the repo with `submodules: recursive`.
2. Installs Python 3 and Node.js 20.
3. Runs `npm ci`, `npm run build`, `npm run test:plugin`, `npm test`.
4. Runs `npx prebuildify --napi-version 8 --tag-armv -t 20.0.0` to produce a `prebuilds/<triple>/` artifact.
5. Uploads the `prebuilds/` directory as a CI artifact (`actions/upload-artifact@v4`).

A final `release` job (only on tag pushes) downloads all four prebuild artifacts, merges them into a single `prebuilds/` directory, optionally runs `prebuildify --pack`, and publishes to npm using the `NPM_TOKEN` secret.

### Platform-Specific Notes

- **macOS**: `MACOSX_DEPLOYMENT_TARGET=10.13` is set via `CFLAGS`/`CXXFLAGS`/`LDFLAGS` so binaries run on High Sierra and later.
- **Linux**: `libasound2-dev` and `libstdc++-12-dev` are installed; the resulting binary uses `dlopen` and has no plugin-runtime dependencies.
- **Windows**: MSVC with `/std:c++17`, exceptions and RTTI enabled; links against `kernel32.lib`, `user32.lib`, `advapi32.lib`.

## Release Process

Releases are driven entirely by CI. To cut a release:

1. Update `CHANGELOG.md` — promote the `[Unreleased]` section to a new `## [x.y.z] - YYYY-MM-DD` heading.
2. Bump `version` in `package.json` (use `npm version major|minor|patch`).
3. Commit: `git commit -m "chore: release x.y.z"`.
4. Tag and push: `git tag vx.y.z && git push origin main --tags`.
5. The CI `release` job triggers on the tag, builds prebuilds for all four platforms, and publishes the resulting tarball to npm.

No manual `npm publish` — all publishing happens in CI, ensuring the published tarball always contains the correct prebuilds.

### Versioning

This project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html):

- **MAJOR** — breaking API changes (renamed/removed methods, changed signatures, dropped Node.js versions, dropped platform triples).
- **MINOR** — new features added backward-compatibly (new methods, new enum values, new error codes).
- **PATCH** — bug fixes and documentation improvements.

The bundled VST3 SDK version is tracked in `version().vst3sdk` but does **not** affect the package version — it is informational only.

## License

By contributing, you agree that your contributions will be licensed under the project's MIT license. See [`LICENSE`](LICENSE).

The bundled VST3 SDK in `third_party/vst3sdk/` is also MIT-licensed, so contributions that touch SDK-upstream files remain distributable under the same terms.
