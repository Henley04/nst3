// nvst3-host — VST3 Host for Node.js (ESM entry)
// Re-exports the same CommonJS loader surface. Node uses cjs-module-lexer
// to detect named exports from the CJS `index.js` (which it does, since
// every named export is assigned via a static `module.exports.X = ...`
// pattern). The default export is the entire module object.
export * from './index.js';
export { default } from './index.js';
