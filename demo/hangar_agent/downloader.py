"""
Version selection and download utilities for Hangar plugins
"""
import asyncio
import re
from typing import List, Optional, Dict, Any
from packaging import version as pkg_version

from .api_client import HangarAPIClient
from .models import HangarPlugin, HangarVersion


class VersionSelector:
    """Handles version selection logic for plugins"""

    # Known Minecraft version patterns
    MINECRAFT_VERSION_PATTERN = re.compile(
        r"^(?:1\.(?:8|9|10|11|12|13|14|15|16|17|18|19|20|21)|"
        r"(?:1\.(?:8|9|10|11|12|13|14|15|16|17|18|19|20|21)\.\d+))"
    )

    # Common platform identifiers
    PLATFORMS = ["PAPER", "VELOCITY", "WATERFALL", "BUKKIT", "SPIGOT"]

    @staticmethod
    def parse_version_string(v: str) -> tuple:
        """Parse version string into comparable tuple"""
        # Extract Minecraft version if present
        mc_match = VersionSelector.MINECRAFT_VERSION_PATTERN.search(v)
        if mc_match:
            mc_ver = mc_match.group()
            remainder = v[len(mc_ver):].strip().strip("-")
            return (mc_ver, remainder)
        return (v, "")

    @staticmethod
    def sort_versions(versions: List[str], reverse: bool = True) -> List[str]:
        """Sort version strings, newest first by default"""
        def version_key(v: str):
            parts = VersionSelector.parse_version_string(v)
            return parts

        return sorted(versions, key=version_key, reverse=reverse)

    @classmethod
    def filter_by_minecraft_version(
        cls,
        versions: List[HangarVersion],
        mc_version: str
    ) -> List[HangarVersion]:
        """
        Filter versions compatible with a specific Minecraft version

        Args:
            versions: List of plugin versions
            mc_version: Minecraft version (e.g., "1.20.4")
        """
        filtered = []
        for v in versions:
            # Check platform dependencies for the MC version
            for platform_versions in v.platform_dependencies.values():
                if mc_version in platform_versions:
                    filtered.append(v)
                    break
        return filtered

    @classmethod
    def filter_by_platform(
        cls,
        versions: List[HangarVersion],
        platform: str
    ) -> List[HangarVersion]:
        """
        Filter versions for a specific platform

        Args:
            versions: List of plugin versions
            platform: Platform name (PAPER, VELOCITY, etc.)
        """
        platform_upper = platform.upper()
        return [
            v for v in versions
            if platform_upper in v.downloads
        ]

    @classmethod
    def get_recommended_version(
        cls,
        versions: List[HangarVersion],
        mc_version: Optional[str] = None,
        platform: Optional[str] = None
    ) -> Optional[HangarVersion]:
        """
        Get the recommended version based on filters

        Prefers:
        1. Latest version matching criteria
        2. Stable (non-prerelease) versions
        """
        filtered = versions

        if mc_version:
            filtered = cls.filter_by_minecraft_version(filtered, mc_version)

        if platform:
            filtered = cls.filter_by_platform(filtered, platform)

        if not filtered:
            return None

        # Sort by name (which is typically semver-like)
        try:
            sorted_versions = sorted(
                filtered,
                key=lambda v: pkg_version.parse(v.name) if _is_version(v.name) else pkg_version.parse("0"),
                reverse=True
            )
        except Exception:
            # Fallback to string sorting
            sorted_versions = sorted(filtered, key=lambda v: v.name, reverse=True)

        return sorted_versions[0] if sorted_versions else None

    @staticmethod
    def list_available_platforms(versions: List[HangarVersion]) -> set:
        """List all unique platforms across versions"""
        platforms = set()
        for v in versions:
            platforms.update(v.downloads.keys())
        return platforms

    @staticmethod
    def list_available_mc_versions(versions: List[HangarVersion]) -> set:
        """List all unique Minecraft versions across versions"""
        mc_versions = set()
        for v in versions:
            for platform_versions in v.platform_dependencies.values():
                mc_versions.update(platform_versions)
        return mc_versions


