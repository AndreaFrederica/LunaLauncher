#!/usr/bin/env node
/**
 * Modrinth API 测试
 *
 * 这个测试文件模拟 C++ 环境，提供必要的全局对象，
 * 然后加载并测试 modrinth_plugin_api.js
 */

const fs = require('fs');
const path = require('path');
const https = require('https');

// 原生 console（避免循环引用）
const print = {
    log: (...args) => process.stdout.write(args.join(' ') + '\n'),
    error: (...args) => process.stderr.write('[ERROR] ' + args.join(' ') + '\n'),
    warn: (...args) => process.stderr.write('[WARN] ' + args.join(' ') + '\n')
};

// 模拟 C++ 提供的全局 console 对象
global.console = {
    log: (...args) => print.log('[JS]', ...args),
    error: (...args) => print.error(...args),
    warn: (...args) => print.warn(...args)
};

// 模拟 C++ 提供的 http 对象
global.http = {
    get: (url) => {
        return new Promise((resolve, reject) => {
            print.log(`\n[HTTP] GET ${url}`);
            https.get(url, (res) => {
                let data = '';
                res.on('data', chunk => data += chunk);
                res.on('end', () => {
                    try {
                        const json = JSON.parse(data);
                        print.log(`[HTTP] Response: ${res.statusCode} (${data.length} bytes)`);
                        resolve(json);
                    } catch (e) {
                        reject(new Error(`JSON parse error: ${e.message}`));
                    }
                });
            }).on('error', reject);
        });
    }
};

// 加载 API 文件
const apiPath = path.join(__dirname, '../launcher/modplatform/jsapi/modrinth_plugin_api.js');
print.log(`\n[Test] Loading API from: ${apiPath}`);

if (!fs.existsSync(apiPath)) {
    print.error(`API file not found: ${apiPath}`);
    process.exit(1);
}

const apiCode = fs.readFileSync(apiPath, 'utf8');

// 使用 Function 构造函数来捕获返回值
const apiFunc = new Function(apiCode + '\nreturn api;');
const api = apiFunc();

// 验证 API 对象已加载
if (typeof api === 'undefined' || !api) {
    print.error('API object not found after loading');
    process.exit(1);
}

print.log('[Test] API loaded successfully');
print.log(`[Test] API ID: ${api.metadata.id}`);
print.log(`[Test] API Version: ${api.metadata.version}`);
print.log(`[Test] Provider: ${api.metadata.provider}`);

