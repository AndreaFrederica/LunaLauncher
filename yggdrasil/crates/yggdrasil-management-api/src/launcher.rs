//! Launcher integration uses the authenticated management boundary. Snapshots
//! are observations from the launcher, not local users or proof of ownership.
use super::*;
use base64::{engine::general_purpose::STANDARD, Engine};
use serde::Deserialize;

#[derive(Deserialize, schemars::JsonSchema)]
#[serde(deny_unknown_fields)]
pub(crate) struct LauncherCacheConfig {
    pub metadata_url: String,
    pub transport: Transport,
    #[serde(default)]
    pub textures_transport: Option<Transport>,
}

impl Management {
    pub(crate) async fn configure_launcher_cache(&self, value: Value) -> Result<Value> {
        let config: LauncherCacheConfig = parse(value)?;
        let mut metadata = url::Url::parse(&config.metadata_url)
            .map_err(|_| Error::Invalid("Invalid metadata URL.".into()))?;
        if metadata.query().is_some() || metadata.fragment().is_some() {
            return Err(Error::Invalid(
                "Metadata URL cannot contain a query or fragment.".into(),
            ));
        }
        // Match QUrl::resolved: a base ending in a filename resolves siblings.
        metadata = metadata
            .join(".")
            .map_err(|_| Error::Invalid("Invalid metadata URL.".into()))?;
        let key = hash(metadata.as_str());
        let metadata_id = format!("launcher-meta-{}", &key[..16]);
        let texture_id = "launcher-textures";
        let mut resources = Vec::new();
        for (id, root, ttl, stale) in [
            (metadata_id.as_str(), metadata.as_str(), 300, 30 * 86400),
            (
                texture_id,
                "https://textures.minecraft.net/",
                30 * 86400,
                365 * 86400,
            ),
        ] {
            let upstream = Upstream {
                id: id.into(),
                name: id.into(),
                kind: "http".into(),
                base_url: root.into(),
                capabilities: vec!["http".into()],
                transport: if id == texture_id {
                    config
                        .textures_transport
                        .clone()
                        .unwrap_or_else(|| config.transport.clone())
                } else {
                    config.transport.clone()
                },
                connect_timeout_ms: 1500,
                request_timeout_ms: 4000,
                retry_count: 0,
                health: HealthPolicy {
                    enabled: false,
                    ..HealthPolicy::default()
                },
                ..Upstream::default()
            };
            validate_upstream(&upstream)?;
            let route = CacheRoute {
                id: id.into(),
                name: id.into(),
                listen_prefix: format!("/cache/{id}"),
                upstream_id: id.into(),
                ttl_seconds: ttl,
                stale_if_error_seconds: stale,
                cache_statuses: vec![200],
                max_object_bytes: 16 * 1024 * 1024,
                ..CacheRoute::default()
            };
            resources.push((upstream, route));
        }
        transact(self.service.repo.as_ref(), |s| {
            for (u, r) in &resources {
                s.upstreams.insert(u.id.clone(), u.clone());
                validate_route(r, s)?;
                // Stable configuration preserves disk entries across child restarts.
                s.cache_routes.insert(r.id.clone(), r.clone());
            }
            Ok(())
        })
        .await?;
        Ok(json!({"metadata_path":format!("/cache/{metadata_id}/"),
            "textures_path":format!("/cache/{texture_id}/")}))
    }

    pub(crate) async fn account_snapshot(
        &self,
        method: &Method,
        id: &str,
        value: Value,
    ) -> Result<Value> {
        let id = canonical_uuid(id)?;
        match *method {
            Method::GET => {
                let (_, state) = self.service.repo.load().await?;
                Ok(json!(state
                    .account_snapshots
                    .get(&id)
                    .ok_or(Error::NotFound)?))
            }
            Method::DELETE => {
                transact(self.service.repo.as_ref(), |s| {
                    s.account_snapshots.remove(&id);
                    let user_id = format!("snapshot-{id}");
                    let owned = s.profiles.get(&id).is_some_and(|p| p.user_id == user_id);
                    s.users.remove(&user_id);
                    s.profiles.retain(|_, p| p.user_id != user_id);
                    s.tokens.retain(|_, t| t.user_id != user_id);
                    if owned {
                        s.joins.retain(|_, j| j.profile_id != id);
                    }
                    Ok(json!({"deleted":true}))
                })
                .await
            }
            Method::PUT => {
                let mut snapshot: AccountSnapshot = parse(value)?;
                snapshot.profile.id = canonical_uuid(&snapshot.profile.id)?;
                if snapshot.profile.id != id
                    || !valid_name(&snapshot.profile.name)
                    || snapshot.verified_at == 0
                    || snapshot.verified_at > now() + 60
                    || snapshot.profile.capes.len() > 64
                {
                    return Err(Error::Invalid("Invalid account snapshot.".into()));
                }
                validate_texture_url(&snapshot.profile.skin.url)?;
                if snapshot.profile.skin.id.len() > 128
                    || snapshot.profile.skin.variant.len() > 16
                    || snapshot.profile.cape.len() > 128
                {
                    return Err(Error::Invalid("Invalid texture metadata.".into()));
                }
                snapshot.profile.skin.data =
                    clean_texture(snapshot.profile.skin.data, "skin").await?;
                for cape in &mut snapshot.profile.capes {
                    validate_texture_url(&cape.url)?;
                    if cape.id.len() > 128 || cape.alias.len() > 256 {
                        return Err(Error::Invalid("Invalid cape metadata.".into()));
                    }
                    cape.data = clean_texture(std::mem::take(&mut cape.data), "cape").await?;
                }
                transact(self.service.repo.as_ref(), |s| {
                    if s.account_snapshots
                        .get(&id)
                        .is_some_and(|old| old.verified_at > snapshot.verified_at)
                    {
                        return Err(Error::Conflict);
                    }
                    s.account_snapshots.insert(id.clone(), snapshot.clone());
                    let user_id = format!("snapshot-{id}");
                    if !snapshot.entitlement.owns_minecraft
                        && !snapshot.entitlement.can_play_minecraft
                    {
                        s.tokens.retain(|_, token| token.user_id != user_id);
                        if s.profiles.get(&id).is_some_and(|p| p.user_id == user_id) {
                            s.joins.retain(|_, join| join.profile_id != id);
                        }
                    }
                    Ok(json!({"stored":true,"verified_at":snapshot.verified_at}))
                })
                .await
            }
            _ => Err(Error::NotFound),
        }
    }

