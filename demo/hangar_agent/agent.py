"""
Hangar Plugin Agent - Interactive CLI for browsing and downloading Hangar plugins

API Structure:
- List plugins: GET /api/v1/projects
- Get plugin: GET /api/v1/projects/{owner}/{slug}
- Get versions: GET /api/v1/projects/{owner}/{slug}/versions
"""
import asyncio
import sys
from typing import Optional, List, Dict, Any
from dataclasses import dataclass
from enum import Enum

from .api_client import HangarAPIClient
from .downloader import HangarDownloader, VersionSelector
from .models import HangarPlugin, HangarVersion, PaginatedResponse


class OutputFormat(Enum):
    """Output format options"""
    TEXT = "text"
    JSON = "json"
    COMPACT = "compact"


@dataclass
class AgentConfig:
    """Configuration for the Hangar agent"""
    default_limit: int = 20
    output_format: OutputFormat = OutputFormat.TEXT
    minecraft_version: Optional[str] = None
    platform: str = "PAPER"
    output_dir: str = "."


class HangarAgent:
    """
    Interactive agent for browsing and downloading Hangar plugins.

    Usage:
        agent = HangarAgent()
        agent.list_plugins()
        agent.search("economy")
        agent.show_plugin("LuckPerms")  # Uses owner/slug format
        agent.download("LuckPerms/LuckPerms", version="5.4.0")
    """

    def __init__(self, config: Optional[AgentConfig] = None):
        self.config = config or AgentConfig()
        self.client = HangarAPIClient()
        self.downloader = HangarDownloader(self.client)

    async def _ensure_client(self):
        """Ensure client is initialized"""
        pass  # Client is lazily initialized

    async def close(self):
        """Clean up resources"""
        await self.client.close()

    def format_plugin_compact(self, plugin: HangarPlugin) -> str:
        """Format plugin info in compact single-line format"""
        stats = plugin.stats
        stats_str = ""
        if stats:
            stats_str = f"⭐{stats.stars} ⬇️{stats.downloads}"

        name = plugin.name
        desc = plugin.description[:50] if plugin.description else ""
        if len(plugin.description or "") > 50:
            desc += "..."

        return f"{name:<25} | {desc:<50} | {stats_str}"

    def format_plugin_detailed(self, plugin: HangarPlugin) -> str:
        """Format plugin info in detailed multi-line format"""
        lines = []

        # Header with name
        lines.append(f"📦 {plugin.name}")
        lines.append(f"   Full slug: {plugin.full_slug}")
        lines.append(f"   URL: {plugin.url}")

        if plugin.avatar_url:
            lines.append(f"   Icon: {plugin.avatar_url}")

        # Description
        if plugin.description:
            lines.append(f"\n   Description:")
            desc = plugin.description
            for line in desc.split("\n"):
                lines.append(f"      {line}")

        # Category
        if plugin.category:
            lines.append(f"\n   Category: {plugin.category}")

        # Stats
        if plugin.stats:
            lines.append(f"\n   Stats:")
            lines.append(f"      ⭐ Stars: {plugin.stats.stars}")
            lines.append(f"      ⬇️ Downloads: {plugin.stats.downloads}")
            lines.append(f"      👁️ Views: {plugin.stats.views}")

        # Platforms
        if plugin.supported_platforms:
            lines.append(f"\n   Supported Platforms:")
            for platform, versions in plugin.supported_platforms.items():
                if len(versions) <= 3:
                    lines.append(f"      {platform}: {', '.join(versions)}")
                else:
                    lines.append(f"      {platform}: {versions[0]} - {versions[-1]} ({len(versions)} versions)")

        # Last updated
        lines.append(f"\n   Last Updated: {plugin.last_updated}")

        return "\n".join(lines)

    def format_plugins_list(self, plugins: List[HangarPlugin], compact: bool = True) -> str:
        """Format a list of plugins"""
        if not plugins:
            return "No plugins found."

        if compact:
            header = f"{'Name':<25} | {'Description':<52} | {'Stats'}"
            separator = "-" * len(header)
            lines = [header, separator]
            for p in plugins:
                lines.append(self.format_plugin_compact(p))
        else:
            lines = []
            for i, p in enumerate(plugins, 1):
                lines.append(f"{i}. {self.format_plugin_detailed(p)}")
                lines.append("")

        return "\n".join(lines)

    def format_version(self, version: HangarVersion) -> str:
        """Format a single version"""
        platforms = ", ".join(version.downloads.keys())
        deps = f" | Deps: {', '.join(version.plugin_dependencies.keys())}" if version.plugin_dependencies else ""
        channel = version.channel.name if version.channel else "Unknown"
        return f"  {version.name:<20} | {platforms:<25} | {channel:<10}{deps}"

    async def list_plugins(
        self,
        limit: Optional[int] = None,
        offset: int = 0,
        query: Optional[str] = None
    ) -> PaginatedResponse:
        """List plugins with optional search"""
        await self._ensure_client()
        limit = limit or self.config.default_limit

        result = await self.client.get_plugins(limit=limit, offset=offset, query=query)
        return result

    async def search_plugins(
        self,
        query: str,
        limit: Optional[int] = None
    ) -> PaginatedResponse:
        """Search for plugins"""
        await self._ensure_client()
        limit = limit or self.config.default_limit

        result = await self.client.search_plugins(query, limit=limit)
        return result

    async def show_plugin(
        self,
        owner: str,
        slug: str,
        include_versions: bool = False
    ) -> HangarPlugin:
        """Get detailed information about a plugin"""
        await self._ensure_client()

        plugin = await self.client.get_plugin(owner, slug)

        if include_versions:
            versions = await self.client.get_plugin_versions(owner, slug)
            plugin.versions = versions

        return plugin

    async def show_plugin_by_name(self, name: str, include_versions: bool = False) -> HangarPlugin:
        """Get plugin by name (will search for it)"""
        plugin = await self.client.get_plugin_by_name(name)
        if include_versions:
            versions = await self.client.get_plugin_versions(plugin.owner, plugin.slug)
            plugin.versions = versions
        return plugin

    async def get_versions(
        self,
        owner: str,
        slug: str,
        show_all: bool = False
    ) -> List[HangarVersion]:
        """Get available versions for a plugin"""
        await self._ensure_client()

        versions = await self.client.get_plugin_versions(owner, slug)

        # Filter by MC version if configured
        if self.config.minecraft_version and not show_all:
            versions = VersionSelector.filter_by_minecraft_version(
                versions,
                self.config.minecraft_version
            )

        # Filter by platform if configured
        if self.config.platform and not show_all:
            versions = VersionSelector.filter_by_platform(
                versions,
                self.config.platform
            )

        return versions

    async def download_plugin(
        self,
        owner: str,
        slug: str,
        version: Optional[str] = None,
        platform: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Download a plugin

        Args:
            owner: Plugin owner
            slug: Plugin slug
            version: Specific version, or None for latest
            platform: Platform to download
        """
        await self._ensure_client()

        platform = platform or self.config.platform
        output_dir = self.config.output_dir

        if version:
            result = await self.downloader.download_by_version(
                owner, slug, version, platform, output_dir
            )
        else:
            result = await self.downloader.download_latest(
                owner, slug, output_dir,
                mc_version=self.config.minecraft_version,
                platform=platform
            )

        return result

    async def get_version_choices(
        self,
        owner: str,
        slug: str
    ) -> List[Dict[str, Any]]:
        """Get version choices for interactive selection"""
        return await self.downloader.get_version_choices(
            owner, slug,
            mc_version=self.config.minecraft_version,
            platform=self.config.platform
        )

    # =====================
    # CLI Methods
    # =====================

    async def cli_list(self, args: List[str]):
        """Handle 'list' CLI command"""
        limit = 20
        offset = 0
        query = None

        i = 0
        while i < len(args):
            if args[i] == "--limit" and i + 1 < len(args):
                limit = int(args[i + 1])
                i += 2
            elif args[i] == "--offset" and i + 1 < len(args):
                offset = int(args[i + 1])
                i += 2
            elif args[i] == "--search" and i + 1 < len(args):
                query = args[i + 1]
                i += 2
            else:
                i += 1

        result = await self.list_plugins(limit=limit, offset=offset, query=query)
        search_str = f" matching '{query}'" if query else ""
        print(f"\nFound {result.count} plugins{search_str} (showing {len(result.plugins)}):\n")
        print(self.format_plugins_list(result.plugins))
        print(f"\nPage: {result.offset // limit + 1}")
        if result.has_next:
            print(f"Next page: offset={result.next_offset}")
        if result.has_prev:
            print(f"Prev page: offset={result.prev_offset}")

        return result

    async def cli_search(self, args: List[str]):
        """Handle 'search' CLI command"""
        if not args:
            print("Usage: search <query> [--limit N]")
            return

        query = args[0]
        limit = 20

        i = 1
        while i < len(args):
            if args[i] == "--limit" and i + 1 < len(args):
                limit = int(args[i + 1])
                i += 2
            else:
                i += 1

        result = await self.search_plugins(query, limit=limit)
        print(f"\nSearch results for '{query}': {result.count} plugins found\n")
        print(self.format_plugins_list(result.plugins))

        return result

    async def cli_show(self, args: List[str]):
        """Handle 'show' CLI command"""
        if not args:
            print("Usage: show <owner>/<slug> [--versions]")
            return

        # Parse owner/slug
        plugin_ref = args[0]
        if "/" not in plugin_ref:
            # Try to find by name
            try:
                plugin = await self.show_plugin_by_name(plugin_ref, include_versions="--versions" in args)
                print(f"\nFound plugin: {plugin.full_slug}")
            except ValueError as e:
                print(f"Error: {e}")
                print("Note: Plugin name should be in format 'owner/slug' (e.g., 'AlessioDP/Parties')")
                return
        else:
            parts = plugin_ref.split("/")
            owner, slug = parts[0], parts[1]
            show_versions = "--versions" in args or "-v" in args
            plugin = await self.show_plugin(owner, slug, include_versions=show_versions)

        print(f"\n{self.format_plugin_detailed(plugin)}")

        if plugin.versions:
            print("\nAvailable Versions:")
            print("-" * 60)
            for v in plugin.versions[:20]:
                print(self.format_version(v))
            if len(plugin.versions) > 20:
                print(f"  ... and {len(plugin.versions) - 20} more versions")

        return plugin

    async def cli_versions(self, args: List[str]):
        """Handle 'versions' CLI command"""
        if not args:
            print("Usage: versions <owner>/<slug> [--all]")
            return

        plugin_ref = args[0]
        if "/" not in plugin_ref:
            print("Error: Plugin reference should be in format 'owner/slug' (e.g., 'AlessioDP/Parties')")
            return

        parts = plugin_ref.split("/")
        owner, slug = parts[0], parts[1]
        show_all = "--all" in args

        versions = await self.get_versions(owner, slug, show_all=show_all)
        print(f"\nVersions for '{owner}/{slug}':\n")

        if not versions:
            print("No versions match your current filters.")
            print(f"Configured: MC={self.config.minecraft_version}, Platform={self.config.platform}")
            if not show_all:
                print("Use --all to see all versions.")

            # Show available platforms
            all_versions = await self.client.get_plugin_versions(owner, slug)
            platforms = VersionSelector.list_available_platforms(all_versions)
            if platforms:
                print(f"\nAvailable platforms: {', '.join(platforms)}")

            return

        for v in versions:
            print(self.format_version(v))

        if not show_all:
            print(f"\n({len(versions)} versions match your filters)")
            print("Use --all to see all versions")

    async def cli_download(self, args: List[str]):
        """Handle 'download' CLI command"""
        if not args:
            print("Usage: download <owner>/<slug> [--version VERSION] [--mc VERSION] [--platform PLATFORM] [--output DIR]")
            return

        plugin_ref = args[0]
        if "/" not in plugin_ref:
            print("Error: Plugin reference should be in format 'owner/slug'")
            return

        parts = plugin_ref.split("/")
        owner, slug = parts[0], parts[1]
        version = None
        mc_version = None
        platform = None
        output_dir = self.config.output_dir

        i = 1
        while i < len(args):
            if args[i] == "--version" and i + 1 < len(args):
                version = args[i + 1]
                i += 2
            elif args[i] == "--mc" and i + 1 < len(args):
                mc_version = args[i + 1]
                i += 2
            elif args[i] == "--platform" and i + 1 < len(args):
                platform = args[i + 1]
                i += 2
            elif args[i] == "--output" and i + 1 < len(args):
                output_dir = args[i + 1]
                i += 2
            else:
                i += 1

        print(f"\nDownloading {owner}/{slug}...")

        if version:
            print(f"  Version: {version}")
        else:
            print(f"  MC Version: {mc_version or 'any'}")
            print(f"  Platform: {platform or 'PAPER'}")

        result = await self.download_plugin(
            owner, slug,
            version=version,
            platform=platform
        )

        print(f"\n✅ Downloaded successfully!")
        print(f"   File: {result['file_path']}")
        print(f"   Version: {result['version']}")
        if result['dependencies']:
            print(f"   Dependencies: {', '.join(result['dependencies'])}")

        return result

    async def cli_interactive(self):
        """Run interactive mode"""
        print("""
╔══════════════════════════════════════════════════════════╗
║         Hangar Plugin Manager - Interactive Mode          ║
║  Type 'help' for commands, 'quit' to exit                ║
╚══════════════════════════════════════════════════════════╝
""")

        while True:
            try:
                cmd = input("hangar> ").strip()
            except (EOFError, KeyboardInterrupt):
                print("\nGoodbye!")
                break

            if not cmd:
                continue

            parts = cmd.split()
            action = parts[0].lower()

            if action in ["quit", "exit", "q"]:
                print("Goodbye!")
                break
            elif action == "help":
                print("""
Available commands:
  list [--limit N] [--offset N] [--search QUERY] - List/search plugins
  search <query> [--limit N]                     - Search for plugins
  show <owner>/<slug> [--versions]               - Show plugin details
  versions <owner>/<slug> [--all]                - List plugin versions
  download <owner>/<slug> [options]              - Download a plugin
  set <key> <value>                              - Set config option
  help                                            - Show this help
  quit                                            - Exit

Options for download:
  --version VERSION    - Specific version to download
  --mc VERSION         - Minecraft version to match
  --platform PLATFORM  - Platform (PAPER, VELOCITY, etc.)
  --output DIR         - Output directory

Examples:
  list --limit 10
  search economy
  show AlessioDP/Parties --versions
  download AlessioDP/Parties --mc 1.20.4
  set mc 1.20.4
                """)
            elif action == "list":
                await self.cli_list(parts[1:])
            elif action == "search":
                await self.cli_search(parts[1:])
            elif action == "show":
                await self.cli_show(parts[1:])
            elif action == "versions":
                await self.cli_versions(parts[1:])
            elif action == "download":
                await self.cli_download(parts[1:])
            elif action == "set":
                if len(parts) >= 3:
                    key = parts[1].lower()
                    value = parts[2]
                    if key == "mc":
                        self.config.minecraft_version = value
                        print(f"Set Minecraft version to {value}")
                    elif key == "platform":
                        self.config.platform = value.upper()
                        print(f"Set platform to {value}")
                    elif key == "output":
                        self.config.output_dir = value
                        print(f"Set output directory to {value}")
                    else:
                        print(f"Unknown config key: {key}")
                else:
                    print("Usage: set <key> <value>")
                    print("Keys: mc, platform, output")
            else:
                print(f"Unknown command: {action}")
                print("Type 'help' for available commands")


# Synchronous wrapper for convenience
class SyncHangarAgent:
    """Synchronous wrapper for HangarAgent"""

    def __init__(self, config: Optional[AgentConfig] = None):
        self._agent = HangarAgent(config)
        self._loop = asyncio.new_event_loop()

    def _run(self, coro):
        return self._loop.run_until_complete(coro)

    def list_plugins(self, **kwargs):
        return self._run(self._agent.list_plugins(**kwargs))

    def search_plugins(self, query: str, **kwargs):
        return self._run(self._agent.search_plugins(query, **kwargs))

    def show_plugin(self, owner: str, slug: str, **kwargs):
        return self._run(self._agent.show_plugin(owner, slug, **kwargs))

    def show_plugin_by_name(self, name: str, **kwargs):
        return self._run(self._agent.show_plugin_by_name(name, **kwargs))

    def get_versions(self, owner: str, slug: str, **kwargs):
        return self._run(self._agent.get_versions(owner, slug, **kwargs))

    def download_plugin(self, owner: str, slug: str, **kwargs):
        return self._run(self._agent.download_plugin(owner, slug, **kwargs))

    def interactive(self):
        self._run(self._agent.cli_interactive())

    def close(self):
        self._run(self._agent.close())
        self._loop.close()


# Main entry point
def main():
    """Main entry point for CLI"""
    import argparse

    parser = argparse.ArgumentParser(description="Hangar Plugin Manager")
    parser.add_argument("--mc", help="Default Minecraft version")
    parser.add_argument("--platform", default="PAPER", help="Default platform")
    parser.add_argument("--output", default=".", help="Output directory")
    parser.add_argument("--interactive", "-i", action="store_true", help="Interactive mode")

    subparsers = parser.add_subparsers(dest="command", help="Commands")

    # list command
    list_parser = subparsers.add_parser("list", help="List plugins")
    list_parser.add_argument("--limit", type=int, default=20)
    list_parser.add_argument("--offset", type=int, default=0)
    list_parser.add_argument("--search", help="Search query")

    # search command
    search_parser = subparsers.add_parser("search", help="Search plugins")
    search_parser.add_argument("query")
    search_parser.add_argument("--limit", type=int, default=20)

    # show command
    show_parser = subparsers.add_parser("show", help="Show plugin details")
    show_parser.add_argument("plugin", help="Plugin in format owner/slug")
    show_parser.add_argument("--versions", action="store_true")

    # versions command
    versions_parser = subparsers.add_parser("versions", help="List plugin versions")
    versions_parser.add_argument("plugin", help="Plugin in format owner/slug")
    versions_parser.add_argument("--all", action="store_true")

    # download command
    download_parser = subparsers.add_parser("download", help="Download a plugin")
    download_parser.add_argument("plugin", help="Plugin in format owner/slug")
    download_parser.add_argument("--version")
    download_parser.add_argument("--mc")
    download_parser.add_argument("--platform")
    download_parser.add_argument("--output")

    args = parser.parse_args()

    # Create config
    config = AgentConfig(
        minecraft_version=args.mc,
        platform=args.platform.upper() if args.platform else "PAPER",
        output_dir=args.output
    )

    agent = SyncHangarAgent(config)

    try:
        if args.command == "list":
            result = agent.list_plugins(
                limit=args.limit,
                offset=args.offset,
                query=args.search
            )
            print(f"\nFound {result.count} plugins:\n")
            print(agent._agent.format_plugins_list(result.plugins))
        elif args.command == "search":
            result = agent.search_plugins(args.query, limit=args.limit)
            print(f"\nSearch results for '{args.query}':")
            print(agent._agent.format_plugins_list(result.plugins))
        elif args.command == "show":
            if "/" not in args.plugin:
                print("Error: Plugin should be in format 'owner/slug'")
            else:
                parts = args.plugin.split("/")
                plugin = agent.show_plugin(parts[0], parts[1], include_versions=args.versions)
                print(agent._agent.format_plugin_detailed(plugin))
        elif args.command == "versions":
            if "/" not in args.plugin:
                print("Error: Plugin should be in format 'owner/slug'")
            else:
                parts = args.plugin.split("/")
                versions = agent.get_versions(parts[0], parts[1], show_all=args.all)
                for v in versions:
                    print(agent._agent.format_version(v))
        elif args.command == "download":
            if "/" not in args.plugin:
                print("Error: Plugin should be in format 'owner/slug'")
            else:
                parts = args.plugin.split("/")
                result = agent.download_plugin(
                    parts[0], parts[1],
                    version=args.version,
                    platform=args.platform
                )
                print(f"Downloaded to: {result['file_path']}")
        elif args.interactive or not args.command:
            agent.interactive()
        else:
            parser.print_help()
    finally:
        agent.close()


if __name__ == "__main__":
    main()
