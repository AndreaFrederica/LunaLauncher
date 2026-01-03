"""
Hangar Plugin Agent - Download plugins from PaperMC's Hangar repository

Quick usage:
    from hangar_agent import SyncHangarAgent

    # Sync usage
    agent = SyncHangarAgent()
    result = agent.search_plugins("economy")
    agent.show_plugin("AlessioDP", "Parties")
    agent.download_plugin("AlessioDP", "Parties")
    agent.interactive()

API Structure:
- List plugins: GET /api/v1/projects
- Get plugin: GET /api/v1/projects/{owner}/{slug}
- Get versions: GET /api/v1/projects/{owner}/{slug}/versions
"""

from .api_client import HangarAPIClient, create_client, SyncHangarClient
from .downloader import HangarDownloader, SyncHangarDownloader, VersionSelector
from .agent import HangarAgent, SyncHangarAgent, AgentConfig
from .models import HangarPlugin, HangarVersion, HangarStats, PaginatedResponse

__all__ = [
    # API Client
    "HangarAPIClient",
    "SyncHangarClient",
    "create_client",
    # Downloader
    "HangarDownloader",
    "SyncHangarDownloader",
    "VersionSelector",
    # Agent
    "HangarAgent",
    "SyncHangarAgent",
    "AgentConfig",
    # Models
    "HangarPlugin",
    "HangarVersion",
    "HangarStats",
    "PaginatedResponse",
]

__version__ = "1.0.0"