// 测试用例
const tests = {
    testMetadata() {
        print.log('\n=== Test 1: Metadata Validation ===');

        const required = ['id', 'displayName', 'version', 'provider', 'supportedTypes'];
        for (const field of required) {
            if (!api.metadata[field]) {
                throw new Error(`Missing required metadata field: ${field}`);
            }
        }

        if (api.metadata.provider !== 'MODRINTH') {
            throw new Error(`Expected provider MODRINTH, got ${api.metadata.provider}`);
        }

        if (!Array.isArray(api.metadata.supportedTypes)) {
            throw new Error('supportedTypes must be an array');
        }

        print.log('✓ Metadata validation passed');
    },

    testSearchURL() {
        print.log('\n=== Test 2: Search URL Generation ===');

        const args1 = {
            offset: 0,
            search: 'worldedit'
        };
        const url1 = api.getSearchURL(args1);
        print.log(`URL: ${url1}`);

        if (!url1.includes('api.modrinth.com/v2/search')) {
            throw new Error('Invalid base URL');
        }
        if (!url1.includes('query=worldedit')) {
            throw new Error('Search term not encoded');
        }
        if (!url1.includes('project_type:plugin')) {
            throw new Error('Missing plugin type filter');
        }

        const args2 = {
            offset: 0,
            search: 'essentials',
            versions: ['1.20.1', '1.19.4'],
            pluginLoaders: 0x01
        };
        const url2 = api.getSearchURL(args2);
        print.log(`URL with filters: ${url2}`);

        if (!url2.includes('versions:1.20.1')) {
            throw new Error('Version filter not applied');
        }
        if (!url2.includes('paper')) {
            throw new Error('Loader filter not applied');
        }

        print.log('✓ Search URL generation passed');
    },

    testInfoURL() {
        print.log('\n=== Test 3: Info URL Generation ===');

        const url = api.getInfoURL('worldedit');
        print.log(`URL: ${url}`);

        if (!url.includes('api.modrinth.com/v2/project/worldedit')) {
            throw new Error('Invalid info URL');
        }

        print.log('✓ Info URL generation passed');
    },

    testVersionsURL() {
        print.log('\n=== Test 4: Versions URL Generation ===');

        const args = {
            addonId: 'worldedit',
            versions: ['1.20.1'],
            pluginLoaders: 0x01
        };
        const url = api.getVersionsURL(args);
        print.log(`URL: ${url}`);

        if (!url.includes('api.modrinth.com/v2/project/worldedit/version')) {
            throw new Error('Invalid versions URL');
        }

        print.log('✓ Versions URL generation passed');
    },

    testLoadIndexedPack() {
        print.log('\n=== Test 5: Load Indexed Pack ===');

        const mockData = {
            project_id: 'AANobbMI',
            slug: 'worldedit',
            title: 'WorldEdit',
            description: 'A Minecraft Map Editor',
            author: 'sk89q',
            icon_url: 'https://cdn.modrinth.com/data/AANobbMI/icon.png',
            downloads: 50000000,
            followers: 10000,
            categories: ['utility', 'worldgen'],
            client_side: 'optional',
            server_side: 'required'
        };

        const pack = api.loadIndexedPack(mockData);

        if (pack.name !== 'WorldEdit') throw new Error('Name mismatch');
        if (pack.slug !== 'worldedit') throw new Error('Slug mismatch');
        if (pack.addonId !== 'AANobbMI') throw new Error('AddonId mismatch');
        if (pack.provider !== 'MODRINTH') throw new Error('Provider mismatch');
        if (pack.downloads !== 50000000) throw new Error('Downloads mismatch');
        if (!pack.websiteUrl.includes('modrinth.com')) throw new Error('Website URL invalid');

        print.log(`Loaded pack: ${pack.name}`);
        print.log(`  Downloads: ${pack.downloads}`);
        print.log(`  Follows: ${pack.follows}`);
        print.log('✓ Load indexed pack passed');
    },

    testLoadIndexedPackVersion() {
        print.log('\n=== Test 6: Load Indexed Pack Version ===');

        const mockData = {
            id: 'abc123',
            version_number: '7.2.15',
            name: 'WorldEdit 7.2.15',
            date_published: '2023-06-01T12:00:00Z',
            changelog: '## Changes\n- Fixed bugs',
            game_versions: ['1.20.1', '1.19.4'],
            loaders: ['paper', 'spigot', 'bukkit'],
            files: [
                {
                    primary: true,
                    url: 'https://cdn.modrinth.com/data/AANobbMI/versions/abc123/worldedit-bukkit-7.2.15.jar',
                    filename: 'worldedit-bukkit-7.2.15.jar',
                    size: 5242880,
                    hashes: {
                        sha1: 'abcdef1234567890',
                        sha512: 'fedcba0987654321'
                    }
                }
            ],
            dependencies: [
                {
                    project_id: 'worldguard',
                    dependency_type: 'optional'
                }
            ]
        };

        const version = api.loadIndexedPackVersion(mockData, 3);

        if (version.version !== '7.2.15') throw new Error('Version mismatch');
        if (version.versionId !== 'abc123') throw new Error('VersionId mismatch');
        if (version.fileName !== 'worldedit-bukkit-7.2.15.jar') throw new Error('FileName mismatch');
        if (version.fileSize !== 5242880) throw new Error('FileSize mismatch');
        if (!version.downloadUrl.includes('cdn.modrinth.com')) throw new Error('Download URL invalid');
        if (version.gameVersions.length !== 2) throw new Error('Game versions mismatch');
        if (version.dependencies.length !== 1) throw new Error('Dependencies mismatch');

        print.log(`Loaded version: ${version.version}`);
        print.log(`  File: ${version.fileName} (${version.fileSize} bytes)`);
        print.log(`  Game versions: ${version.gameVersions.join(', ')}`);
        print.log('✓ Load indexed pack version passed');
    },

    testDocumentToArray() {
        print.log('\n=== Test 7: Document To Array ===');

        const searchDoc = {
            hits: [{ id: 1 }, { id: 2 }, { id: 3 }],
            offset: 0,
            limit: 25,
            total_hits: 100
        };

        const arr1 = api.documentToArray(searchDoc);
        if (arr1.length !== 3) throw new Error('Search result conversion failed');

        const versionDoc = [{ id: 'v1' }, { id: 'v2' }];
        const arr2 = api.documentToArray(versionDoc);
        if (arr2.length !== 2) throw new Error('Version list conversion failed');

        print.log(`Converted search doc to array: ${arr1.length} items`);
        print.log(`Converted version doc to array: ${arr2.length} items`);
        print.log('✓ Document to array conversion passed');
    },

    testSortingMethods() {
        print.log('\n=== Test 8: Sorting Methods ===');

        const methods = api.getSortingMethods();

        if (!Array.isArray(methods)) throw new Error('Sorting methods must be an array');
        if (methods.length === 0) throw new Error('No sorting methods defined');

        for (const method of methods) {
            if (!method.name || !method.displayName) {
                throw new Error('Invalid sorting method format');
            }
        }

        print.log(`Available sorting methods: ${methods.map(m => m.name).join(', ')}`);
        print.log('✓ Sorting methods passed');
    },

    testLoaderConversion() {
        print.log('\n=== Test 9: Loader Conversion ===');

        const loaders = 0x01 | 0x02 | 0x04; // Paper + Spigot + Bukkit

        const facets = api.getLoaderFacets(loaders);
        print.log(`Facets: ${facets.join(', ')}`);
        if (facets.length !== 3) throw new Error('Facet conversion failed');

        const strings = api.getLoaderStrings(loaders);
        print.log(`Strings: ${strings.join(', ')}`);
        if (strings.length !== 3) throw new Error('String conversion failed');

        print.log('✓ Loader conversion passed');
    }
};

