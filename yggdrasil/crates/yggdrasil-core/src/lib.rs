//! Framework-independent models and persistence port for embedded and standalone use.
use async_trait::async_trait;
use md5::{Digest, Md5};
use rand::RngCore;
use serde::{Deserialize, Serialize};
use std::{
    collections::BTreeMap,
    time::{SystemTime, UNIX_EPOCH},
};
use uuid::Uuid;

pub type Result<T> = std::result::Result<T, Error>;
#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("{0}")]
    Invalid(String),
    #[error("Invalid credentials or token.")]
    Forbidden,
    #[error("Authentication required.")]
    Unauthorized,
    #[error("Not found.")]
    NotFound,
    #[error("Identity or configuration conflict.")]
    Conflict,
    #[error("Too many attempts. Try again later.")]
    RateLimited,
    #[error("Upstream unavailable.")]
    Upstream,
    #[error("Internal service error.")]
    Internal,
}
pub fn now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}
pub fn secret() -> String {
    let mut b = [0u8; 32];
    rand::rngs::OsRng.fill_bytes(&mut b);
    hex::encode(b)
}
pub fn hash(b: impl AsRef<[u8]>) -> String {
    hex::encode(sha2::Sha256::digest(b.as_ref()))
}
pub fn id() -> String {
    Uuid::new_v4().simple().to_string()
}
pub fn canonical_uuid(s: &str) -> Result<String> {
    Uuid::parse_str(s)
        .map(|v| v.simple().to_string())
        .map_err(|_| Error::Invalid("Invalid UUID.".into()))
}
pub fn valid_name(s: &str) -> bool {
    (1..=16).contains(&s.len()) && s.bytes().all(|c| c.is_ascii_alphanumeric() || c == b'_')
}
pub fn offline_uuid(name: &str) -> Uuid {
    let mut b: [u8; 16] = Md5::digest(format!("OfflinePlayer:{name}").as_bytes()).into();
    b[6] = (b[6] & 0x0f) | 0x30;
    b[8] = (b[8] & 0x3f) | 0x80;
    Uuid::from_bytes(b)
}
#[derive(Clone, Debug, Serialize, Deserialize, schemars::JsonSchema, Default)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum UuidScheme {
    RandomV4,
    #[default]
    OfflinePlayer,
    Fixed {
        uuid: Uuid,
    },
    MojangMirror {
        uuid: Uuid,
    },
    NameV3 {
        namespace: Uuid,
        value: String,
    },
    NameV5 {
        namespace: Uuid,
        value: String,
    },
}
impl UuidScheme {
    pub fn resolve(&self, name: &str) -> Uuid {
        match self {
            Self::RandomV4 => Uuid::new_v4(),
            Self::OfflinePlayer => offline_uuid(name),
            Self::Fixed { uuid } | Self::MojangMirror { uuid } => *uuid,
            Self::NameV3 { namespace, value } => Uuid::new_v3(namespace, value.as_bytes()),
            Self::NameV5 { namespace, value } => Uuid::new_v5(namespace, value.as_bytes()),
        }
    }
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct User {
    pub id: String,
    pub username: String,
    pub password_hash: String,
    pub enabled: bool,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct Profile {
    pub id: String,
    pub user_id: String,
    pub name: String,
    pub uuid_scheme: UuidScheme,
    pub skin: Option<String>,
    pub cape: Option<String>,
    pub slim: bool,
    pub history: Vec<NameHistory>,
    pub mirrored_at: Option<u64>,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct NameHistory {
    pub name: String,
    pub changed_to_at: u64,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct Token {
    pub id: String,
    pub user_id: String,
    pub client_token: String,
    pub profile_id: Option<String>,
    pub expires: u64,
    pub temporary_invalid: bool,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct Join {
    pub profile_id: String,
    pub token_hash: String,
    pub ip: String,
    pub expires: u64,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct AdminSession {
    pub admin_id: String,
    pub expires: u64,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct Affinity {
    pub upstream_id: String,
    pub expires: u64,
    pub username: Option<String>,
    #[serde(default)]
    pub principal_hash: Option<String>,
    #[serde(default)]
    pub token_hash: Option<String>,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum Operation {
    Authenticate,
    Refresh,
    Validate,
    Invalidate,
    Signout,
    Join,
    HasJoined,
    ProfileLookup,
    ProfileBatch,
    ProfileByName,
    NameHistory,
    TextureUpload,
    TextureDelete,
    PlayerCertificates,
    Privileges,
    PlayerAttributes,
    Blocklist,
}
impl Operation {
    pub fn capability(self) -> &'static str {
        match self {
            Self::Authenticate
            | Self::Refresh
            | Self::Validate
            | Self::Invalidate
            | Self::Signout => "auth",
            Self::Join | Self::HasJoined => "session",
            Self::ProfileLookup | Self::ProfileBatch | Self::ProfileByName | Self::NameHistory => {
                "profiles"
            }
            Self::TextureUpload | Self::TextureDelete => "textures",
            _ => "minecraft_services",
        }
    }
    pub fn safe(self) -> bool {
        matches!(
            self,
            Self::HasJoined
                | Self::ProfileLookup
                | Self::ProfileBatch
                | Self::ProfileByName
                | Self::NameHistory
                | Self::Privileges
                | Self::PlayerAttributes
                | Self::Blocklist
        )
    }
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema, Default)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Transport {
    #[default]
    Direct,
    HttpProxy {
        proxy_url: String,
    },
    Socks5 {
        proxy_url: String,
    },
    Socks5h {
        proxy_url: String,
    },
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(default)]
pub struct HealthPolicy {
    pub enabled: bool,
    pub path: String,
    pub interval_seconds: u64,
    pub failure_threshold: u32,
    pub recovery_threshold: u32,
}
impl Default for HealthPolicy {
    fn default() -> Self {
        Self {
            enabled: true,
            path: "/".into(),
            interval_seconds: 30,
            failure_threshold: 3,
            recovery_threshold: 2,
        }
    }
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(default)]
pub struct Upstream {
    pub id: String,
    pub name: String,
    pub kind: String,
    pub base_url: String,
    pub priority: i32,
    pub enabled: bool,
    pub capabilities: Vec<String>,
    pub transport: Transport,
    pub connect_timeout_ms: u64,
    pub request_timeout_ms: u64,
    pub retry_count: u32,
    pub health: HealthPolicy,
    pub extra_headers: BTreeMap<String, String>,
    /// Exact additional hosts from which the gateway may mirror profile textures.
    pub texture_domains: Vec<String>,
}
impl Default for Upstream {
    fn default() -> Self {
        Self {
            id: id(),
            name: String::new(),
            kind: "yggdrasil".into(),
            base_url: String::new(),
            priority: 100,
            enabled: true,
            capabilities: vec!["auth".into(), "session".into(), "profiles".into()],
            transport: Transport::Direct,
            connect_timeout_ms: 5000,
            request_timeout_ms: 30000,
            retry_count: 1,
            health: HealthPolicy::default(),
            extra_headers: BTreeMap::new(),
            texture_domains: Vec::new(),
        }
    }
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct Target {
    pub source: String,
    pub priority: Option<i32>,
}
#[derive(Clone, Copy, PartialEq, Eq, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(rename_all = "snake_case")]
pub enum Strategy {
    PriorityFailover,
    MergePriority,
    Pinned,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct Rule {
    pub id: String,
    pub operation: Operation,
    pub match_priority: i32,
    pub strategy: Strategy,
    pub targets: Vec<Target>,
    pub enabled: bool,
    #[serde(default)]
    pub reject_conflicts: bool,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(default)]
pub struct CacheRoute {
    pub id: String,
    pub name: String,
    pub listen_prefix: String,
    pub upstream_id: String,
    pub upstream_path_prefix: String,
    pub methods: Vec<String>,
    pub enabled: bool,
    pub ttl_seconds: u64,
    pub negative_ttl_seconds: u64,
    pub stale_if_error_seconds: u64,
    pub max_object_bytes: usize,
    pub cache_authenticated: bool,
    pub respect_cache_control: bool,
    pub revalidate: bool,
    pub vary_headers: Vec<String>,
    pub ignore_query: Vec<String>,
    pub cache_statuses: Vec<u16>,
}
impl Default for CacheRoute {
    fn default() -> Self {
        Self {
            id: id(),
            name: String::new(),
            listen_prefix: "/cache/assets".into(),
            upstream_id: String::new(),
            upstream_path_prefix: "/".into(),
            methods: vec!["GET".into(), "HEAD".into()],
            enabled: true,
            ttl_seconds: 3600,
            negative_ttl_seconds: 30,
            stale_if_error_seconds: 300,
            max_object_bytes: 64 * 1024 * 1024,
            cache_authenticated: false,
            respect_cache_control: true,
            revalidate: true,
            vary_headers: vec!["accept".into(), "accept-encoding".into()],
            ignore_query: vec![],
            cache_statuses: vec![200, 203, 301, 404],
        }
    }
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
pub struct CacheEntry {
    pub route_id: String,
    pub blob: String,
    pub status: u16,
    pub headers: BTreeMap<String, String>,
    pub fresh_until: u64,
    pub stale_until: u64,
    pub size: usize,
    pub accessed: u64,
}
/// Private launcher observations, never used by authentication or federation.
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct AccountSnapshot {
    pub profile: SnapshotProfile,
    pub entitlement: SnapshotEntitlement,
    pub verified_at: u64,
    /// Allows a launcher-managed local session for cached third-party
    /// Yggdrasil profiles. Official snapshots keep this false and continue
    /// to require a verified Minecraft entitlement.
    #[serde(default)]
    pub local_auth: bool,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct SnapshotProfile {
    pub id: String,
    pub name: String,
    pub skin: SnapshotSkin,
    #[serde(default)]
    pub cape: String,
    pub capes: Vec<SnapshotCape>,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct SnapshotSkin {
    pub id: String,
    pub url: String,
    pub variant: String,
    #[serde(default)]
    pub data: String,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct SnapshotCape {
    pub id: String,
    pub url: String,
    pub alias: String,
    #[serde(default)]
    pub data: String,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct SnapshotEntitlement {
    pub owns_minecraft: bool,
    pub can_play_minecraft: bool,
}
#[derive(Clone, Serialize, Deserialize, schemars::JsonSchema, Default)]
#[serde(default)]
pub struct State {
    pub account_snapshots: BTreeMap<String, AccountSnapshot>,
    pub users: BTreeMap<String, User>,
    pub profiles: BTreeMap<String, Profile>,
    pub tokens: BTreeMap<String, Token>,
    pub joins: BTreeMap<String, Join>,
    pub admins: BTreeMap<String, User>,
    pub admin_sessions: BTreeMap<String, AdminSession>,
    pub upstreams: BTreeMap<String, Upstream>,
    pub rules: BTreeMap<String, Rule>,
    pub affinities: BTreeMap<String, Affinity>,
    pub cache_routes: BTreeMap<String, CacheRoute>,
    pub cache_entries: BTreeMap<String, CacheEntry>,
}
#[async_trait]
pub trait Repository: Send + Sync {
    async fn load(&self) -> Result<(i64, State)>;
    /// Atomically commit only if the revision still matches (including across processes).
    async fn compare_exchange(&self, revision: i64, state: &State) -> Result<bool>;
}
pub async fn transact<T>(repo: &dyn Repository, f: impl Fn(&mut State) -> Result<T>) -> Result<T> {
    for _ in 0..32 {
        let (v, mut s) = repo.load().await?;
        let out = f(&mut s)?;
        if repo.compare_exchange(v, &s).await? {
            return Ok(out);
        }
    }
    Err(Error::Conflict)
}
#[async_trait]
pub trait BlobStore: Send + Sync {
    async fn put(&self, bytes: &[u8]) -> Result<String>;
    async fn get(&self, hash: &str) -> Result<Vec<u8>>;
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn java_offline_identity() {
        assert_eq!(
            offline_uuid("Notch").to_string(),
            "b50ad385-829d-3141-a216-7e7d7539ba7f"
        );
        assert_ne!(offline_uuid("Steve"), offline_uuid("steve"));
    }
}
