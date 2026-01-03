"""
Data models for Hangar API (PaperMC)
API Documentation: https://hangar.papermc.io/api/v1
"""
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any
from datetime import datetime


@dataclass
class HangarNamespace:
    """Plugin namespace (owner/slug)"""
    owner: str
    slug: str

    @classmethod
    def from_dict(cls, data: dict) -> "HangarNamespace":
        return cls(
            owner=data.get("owner", ""),
            slug=data.get("slug", "")
        )


@dataclass
class HangarStats:
    """Plugin statistics"""
    views: int = 0
    downloads: int = 0
    stars: int = 0
    recent_views: int = 0
    recent_downloads: int = 0
    watchers: int = 0

    @classmethod
    def from_dict(cls, data: dict) -> "HangarStats":
        return cls(
            views=data.get("views", 0),
            downloads=data.get("downloads", 0),
            stars=data.get("stars", 0),
            recent_views=data.get("recentViews", 0),
            recent_downloads=data.get("recentDownloads", 0),
            watchers=data.get("watchers", 0)
        )


@dataclass
class HangarVersionChannel:
    """Version release channel"""
    name: str
    description: Optional[str] = None
    color: str = "#000000"
    flags: List[str] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: dict) -> "HangarVersionChannel":
        return cls(
            name=data.get("name", ""),
            description=data.get("description"),
            color=data.get("color", "#000000"),
            flags=data.get("flags", [])
        )


@dataclass
class HangarVersionDownload:
    """Download info for a specific platform"""
    file_name: str
    size_bytes: int
    sha256_hash: str
    download_url: str
    external_url: Optional[str] = None

    @classmethod
    def from_dict(cls, data: dict) -> "HangarVersionDownload":
        file_info = data.get("fileInfo", {})
        return cls(
            file_name=file_info.get("name", ""),
            size_bytes=file_info.get("sizeBytes", 0),
            sha256_hash=file_info.get("sha256Hash", ""),
            download_url=data.get("downloadUrl", ""),
            external_url=data.get("externalUrl")
        )


@dataclass
class HangarVersion:
    """Plugin version information"""
    name: str
    created_at: str
    project_id: int
    version_id: int
    description: str = ""
    author: str = ""
    visibility: str = "public"
    review_state: str = ""
    channel: Optional[HangarVersionChannel] = None
    platform_downloads: Dict[str, int] = field(default_factory=dict)
    downloads: Dict[str, HangarVersionDownload] = field(default_factory=dict)
    plugin_dependencies: Dict[str, str] = field(default_factory=dict)
    platform_dependencies: Dict[str, List[str]] = field(default_factory=dict)
    platform_dependencies_formatted: Dict[str, List[str]] = field(default_factory=dict)
    member_names: List[str] = field(default_factory=list)
    total_downloads: int = 0

    @property
    def platforms(self) -> List[str]:
        """Get list of available platforms"""
        return list(self.downloads.keys())

    @property
    def minecraft_versions(self) -> List[str]:
        """Get list of supported Minecraft versions"""
        all_versions = []
        for versions in self.platform_dependencies.values():
            all_versions.extend(versions)
        return list(set(all_versions))

    @classmethod
    def from_dict(cls, data: dict) -> "HangarVersion":
        # Parse downloads
        downloads = {}
        downloads_data = data.get("downloads", {})
        for platform, platform_data in downloads_data.items():
            downloads[platform] = HangarVersionDownload.from_dict(platform_data)

        # Parse channel
        channel = None
        if "channel" in data:
            channel = HangarVersionChannel.from_dict(data["channel"])

        # Parse platform dependencies
        platform_deps = data.get("platformDependencies", {})
        platform_deps_formatted = data.get("platformDependenciesFormatted", {})

        stats = data.get("stats", {})
        platform_downloads = stats.get("platformDownloads", {})

        return cls(
            name=data.get("name", ""),
            created_at=data.get("createdAt", ""),
            project_id=data.get("projectId", 0),
            version_id=data.get("id", 0),
            description=data.get("description", ""),
            author=data.get("author", ""),
            visibility=data.get("visibility", "public"),
            review_state=data.get("reviewState", ""),
            channel=channel,
            platform_downloads=platform_downloads,
            downloads=downloads,
            plugin_dependencies=data.get("pluginDependencies", {}),
            platform_dependencies=platform_deps,
            platform_dependencies_formatted=platform_deps_formatted,
            member_names=data.get("memberNames", []),
            total_downloads=stats.get("totalDownloads", 0)
        )

    def get_download_url(self, platform: str = "PAPER") -> Optional[str]:
        """Get download URL for a specific platform"""
        if platform in self.downloads:
            return self.downloads[platform].download_url
        # Try any available platform
        if self.downloads:
            return next(iter(self.downloads.values())).download_url
        return None

    def get_file_name(self, platform: str = "PAPER") -> str:
        """Get file name for a specific platform"""
        if platform in self.downloads:
            return self.downloads[platform].file_name
        if self.downloads:
            return next(iter(self.downloads.values())).file_name
        return f"{self.name}.jar"