def _is_version(v: str) -> bool:
    """Check if string looks like a version number"""
    try:
        pkg_version.parse(v)
        return True
    except Exception:
        return False


class HangarDownloader:
    """Handles downloading of Hangar plugins with version selection"""

    def __init__(self, client: Optional[HangarAPIClient] = None):
        self.client = client or HangarAPIClient()
        self.selector = VersionSelector()

    async def download_latest(
        self,
        owner: str,
        slug: str,
        output_dir: str = ".",
        mc_version: Optional[str] = None,
        platform: str = "PAPER"
    ) -> Dict[str, Any]:
        """
        Download the latest compatible version of a plugin

        Returns dict with download info
        """
        # Get all versions
        versions = await self.client.get_plugin_versions(owner, slug)

        if not versions:
            raise ValueError(f"No versions found for plugin {owner}/{slug}")

        # Find recommended version
        recommended = self.selector.get_recommended_version(
            versions,
            mc_version=mc_version,
            platform=platform
        )

        if not recommended:
            # Fall back to latest version
            recommended = versions[0]

        # Download
        output_path = await self.client.download_plugin(
            owner,
            slug,
            recommended.name,
            platform,
            output_dir
        )

        return {
            "owner": owner,
            "slug": slug,
            "name": self.client.get_plugin(owner, slug) if False else f"{owner}/{slug}",
            "version": recommended.name,
            "file_path": output_path,
            "platforms": recommended.platforms,
            "dependencies": list(recommended.plugin_dependencies.keys())
        }

    async def download_by_version(
        self,
        owner: str,
        slug: str,
        version_name: str,
        platform: str = "PAPER",
        output_dir: str = "."
    ) -> Dict[str, Any]:
        """Download a specific version of a plugin"""
        output_path = await self.client.download_plugin(
            owner,
            slug,
            version_name,
            platform,
            output_dir
        )

        # Get version info
        versions = await self.client.get_plugin_versions(owner, slug)
        version_info = next(
            (v for v in versions if v.name == version_name),
            None
        )

        return {
            "owner": owner,
            "slug": slug,
            "version": version_name,
            "file_path": output_path,
            "platforms": version_info.platforms if version_info else [],
            "dependencies": list(version_info.plugin_dependencies.keys()) if version_info else []
        }

    async def get_version_choices(
        self,
        owner: str,
        slug: str,
        mc_version: Optional[str] = None,
        platform: Optional[str] = None
    ) -> List[Dict[str, Any]]:
        """
        Get available version choices for interactive selection
        """
        versions = await self.client.get_plugin_versions(owner, slug)

        choices = []
        for v in versions:
            # Skip if doesn't match filters
            if mc_version:
                if mc_version not in v.minecraft_versions:
                    continue

            if platform:
                if platform.upper() not in v.downloads:
                    continue

            choices.append({
                "name": v.name,
                "created_at": v.created_at,
                "platforms": v.platforms,
                "dependencies": list(v.plugin_dependencies.keys()),
                "file_size_mb": round(
                    v.downloads.get("PAPER", v.downloads.get(list(v.downloads.keys())[0] if v.downloads else {})).size_bytes / 1024 / 1024, 2
                ) if v.downloads else None
            })

        return choices

    async def close(self):
        await self.client.close()


# Convenience class for sync usage
class SyncHangarDownloader:
    """Synchronous wrapper for HangarDownloader"""

    def __init__(self):
        self._downloader = HangarDownloader()
        self._loop = asyncio.new_event_loop()

    def _run(self, coro):
        return self._loop.run_until_complete(coro)

    def download_latest(self, owner: str, slug: str, output_dir: str = ".", **kwargs):
        return self._run(
            self._downloader.download_latest(owner, slug, output_dir, **kwargs)
        )

    def download_by_version(self, owner: str, slug: str, version: str, output_dir: str = ".", **kwargs):
        return self._run(
            self._downloader.download_by_version(owner, slug, version, output_dir, **kwargs)
        )

    def get_version_choices(self, owner: str, slug: str, **kwargs):
        return self._run(self._downloader.get_version_choices(owner, slug, **kwargs))

    def close(self):
        self._run(self._downloader.close())
        self._loop.close()
