"""
Hangar API Client for PaperMC Plugin Repository
API Documentation: https://hangar.papermc.io/api/v1

Endpoints:
- GET /api/v1/projects - List/search plugins
- GET /api/v1/projects/{owner}/{slug} - Get plugin details
- GET /api/v1/projects/{owner}/{slug}/versions - Get plugin versions
"""
import aiohttp
import asyncio
from typing import Optional, List, Dict, Any
from urllib.parse import quote

from .models import HangarPlugin, HangarVersion, PaginatedResponse, HangarPagination


class HangarAPIClient:
    """Async client for Hangar API"""

    BASE_URL = "https://hangar.papermc.io/api/v1"

    def __init__(self, session: Optional[aiohttp.ClientSession] = None):
        self._session = session
        self._owns_session = session is None

    async def _get_session(self) -> aiohttp.ClientSession:
        """Get or create aiohttp session"""
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                headers={
                    "User-Agent": "HangarAgent/1.0",
                    "Accept": "application/json"
                }
            )
        return self._session

    async def close(self):
        """Close the session if we own it"""
        if self._owns_session and self._session and not self._session.closed:
            await self._session.close()

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        await self.close()

    async def _request(self, endpoint: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        """Make a GET request to the API"""
        session = await self._get_session()
        url = f"{self.BASE_URL}{endpoint}"

        async with session.get(url, params=params) as response:
            response.raise_for_status()
            return await response.json()

    async def get_plugins(
        self,
        limit: int = 50,
        offset: int = 0,
        query: Optional[str] = None
    ) -> PaginatedResponse:
        """
        Get list of plugins with optional search

        Args:
            limit: Number of results per page (max 100)
            offset: Pagination offset
            query: Optional search query
        """
        params = {"limit": min(limit, 100), "offset": offset}
        if query:
            params["q"] = query

        data = await self._request("/projects", params)

        pagination = HangarPagination.from_dict(data.get("pagination", {}))
        plugins = [HangarPlugin.from_list_dict(p) for p in data.get("result", [])]

        return PaginatedResponse(plugins=plugins, pagination=pagination)

    async def search_plugins(
        self,
        query: str,
        limit: int = 50,
        offset: int = 0
    ) -> PaginatedResponse:
        """
        Search for plugins

        Args:
            query: Search query string
            limit: Number of results
            offset: Pagination offset
        """
        return await self.get_plugins(limit=limit, offset=offset, query=query)

    async def get_plugin(self, owner: str, slug: str) -> HangarPlugin:
        """
        Get detailed information about a specific plugin

        Args:
            owner: Plugin owner (username)
            slug: Plugin slug (URL-friendly name)
        """
        data = await self._request(f"/projects/{owner}/{slug}")
        return HangarPlugin.from_detail_dict(data)

    async def get_plugin_by_name(self, name: str) -> HangarPlugin:
        """
        Get plugin by name (looks up namespace from list)

        Args:
            name: Plugin name to find
        """
        # First, search for the plugin
        result = await self.get_plugins(limit=10, query=name)
        for plugin in result.plugins:
            if plugin.name.lower() == name.lower():
                return plugin
            if plugin.slug.lower() == name.lower():
                return plugin
        # If not found, raise error
        raise ValueError(f"Plugin '{name}' not found")

    async def get_plugin_versions(
        self,
        owner: str,
        slug: str,
        limit: int = 50
    ) -> List[HangarVersion]:
        """
        Get available versions for a plugin

        Args:
            owner: Plugin owner
            slug: Plugin slug
            limit: Max number of versions
        """
        params = {"limit": min(limit, 100)}
        data = await self._request(f"/projects/{owner}/{slug}/versions", params)

        versions = [HangarVersion.from_dict(v) for v in data.get("result", [])]
        return versions

    async def get_download_url(
        self,
        owner: str,
        slug: str,
        version_name: str,
        platform: str = "PAPER"
    ) -> str:
        """
        Get download URL for a specific version and platform

        Args:
            owner: Plugin owner
            slug: Plugin slug
            version_name: Version name (e.g., "1.0.0")
            platform: Platform (PAPER, VELOCITY, WATERFALL, etc.)
        """
        data = await self._request(f"/projects/{owner}/{slug}/versions", {"limit": 100})

        for v in data.get("result", []):
            if v.get("name") == version_name:
                downloads = v.get("downloads", {})
                if platform in downloads:
                    return downloads[platform].get("downloadUrl", "")
                # Fallback to first available platform
                if downloads:
                    return next(iter(downloads.values())).get("downloadUrl", "")

        raise ValueError(f"Version '{version_name}' not found for {owner}/{slug}")

    async def download_plugin(
        self,
        owner: str,
        slug: str,
        version_name: str,
        platform: str = "PAPER",
        output_dir: str = "."
    ) -> str:
        """
        Download a specific version of a plugin

        Args:
            owner: Plugin owner
            slug: Plugin slug
            version_name: Version name
            platform: Platform
            output_dir: Directory to save the plugin

        Returns:
            Path to the downloaded file
        """
        import os
        from pathlib import Path

        session = await self._get_session()

        # Get version data to find download URL
        params = {"limit": 100}
        data = await self._request(f"/projects/{owner}/{slug}/versions", params)

        download_url = None
        file_name = f"{slug}-{version_name}.jar"

        for v in data.get("result", []):
            if v.get("name") == version_name:
                downloads = v.get("downloads", {})
                if platform in downloads:
                    download_url = downloads[platform].get("downloadUrl")
                    file_info = downloads[platform].get("fileInfo", {})
                    file_name = file_info.get("name", file_name)
                elif downloads:
                    first_platform = next(iter(downloads.keys()))
                    download_url = downloads[first_platform].get("downloadUrl")
                    file_info = downloads[first_platform].get("fileInfo", {})
                    file_name = file_info.get("name", file_name)
                break

        if not download_url:
            raise ValueError(f"No download URL found for {owner}/{slug} v{version_name}")

        output_path = Path(output_dir) / file_name

        # Download the file
        async with session.get(download_url) as response:
            response.raise_for_status()
            content = await response.read()

            # Ensure output directory exists
            output_path.parent.mkdir(parents=True, exist_ok=True)

            # Write to file
            output_path.write_bytes(content)

        return str(output_path)

    async def download_latest(
        self,
        owner: str,
        slug: str,
        platform: str = "PAPER",
        output_dir: str = "."
    ) -> str:
        """
        Download the latest version of a plugin

        Args:
            owner: Plugin owner
            slug: Plugin slug
            platform: Platform
            output_dir: Directory to save the plugin

        Returns:
            Path to the downloaded file
        """
        versions = await self.get_plugin_versions(owner, slug)
        if not versions:
            raise ValueError(f"No versions found for {owner}/{slug}")

        latest_version = versions[0].name
        return await self.download_plugin(owner, slug, latest_version, platform, output_dir)


# Convenience function to create client
def create_client() -> HangarAPIClient:
    """Create a new Hangar API client"""
    return HangarAPIClient()


# Sync wrapper for convenience
class SyncHangarClient:
    """Synchronous wrapper around HangarAPIClient"""

    def __init__(self):
        self._client = create_client()
        self._loop = asyncio.new_event_loop()

    def _run_async(self, coro):
        """Run an async coroutine in the event loop"""
        try:
            return self._loop.run_until_complete(coro)
        except Exception as e:
            self._loop.close()
            raise e

    def get_plugins(self, **kwargs) -> PaginatedResponse:
        return self._run_async(self._client.get_plugins(**kwargs))

    def search_plugins(self, query: str, **kwargs) -> PaginatedResponse:
        return self._run_async(self._client.search_plugins(query, **kwargs))

    def get_plugin(self, owner: str, slug: str) -> HangarPlugin:
        return self._run_async(self._client.get_plugin(owner, slug))

    def get_plugin_by_name(self, name: str) -> HangarPlugin:
        return self._run_async(self._client.get_plugin_by_name(name))

    def get_plugin_versions(self, owner: str, slug: str, **kwargs) -> List[HangarVersion]:
        return self._run_async(self._client.get_plugin_versions(owner, slug, **kwargs))

    def download_plugin(self, owner: str, slug: str, version: str, **kwargs) -> str:
        return self._run_async(
            self._client.download_plugin(owner, slug, version, **kwargs)
        )

    def download_latest(self, owner: str, slug: str, **kwargs) -> str:
        return self._run_async(
            self._client.download_latest(owner, slug, **kwargs)
        )

    def close(self):
        self._run_async(self._client.close())
        self._loop.close()
