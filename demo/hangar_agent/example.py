#!/usr/bin/env python3
"""
Example usage of Hangar Plugin Agent
"""
from hangar_agent import SyncHangarAgent, SyncHangarDownloader


def example_search():
    """Example: Search for economy plugins"""
    print("=" * 60)
    print("Example: Search for economy plugins")
    print("=" * 60)

    agent = SyncHangarAgent()
    result = agent.search_plugins("economy", limit=5)

    print(f"\nFound {result.count} plugins matching 'economy':\n")
    for i, plugin in enumerate(result.plugins, 1):
        print(f"{i}. {plugin.name}")
        print(f"   {plugin.description[:60]}...")
        if plugin.stats:
            print(f"   ⭐ {plugin.stats.stars} | ⬇️ {plugin.stats.downloads}")
        print()

    agent.close()


def example_show_plugin():
    """Example: Show detailed plugin info"""
    print("=" * 60)
    print("Example: Show plugin details - LuckPerms")
    print("=" * 60)

    agent = SyncHangarAgent()
    plugin = agent.show_plugin("LuckPerms", include_versions=True)

    print(f"\n{plugin.name}")
    print(f"Slug: {plugin.slug}")
    print(f"Authors: {', '.join(a.name for a in plugin.authors)}")
    print(f"\n{plugin.description}")
    print(f"\nLatest version: {plugin.latest_version}")

    if plugin.versions:
        print("\nRecent versions:")
        for v in plugin.versions[:5]:
            print(f"  - {v.name} ({', '.join(v.plugin_platforms[:2])})")

    agent.close()


def example_download():
    """Example: Download a plugin"""
    print("=" * 60)
    print("Example: Download a plugin")
    print("=" * 60)

    downloader = SyncHangarDownloader()

    # Get version choices first
    print("\nAvailable versions for LuckPerms:")
    choices = downloader.get_version_choices("LuckPerms", mc_version="1.20.4")
    for c in choices[:5]:
        print(f"  {c['name']} - {c['platforms']}")

    # Download latest compatible version
    print("\nDownloading LuckPerms (latest for 1.20.4)...")
    result = downloader.download_latest("LuckPerms", mc_version="1.20.4")

    print(f"\n✅ Downloaded to: {result['file_path']}")
    print(f"   Version: {result['version']}")

    downloader.close()


def example_interactive():
    """Example: Run interactive mode"""
    print("=" * 60)
    print("Starting Interactive Mode")
    print("=" * 60)
    print("\nTry commands like:")
    print("  list --limit 10")
    print("  search chat")
    print("  show Essentials")
    print("  versions Essentials --all")
    print("  download ViaVersion --mc 1.20.4")
    print("  set mc 1.20.4")
    print("  quit")
    print()

    agent = SyncHangarAgent()
    agent.interactive()
    agent.close()


if __name__ == "__main__":
    import sys

    examples = {
        "1": ("Search Plugins", example_search),
        "2": ("Show Plugin Details", example_show_plugin),
        "3": ("Download Plugin", example_download),
        "4": ("Interactive Mode", example_interactive),
    }

    print("Hangar Plugin Agent - Example Usage\n")
    print("Select an example:")
    for key, (name, _) in examples.items():
        print(f"  {key}. {name}")
    print("  q. Quit")

    choice = input("\nChoice: ").strip().lower()

    if choice in examples:
        examples[choice][1]()
    elif choice == "q":
        print("Goodbye!")
    else:
        print("Invalid choice")