@dataclass
class HangarPlugin:
    """Complete plugin information from Hangar API"""
    name: str
    namespace: HangarNamespace
    description: str
    created_at: str
    last_updated: str
    visibility: str = "public"
    category: str = ""
    stats: Optional[HangarStats] = None
    settings: Dict[str, Any] = field(default_factory=dict)
    supported_platforms: Dict[str, List[str]] = field(default_factory=dict)
    main_page_content: Optional[str] = None
    avatar_url: Optional[str] = None
    user_actions: Optional[Dict[str, bool]] = None
    versions: List[HangarVersion] = field(default_factory=list)

    @property
    def owner(self) -> str:
        return self.namespace.owner

    @property
    def slug(self) -> str:
        return self.namespace.slug

    @property
    def full_slug(self) -> str:
        """Get owner/slug format"""
        return f"{self.owner}/{self.slug}"

    @property
    def url(self) -> str:
        return f"https://hangar.papermc.io/{self.owner}/{self.slug}"

    @property
    def latest_version(self) -> Optional[str]:
        if self.versions:
            return self.versions[0].name
        return None

    @classmethod
    def from_list_dict(cls, data: dict) -> "HangarPlugin":
        """Create from /projects list response"""
        namespace = HangarNamespace.from_dict(data.get("namespace", {}))

        stats = None
        if "stats" in data:
            stats = HangarStats.from_dict(data["stats"])

        return cls(
            name=data.get("name", ""),
            namespace=namespace,
            description=data.get("description", ""),
            created_at=data.get("createdAt", ""),
            last_updated=data.get("lastUpdated", ""),
            visibility=data.get("visibility", "public"),
            category=data.get("category", ""),
            stats=stats,
            settings=data.get("settings", {}),
            supported_platforms=data.get("supportedPlatforms", {}),
            main_page_content=data.get("mainPageContent"),
            avatar_url=data.get("avatarUrl"),
            user_actions=data.get("userActions")
        )

    @classmethod
    def from_detail_dict(cls, data: dict) -> "HangarPlugin":
        """Create from /projects/{owner}/{slug} response"""
        namespace = HangarNamespace.from_dict(data.get("namespace", {}))

        stats = None
        if "stats" in data:
            stats = HangarStats.from_dict(data["stats"])

        return cls(
            name=data.get("name", ""),
            namespace=namespace,
            description=data.get("description", ""),
            created_at=data.get("createdAt", ""),
            last_updated=data.get("lastUpdated", ""),
            visibility=data.get("visibility", "public"),
            category=data.get("category", ""),
            stats=stats,
            settings=data.get("settings", {}),
            supported_platforms=data.get("supportedPlatforms", {}),
            main_page_content=data.get("mainPageContent"),
            avatar_url=data.get("avatarUrl"),
            user_actions=data.get("userActions")
        )


@dataclass
class HangarPagination:
    """Pagination info from API response"""
    count: int
    limit: int
    offset: int

    @property
    def has_next(self) -> bool:
        return self.offset + self.limit < self.count

    @property
    def has_prev(self) -> bool:
        return self.offset > 0

    @property
    def next_offset(self) -> Optional[int]:
        return self.offset + self.limit if self.has_next else None

    @property
    def prev_offset(self) -> Optional[int]:
        return self.offset - self.limit if self.has_prev else None

    @classmethod
    def from_dict(cls, data: dict) -> "HangarPagination":
        return cls(
            count=data.get("count", 0),
            limit=data.get("limit", 0),
            offset=data.get("offset", 0)
        )


@dataclass
class PaginatedResponse:
    """Paginated API response wrapper for plugins"""
    plugins: List[HangarPlugin]
    pagination: HangarPagination

    @property
    def count(self) -> int:
        return self.pagination.count

    @property
    def limit(self) -> int:
        return self.pagination.limit

    @property
    def offset(self) -> int:
        return self.pagination.offset

    @property
    def has_next(self) -> bool:
        return self.pagination.has_next

    @property
    def has_prev(self) -> bool:
        return self.pagination.has_prev

    @property
    def next_offset(self) -> Optional[int]:
        return self.pagination.next_offset

    @property
    def prev_offset(self) -> Optional[int]:
        return self.pagination.prev_offset
