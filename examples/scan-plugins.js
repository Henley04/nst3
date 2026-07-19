'use strict';
//
// examples/scan-plugins.js
//
// Scans default VST3 plugin locations (or a custom directory) and prints
// a formatted table of every plugin discovered, including name, vendor,
// version, category, sub-categories and class ID.
//
// Usage:
//   node examples/scan-plugins.js                   # scan platform defaults
//   node examples/scan-plugins.js /path/to/plugins  # scan a custom directory
//   node examples/scan-plugins.js path/to/X.vst3    # inspect a single plugin
//
// Run from the project root:
//   node examples/scan-plugins.js
//

const { Host, version } = require('../');

/**
 * Pad / truncate a string to a fixed column width for table layout.
 * @param {string} s
 * @param {number} width
 * @returns {string}
 */
function column(s, width) {
    const str = s == null ? '' : String(s);
    if (str.length > width) return str.slice(0, Math.max(0, width - 1)) + '\u2026';
    return str + ' '.repeat(width - str.length);
}

/**
 * Print a single plugin row using the configured column widths.
 * @param {import('../').PluginInfo} info
 * @param {{name:number,vendor:number,version:number,category:number,subCategories:number,classId:number}} widths
 */
function printRow(info, widths) {
    const cols = [
        column(info.name, widths.name),
        column(info.vendor, widths.vendor),
        column(info.version, widths.version),
        column(info.category, widths.category),
        column(info.subCategories, widths.subCategories),
        column(info.classId, widths.classId),
    ];
    console.log('  ' + cols.join('  '));
}

/**
 * @param {import('../').PluginInfo[]} plugins
 */
function printTable(plugins) {
    const widths = {
        name: 24,
        vendor: 18,
        version: 10,
        category: 20,
        subCategories: 22,
        classId: 34,
    };
    const header = {
        name: 'Name',
        vendor: 'Vendor',
        version: 'Version',
        category: 'Category',
        subCategories: 'Sub-categories',
        classId: 'Class ID',
    };
    console.log();
    printRow(header, widths);
    console.log('  ' + '-'.repeat(
        widths.name + widths.vendor + widths.version +
        widths.category + widths.subCategories + widths.classId +
        2 * 5 // 2-space gutter between 6 columns
    ));
    for (const p of plugins) printRow(p, widths);
    console.log();
}

function main() {
    const argv = process.argv.slice(2);
    const customPath = argv[0];

    let plugins;
    try {
        if (customPath) {
            // If the user pointed us directly at a .vst3 bundle/file, inspect
            // just that one module; otherwise treat the path as a directory
            // to scan recursively.
            const isVst3File = /\.vst3$/i.test(customPath);
            if (isVst3File) {
                console.log(`Inspecting plugin: ${customPath}`);
                const result = Host.inspectPlugin(customPath);
                plugins = Array.isArray(result) ? result : [result];
            } else {
                console.log(`Scanning directory: ${customPath}`);
                plugins = Host.scanDirectory(customPath);
            }
        } else {
            console.log('Scanning default VST3 plugin locations…');
            plugins = Host.scanDefaultLocations();
        }
    } catch (err) {
        console.error(`\nError: failed to scan plugins: ${err && err.message ? err.message : err}`);
        if (err && err.code) console.error(`  code: ${err.code}`);
        process.exit(1);
    }

    if (!Array.isArray(plugins) || plugins.length === 0) {
        console.log('\nNo VST3 plugins found.');
        if (!customPath) {
            console.log(
                '\nVST3 plugins are searched in the platform-default locations:\n' +
                '  • Linux: ~/.vst3, /usr/lib/vst3, /usr/local/lib/vst3\n' +
                '  • macOS: ~/Library/Audio/Plug-Ins/VST3, /Library/Audio/Plug-Ins/VST3\n' +
                '  • Windows: %LOCALAPPDATA%\\Programs\\Common\\VST3, C:\\Program Files\\Common Files\\VST3\n\n' +
                'Tip: build the bundled test plugin with `npm run test:plugin`, then pass its path:\n' +
                '  node examples/scan-plugins.js test/plugin/build/Gain.vst3'
            );
        }
        console.log('\nFound 0 plugin(s)');
        return;
    }

    printTable(plugins);

    // Also dump the filesystem path for each plugin, below the table, so
    // users can copy-paste it into the other examples.
    console.log('Paths:');
    plugins.forEach((p, i) => {
        console.log(`  [${i}] ${p.path}`);
    });

    console.log(`\nFound ${plugins.length} plugin(s)`);
}

try {
    const v = version();
    console.log(`nvst3-host ${v.native} (VST3 SDK ${v.vst3sdk}, N-API v${v.napi})`);
    main();
} catch (err) {
    console.error(`\nFatal: ${err && err.message ? err.message : err}`);
    process.exit(1);
}
