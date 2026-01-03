#!/usr/bin/env python3
"""
Test script for Hangar Agent
"""
import sys
import os

# Add the 'demo' directory to path for imports
demo_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
if demo_dir not in sys.path:
    sys.path.insert(0, demo_dir)

import asyncio
from hangar_agent import SyncHangarAgent, SyncHangarDownloader


def test_api_connection():
    """Test if Hangar API is accessible"""
    print("=" * 60)
    print("Test 1: API Connection")
    print("=" * 60)

    agent = SyncHangarAgent()
    try:
        result = agent.list_plugins(limit=5)
        print(f"✅ API connected successfully!")
        print(f"   Total plugins: {result.count}")
        print(f"   Retrieved: {len(result.plugins)}")
        return True
    except Exception as e:
        print(f"❌ API connection failed: {e}")
        return False
    finally:
        agent.close()


def test_search():
    """Test search functionality"""
    print("\n" + "=" * 60)
    print("Test 2: Search Plugins")
    print("=" * 60)

    agent = SyncHangarAgent()
    try:
        # Search for economy plugins
        result = agent.search_plugins("economy", limit=10)
        print(f"✅ Search for 'economy': {result.count} results")

        for i, plugin in enumerate(result.plugins[:5], 1):
            stats = plugin.stats
            print(f"   {i}. {plugin.full_slug}")
            print(f"      {plugin.description[:50]}...")
            if stats:
                print(f"      ⭐ {stats.stars} | ⬇️ {stats.downloads}")

        return True
    except Exception as e:
        print(f"❌ Search failed: {e}")
        return False
    finally:
        agent.close()


def test_show_plugin():
    """Test showing plugin details"""
    print("\n" + "=" * 60)
    print("Test 3: Show Plugin Details")
    print("=" * 60)

    agent = SyncHangarAgent()
    try:
        # Test with a known plugin
        plugin = agent.show_plugin("AlessioDP", "Parties", include_versions=True)
        print(f"✅ Plugin details for '{plugin.name}'")
        print(f"   Full slug: {plugin.full_slug}")
        print(f"   URL: {plugin.url}")
        print(f"   Description: {plugin.description[:100]}...")
        print(f"   Category: {plugin.category}")

        if plugin.stats:
            print(f"   Stats: ⭐ {plugin.stats.stars} | ⬇️ {plugin.stats.downloads}")

        if plugin.supported_platforms:
            print(f"   Platforms: {', '.join(plugin.supported_platforms.keys())}")

        if plugin.versions:
            print(f"   Available versions: {len(plugin.versions)}")
            for v in plugin.versions[:3]:
                print(f"      - {v.name} ({', '.join(v.downloads.keys())})")

        return True
    except Exception as e:
        print(f"❌ Show plugin failed: {e}")
        return False
    finally:
        agent.close()


def test_get_versions():
    """Test getting plugin versions"""
    print("\n" + "=" * 60)
    print("Test 4: Get Plugin Versions")
    print("=" * 60)

    agent = SyncHangarAgent()
    try:
        versions = agent.get_versions("AlessioDP", "Parties")
        print(f"✅ Retrieved {len(versions)} versions")

        for v in versions[:5]:
            platforms = ', '.join(v.downloads.keys())
            print(f"   {v.name:<20} | {platforms}")

        if len(versions) > 5:
            print(f"   ... and {len(versions) - 5} more")

        return True
    except Exception as e:
        print(f"❌ Get versions failed: {e}")
        return False
    finally:
        agent.close()


def test_download():
    """Test downloading a plugin"""
    print("\n" + "=" * 60)
    print("Test 5: Download Plugin")
    print("=" * 60)

    downloader = SyncHangarDownloader()
    try:
        print("Getting version choices for Parties plugin...")

        choices = downloader.get_version_choices("AlessioDP", "Parties")
        if choices:
            print(f"✅ Found {len(choices)} versions")
            latest = choices[0]
            print(f"   Latest: {latest['name']} - Platforms: {latest['platforms']}")

        # Test download with actual download
        print("\nAttempting to download Parties 3.2.14...")
        result = downloader.download_by_version("AlessioDP", "Parties", "3.2.14")
        print(f"✅ Downloaded successfully!")
        print(f"   File: {result['file_path']}")
        print(f"   Version: {result['version']}")

        return True

    except Exception as e:
        print(f"❌ Download test failed: {e}")
        return False
    finally:
        downloader.close()


def main():
    """Run all tests"""
    print("\n" + "=" * 60)
    print("    Hangar Agent - Test Suite")
    print("=" * 60 + "\n")

    tests = [
        ("API Connection", test_api_connection),
        ("Search Plugins", test_search),
        ("Show Plugin Details", test_show_plugin),
        ("Get Plugin Versions", test_get_versions),
        ("Download Test", test_download),
    ]

    results = []
    for name, test_func in tests:
        try:
            passed = test_func()
            results.append((name, passed))
        except Exception as e:
            print(f"❌ {name} crashed: {e}")
            results.append((name, False))

    # Summary
    print("\n" + "=" * 60)
    print("    Test Summary")
    print("=" * 60)

    passed = sum(1 for _, p in results if p)
    total = len(results)

    for name, p in results:
        status = "✅ PASS" if p else "❌ FAIL"
        print(f"   {status}: {name}")

    print(f"\nTotal: {passed}/{total} tests passed")

    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
