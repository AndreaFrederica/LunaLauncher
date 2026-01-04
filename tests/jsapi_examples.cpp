// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */

/**
 * JSResourceAPI 使用示例和测试
 * 
 * 展示如何使用 JavaScript 实现资源 API
 */

#include "JSResourceAPI.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>

void testModrinthAPI()
{
    qDebug() << "\n=== Testing Modrinth Plugin API ===";
    
    // 从文件加载 Modrinth API
    auto api = JSResourceAPI::fromFile(
        ":/jsapi/modrinth_plugin_api.js",
        "ModrinthPluginAPI"
    );
    
    if (!api) {
        qWarning() << "Failed to load Modrinth API";
        return;
    }
    
    // 测试搜索 URL 生成
    ResourceAPI::SearchArgs searchArgs;
    searchArgs.offset = 0;
    searchArgs.search = "worldedit";
    searchArgs.pluginLoaders = ModPlatform::PluginLoaderType::PAPER;
    
    auto searchURL = api->getSearchURL(searchArgs);
    if (searchURL) {
        qDebug() << "Search URL:" << *searchURL;
    }
    
    // 测试项目信息 URL
    auto infoURL = api->getInfoURL("worldedit");
    if (infoURL) {
        qDebug() << "Info URL:" << *infoURL;
    }
    
    // 测试版本 URL
    ResourceAPI::VersionSearchArgs versionArgs;
    auto pack = std::make_shared<ModPlatform::IndexedPack>();
    pack->addonId = "worldedit";
    versionArgs.pack = pack;
    versionArgs.pluginLoaders = ModPlatform::PluginLoaderType::PAPER;
    
    auto versionsURL = api->getVersionsURL(versionArgs);
    if (versionsURL) {
        qDebug() << "Versions URL:" << *versionsURL;
    }
    
    qDebug() << "Modrinth API test completed";
}

void testHangarAPI()
{
    qDebug() << "\n=== Hangar API test removed ===";
    // Hangar API 已被移除
    qDebug() << "Skipping Hangar API test";
}

void testCustomAPI()
{
    qDebug() << "\n=== Testing Custom API ===";
    
    // 从字符串创建自定义 API
    QString customJS = R"(
        const api = {
            getSearchURL(args) {
                return `https://example.com/search?q=${args.search || ''}`;
            },
            
            getInfoURL(id) {
                return `https://example.com/project/${id}`;
            },
            
            getVersionsURL(args) {
                return `https://example.com/project/${args.addonId}/versions`;
            },
            
            loadIndexedPack(obj) {
                console.log('Loading pack:', obj.name);
                return {
                    name: obj.name || 'Unknown',
                    addonId: obj.id || '',
                    description: obj.description || '',
                    provider: 'CUSTOM'
                };
            },
            
            getSortingMethods() {
                return [
                    { name: 'relevance', displayName: 'Relevance' },
                    { name: 'popular', displayName: 'Popular' }
                ];
            },
            
            documentToArray(doc) {
                return doc.results || [];
            }
        };
        
        console.log('[CustomAPI] Initialized');
    )";
    
    auto api = JSResourceAPI::fromString(customJS, "CustomAPI");
    
    if (!api) {
        qWarning() << "Failed to create custom API";
        return;
    }
    
    // 测试自定义 API
    ResourceAPI::SearchArgs searchArgs;
    searchArgs.search = "test";
    
    auto searchURL = api->getSearchURL(searchArgs);
    if (searchURL) {
        qDebug() << "Custom Search URL:" << *searchURL;
    }
    
    auto infoURL = api->getInfoURL("test-project");
    if (infoURL) {
        qDebug() << "Custom Info URL:" << *infoURL;
    }
    
    qDebug() << "Custom API test completed";
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "JSResourceAPI Examples and Tests";
    qDebug() << "================================\n";
    
    testModrinthAPI();
    testHangarAPI();
    testCustomAPI();
    
    qDebug() << "\n=== All tests completed ===";
    
    return 0;
}