// 运行所有测试
async function runTests() {
    print.log('\n╔════════════════════════════════════════╗');
    print.log('║  Modrinth Plugin API Test Suite       ║');
    print.log('╚════════════════════════════════════════╝');

    let passed = 0;
    let failed = 0;
    const failedTests = [];

    for (const [name, test] of Object.entries(tests)) {
        try {
            await test();
            passed++;
        } catch (e) {
            failed++;
            failedTests.push({ name, error: e.message });
            print.error(`\n✗ ${name} FAILED: ${e.message}`);
        }
    }

    print.log('\n╔════════════════════════════════════════╗');
    print.log('║  Test Results                          ║');
    print.log('╚════════════════════════════════════════╝');
    print.log(`Total: ${passed + failed}`);
    print.log(`Passed: ${passed} ✓`);
    print.log(`Failed: ${failed} ✗`);

    if (failed > 0) {
        print.log('\nFailed tests:');
        failedTests.forEach(({ name, error }) => {
            print.log(`  - ${name}: ${error}`);
        });
        process.exit(1);
    } else {
        print.log('\n✓ All tests passed!');
        process.exit(0);
    }
}

// 执行测试
runTests().catch(e => {
    print.error(`Fatal error: ${e.message}`);
    print.error(e.stack);
    process.exit(1);
});