    pub(crate) async fn snapshot_session(&self, id: &str) -> Result<Value> {
        let id = canonical_uuid(id)?;
        let (_, state) = self.service.repo.load().await?;
        let snapshot = state.account_snapshots.get(&id).ok_or(Error::NotFound)?;
        if !snapshot.local_auth
            && !snapshot.entitlement.owns_minecraft
            && !snapshot.entitlement.can_play_minecraft
        {
            return Err(Error::Forbidden);
        }
        let skin = self.snapshot_blob(&snapshot.profile.skin.data).await?;
        let cape = if let Some(cape) = snapshot
            .profile
            .capes
            .iter()
            .find(|c| c.id == snapshot.profile.cape)
        {
            self.snapshot_blob(&cape.data).await?
        } else {
            None
        };
        let user_id = format!("snapshot-{id}");
        let password_hash = password_hash(secret()).await?;
        let token = secret();
        let client = secret();
        transact(self.service.repo.as_ref(), |s| {
            // An account removed or updated during blob preparation must not be resurrected.
            let current = s.account_snapshots.get(&id).ok_or(Error::NotFound)?;
            if serde_json::to_value(current).ok() != serde_json::to_value(snapshot).ok() {
                return Err(Error::Conflict);
            }
            if s.profiles.get(&id).is_some_and(|p| p.user_id != user_id)
                || s.users.values().any(|u| u.id != user_id && u.username.eq_ignore_ascii_case(&user_id))
                || s.profiles.values().any(|p| p.id != id && p.name.eq_ignore_ascii_case(&snapshot.profile.name)) {
                return Err(Error::Conflict);
            }
            s.users.insert(user_id.clone(), User { id: user_id.clone(), username: user_id.clone(), password_hash: password_hash.clone(), enabled: true });
            let old = s.profiles.get(&id);
            let mut history = old.map(|p| p.history.clone()).unwrap_or_default();
            if history.last().is_none_or(|h| h.name != snapshot.profile.name) {
                history.push(NameHistory { name: snapshot.profile.name.clone(), changed_to_at: snapshot.verified_at * 1000 });
            }
            s.profiles.insert(id.clone(), Profile {
                id: id.clone(), user_id: user_id.clone(), name: snapshot.profile.name.clone(),
                uuid_scheme: UuidScheme::MojangMirror { uuid: id.parse().map_err(|_| Error::Internal)? },
                skin: skin.clone(), cape: cape.clone(), slim: snapshot.profile.skin.variant.eq_ignore_ascii_case("slim"),
                history, mirrored_at: Some(snapshot.verified_at),
            });
            s.tokens.insert(hash(&token), Token { id: yggdrasil_core::id(), user_id: user_id.clone(),
                client_token: client.clone(), profile_id: Some(id.clone()), expires: now() + 86400, temporary_invalid: false });
            Ok(json!({"accessToken":token,"clientToken":client,"selectedProfile":{"id":id,"name":snapshot.profile.name},
                "verified_at":snapshot.verified_at,"api_url":self.service.public_url,"metadata":self.service.metadata()?}))
        }).await
    }

    async fn snapshot_blob(&self, data: &str) -> Result<Option<String>> {
        if data.is_empty() {
            return Ok(None);
        }
        let bytes = STANDARD.decode(data).map_err(|_| Error::Internal)?;
        self.service.blobs.put(&bytes).await.map(Some)
    }
}

fn validate_texture_url(value: &str) -> Result<()> {
    if value.is_empty() {
        return Ok(());
    }
    let url = url::Url::parse(value).map_err(|_| Error::Invalid("Invalid texture URL.".into()))?;
    if url.host_str() != Some("textures.minecraft.net")
        || !matches!(url.scheme(), "http" | "https")
        || url.port().is_some()
        || !url.username().is_empty()
        || url.password().is_some()
        || url.query().is_some()
        || url.fragment().is_some()
        || value.len() > 2048
    {
        return Err(Error::Invalid("Invalid official texture URL.".into()));
    }
    Ok(())
}

async fn clean_texture(data: String, kind: &'static str) -> Result<String> {
    if data.is_empty() {
        return Ok(data);
    }
    let bytes = STANDARD
        .decode(data)
        .map_err(|_| Error::Invalid("Invalid texture encoding.".into()))?;
    let clean = tokio::task::spawn_blocking(move || sanitize_texture(&bytes, kind))
        .await
        .map_err(|_| Error::Internal)??;
    Ok(STANDARD.encode(clean))
}
