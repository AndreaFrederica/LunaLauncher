//! Versioned operator API. Secret fields are write-only; cookie writes require an
//! exact same-origin Origin header. Bearer clients do not need browser CSRF state.
mod launcher;
mod schemas;
use axum::{
    body::to_bytes,
    extract::{Request, State as AxumState},
    http::{HeaderMap, HeaderValue, Method, StatusCode},
    response::{IntoResponse, Response},
    routing::{any, get},
    Router,
};
use serde_json::{json, Value};
use std::{collections::BTreeMap, sync::Arc};
use yggdrasil_cache::{validate_route, Cache};
use yggdrasil_core::*;
use yggdrasil_federation::validate_rule;
use yggdrasil_service::{password_hash, sanitize_texture, Credentials, Service, MAX_TEXTURE};
use yggdrasil_upstream::{validate_upstream, Clients};

#[derive(Clone)]
pub struct Management {
    pub service: Arc<Service>,
    pub clients: Clients,
    pub cache: Cache,
    pub database_backend: String,
    /// Public self-registration can be disabled for closed deployments.
    pub registration_enabled: bool,
}
pub fn router(m: Management) -> Router {
    Router::new()
        .route("/api/openapi.json", get(openapi))
        .route("/api/v1/{*path}", any(dispatch))
        .layer(axum::extract::DefaultBodyLimit::max(MAX_TEXTURE + 65536))
        .with_state(m)
}
pub struct ApiError(pub Error);
impl From<Error> for ApiError {
    fn from(e: Error) -> Self {
        Self(e)
    }
}
impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
        let (status, code) = match self.0 {
            Error::Unauthorized => (401, "unauthorized"),
            Error::Forbidden => (403, "forbidden"),
            Error::NotFound => (404, "not_found"),
            Error::Conflict => (409, "conflict"),
            Error::Invalid(_) => (400, "invalid_input"),
            Error::RateLimited => (429, "rate_limited"),
            Error::Upstream => (502, "upstream_unavailable"),
            Error::Internal => (500, "internal_error"),
        };
        (
            StatusCode::from_u16(status).unwrap(),
            axum::Json(json!({"error":{"code":code,"message":self.0.to_string(),"details":{}}})),
        )
            .into_response()
    }
}
fn text(v: &Value, k: &str) -> Result<String> {
    v[k].as_str()
        .map(str::to_owned)
        .ok_or_else(|| Error::Invalid(format!("Missing string field: {k}.")))
}
fn public_user(u: &User) -> Value {
    json!({"id":u.id,"username":u.username,"enabled":u.enabled})
}
fn public_upstream(u: &Upstream) -> Value {
    let mut v = serde_json::to_value(u).unwrap_or(Value::Null);
    if let Some(proxy) = v["transport"]["proxy_url"].as_str() {
        if let Ok(mut url) = url::Url::parse(proxy) {
            let _ = url.set_username("");
            let _ = url.set_password(None);
            v["transport"]["proxy_url"] = json!(url.to_string());
        }
    }
    if let Some(headers) = v["extra_headers"].as_object_mut() {
        for val in headers.values_mut() {
            *val = json!("[redacted]");
        }
    }
    v
}
fn json_ok(v: Value) -> Response {
    let mut response = axum::Json(v).into_response();
    response
        .headers_mut()
        .insert("cache-control", HeaderValue::from_static("no-store"));
    response
}
fn page(mut items: Vec<Value>, query: &BTreeMap<String, String>) -> Result<Value> {
    items.sort_by_key(|v| v["id"].as_str().unwrap_or("").to_owned());
    if let Some(cursor) = query.get("cursor") {
        items.retain(|v| v["id"].as_str().unwrap_or("") > cursor.as_str())
    }
    let limit = query
        .get("limit")
        .map(|s| s.parse::<usize>())
        .transpose()
        .map_err(|_| Error::Invalid("Invalid limit.".into()))?
        .unwrap_or(50)
        .clamp(1, 200);
    let more = items.len() > limit;
    items.truncate(limit);
    let cursor = if more {
        items
            .last()
            .and_then(|v| v["id"].as_str())
            .map(str::to_owned)
    } else {
        None
    };
    Ok(json!({"items":items,"next_cursor":cursor}))
}
fn merge(old: Value, new: &Value) -> Result<Value> {
    let mut old = old.as_object().cloned().ok_or(Error::Internal)?;
    let patch = new
        .as_object()
        .ok_or_else(|| Error::Invalid("Expected JSON object.".into()))?;
    for (k, v) in patch {
        old.insert(k.clone(), v.clone());
    }
    Ok(Value::Object(old))
}
fn parse<T: serde::de::DeserializeOwned>(v: Value) -> Result<T> {
    serde_json::from_value(v).map_err(|_| Error::Invalid("Invalid resource fields.".into()))
}

impl Management {
    async fn account(
        &self,
        method: &Method,
        path: &str,
        b: Value,
        headers: &HeaderMap,
    ) -> Result<Response> {
        let raw = headers
            .get("authorization")
            .and_then(|v| v.to_str().ok())
            .and_then(|v| v.strip_prefix("Bearer "))
            .ok_or(Error::Unauthorized)?;
        let (_, state) = self.service.repo.load().await?;
        let token = yggdrasil_service::checked_token(&state, raw, None, false)?;
        let user = state.users.get(&token.user_id).ok_or(Error::Forbidden)?;
        match (method.as_str(), path) {
            ("GET", "account") => Ok(json_ok(json!({"user":public_user(user),
                "profiles":state.profiles.values().filter(|p| p.user_id == user.id).collect::<Vec<_>>()}))),
            ("POST", "account/password") => {
                let p: schemas::OwnPassword = parse(b)?;
                self.service
                    .change_own_password(raw, p.current_password, p.password)
                    .await?;
                Ok(json_ok(json!({})))
            }
            _ => Err(Error::NotFound),
        }
    }
    pub async fn authorize(&self, headers: &HeaderMap, write: bool) -> Result<(String, User)> {
        let bearer = headers
            .get("authorization")
            .and_then(|v| v.to_str().ok())
            .and_then(|v| v.strip_prefix("Bearer "));
        let raw = if let Some(raw) = bearer {
            raw
        } else {
            if write {
                let origin = headers
                    .get("origin")
                    .and_then(|v| v.to_str().ok())
                    .ok_or(Error::Forbidden)?;
                let expected = url::Url::parse(&self.service.public_url)
                    .map_err(|_| Error::Internal)?
                    .origin()
                    .ascii_serialization();
                if origin != expected {
                    return Err(Error::Forbidden);
                }
            }
            headers
                .get("cookie")
                .and_then(|v| v.to_str().ok())
                .and_then(|v| {
                    v.split(';')
                        .map(str::trim)
                        .find_map(|s| s.strip_prefix("ygg_session="))
                })
                .ok_or(Error::Unauthorized)?
        };
        let h = hash(raw);
        let (_, s) = self.service.repo.load().await?;
        let session = s
            .admin_sessions
            .get(&h)
            .filter(|v| v.expires > now())
            .ok_or(Error::Unauthorized)?;
        let user = s
            .admins
            .get(&session.admin_id)
            .filter(|u| u.enabled)
            .ok_or(Error::Unauthorized)?
            .clone();
        Ok((h, user))
    }
    pub async fn launcher_session(&self) -> Result<String> {
        let raw = secret();
        let pwd = password_hash(secret()).await?;
        transact(self.service.repo.as_ref(), |s| {
            let user = s
                .admins
                .values()
                .find(|u| u.username == "__launcher__")
                .cloned()
                .unwrap_or(User {
                    id: id(),
                    username: "__launcher__".into(),
                    password_hash: pwd.clone(),
                    enabled: true,
                });
            if !user.enabled {
                return Err(Error::Forbidden);
            }
            s.admins.insert(user.id.clone(), user.clone());
            s.admin_sessions.insert(
                hash(&raw),
                AdminSession {
                    admin_id: user.id,
                    expires: now() + 86400 * 30,
                },
            );
            Ok(())
        })
        .await?;
        Ok(raw)
    }
    async fn login(&self, b: Value, headers: &HeaderMap) -> Result<Response> {
        if let Some(origin) = headers.get("origin") {
            let expected = url::Url::parse(&self.service.public_url)
                .map_err(|_| Error::Internal)?
                .origin()
                .ascii_serialization();
            if origin.to_str().ok() != Some(&expected) {
                return Err(Error::Forbidden);
            }
        }
        let c: Credentials = parse(b)?;
        let u = self.service.check_credentials(&c, true).await?;
        let raw = secret();
        let h = hash(&raw);
        transact(self.service.repo.as_ref(), |s| {
            if !s
                .admins
                .get(&u.id)
                .is_some_and(|v| v.enabled && v.password_hash == u.password_hash)
            {
                return Err(Error::Forbidden);
            }
            s.admin_sessions.insert(
                h.clone(),
                AdminSession {
                    admin_id: u.id.clone(),
                    expires: now() + 43200,
                },
            );
            Ok(())
        })
        .await?;
        let mut r = json_ok(json!({"token":raw,"expires_in":43200,"user":public_user(&u)}));
        let secure = if self.service.public_url.starts_with("https:") {
            "; Secure"
        } else {
            ""
        };
        r.headers_mut().insert(
            "set-cookie",
            HeaderValue::from_str(&format!(
                "ygg_session={raw}; Path=/api/v1; HttpOnly; SameSite=Strict; Max-Age=43200{secure}"
            ))
            .map_err(|_| Error::Internal)?,
        );
        Ok(r)
    }
    async fn register(&self, b: Value, headers: &HeaderMap, peer: &str) -> Result<Response> {
        if !self.registration_enabled {
            return Err(Error::Forbidden);
        }
        if let Some(origin) = headers.get("origin") {
            let expected = url::Url::parse(&self.service.public_url)
                .map_err(|_| Error::Internal)?
                .origin()
                .ascii_serialization();
            if origin.to_str().ok() != Some(&expected) {
                return Err(Error::Forbidden);
            }
        }
        self.service
            .rate_limit(&format!("register-peer:{peer}"))
            .await?;
        let registration: schemas::Register = parse(b)?;
        let (user, auth) = self
            .service
            .register(
                registration.username,
                registration.password,
                registration.profile_name,
                registration.client_token,
                registration.request_user,
            )
            .await?;
        Ok(json_ok(json!({
            "user": public_user(&user),
            "accessToken": auth.access_token,
            "clientToken": auth.client_token,
            "availableProfiles": auth.available_profiles,
            "selectedProfile": auth.selected_profile,
            "userProperties": auth.user,
        })))
    }
    async fn mirror(&self, pid: &str, token: String) -> Result<Value> {
        let official = Upstream {
            base_url: "https://api.minecraftservices.com".into(),
            ..Upstream::default()
        };
        let mut headers = HeaderMap::new();
        headers.insert(
            "authorization",
            HeaderValue::from_str(&format!("Bearer {token}")).map_err(|_| Error::Forbidden)?,
        );
        let r = self
            .clients
            .fetch(
                &official,
                Method::GET,
                "/minecraft/profile",
                headers,
                vec![],
                true,
                1024 * 1024,
            )
            .await
            .map_err(|_| Error::Upstream)?;
        if r.status != 200 {
            return Err(Error::Forbidden);
        }
        let profile: Value = parse(serde_json::from_slice(&r.body).map_err(|_| Error::Upstream)?)?;
        if canonical_uuid(&text(&profile, "id")?)? != pid {
            return Err(Error::Invalid(
                "Official UUID must match the target profile UUID.".into(),
            ));
        }
        let name = text(&profile, "name")?;
        if !valid_name(&name) {
            return Err(Error::Upstream);
        }
        let mut skin = None;
        let mut cape = None;
        let mut slim = false;
        for (field, kind) in [("skins", "skin"), ("capes", "cape")] {
            if let Some(texture) = profile[field]
                .as_array()
                .and_then(|a| a.iter().find(|v| v["state"] == "ACTIVE"))
            {
                let mut url =
                    url::Url::parse(&text(texture, "url")?).map_err(|_| Error::Upstream)?;
                if url.host_str() != Some("textures.minecraft.net")
                    || !matches!(url.scheme(), "http" | "https")
                    || url.port().is_some()
                    || !url.username().is_empty()
                    || url.password().is_some()
                {
                    return Err(Error::Upstream);
                }
                let _ = url.set_scheme("https");
                let u = Upstream {
                    base_url: "https://textures.minecraft.net".into(),
                    ..Upstream::default()
                };
                let bytes = self
                    .clients
                    .fetch(
                        &u,
                        Method::GET,
                        url.path(),
                        HeaderMap::new(),
                        vec![],
                        true,
                        MAX_TEXTURE,
                    )
                    .await
                    .map_err(|_| Error::Upstream)?;
                if bytes.status != 200 {
                    return Err(Error::Upstream);
                }
                let k = kind.to_owned();
                let clean = tokio::task::spawn_blocking(move || sanitize_texture(&bytes.body, &k))
                    .await
                    .map_err(|_| Error::Internal)??;
                let h = self.service.blobs.put(&clean).await?;
                if kind == "skin" {
                    skin = Some(h);
                    slim = texture["variant"] == "SLIM"
                } else {
                    cape = Some(h)
                }
            }
        }
        transact(self.service.repo.as_ref(), |s| {
            if s.profiles
                .values()
                .any(|p| p.id != pid && p.name.eq_ignore_ascii_case(&name))
            {
                return Err(Error::Conflict);
            }
            let p = s.profiles.get_mut(pid).ok_or(Error::NotFound)?;
            if p.name != name {
                p.history.push(NameHistory {
                    name: name.clone(),
                    changed_to_at: now() * 1000,
                });
                p.name = name.clone();
                for t in s
                    .tokens
                    .values_mut()
                    .filter(|t| t.profile_id.as_deref() == Some(pid))
                {
                    t.temporary_invalid = true;
                }
            }
            p.skin = skin.clone();
            p.cape = cape.clone();
            p.slim = slim;
            p.mirrored_at = Some(now());
            p.uuid_scheme = UuidScheme::MojangMirror {
                uuid: pid.parse().map_err(|_| Error::Internal)?,
            };
            Ok(json!(p))
        })
        .await
    }
    async fn resource(
        &self,
        method: &Method,
        path: &str,
        b: Value,
        q: BTreeMap<String, String>,
        headers: &HeaderMap,
        bytes: Vec<u8>,
    ) -> Result<Value> {
        let segments: Vec<&str> = path.split('/').collect();
        let group = segments.first().copied().unwrap_or("");
        let rid = segments.get(1).copied();
        let action = segments.get(2).copied();
        let (_, s) = self.service.repo.load().await?;
        match (method.as_str(), path) {
            ("PUT", "launcher/cache") => return self.configure_launcher_cache(b).await,
            ("GET", "status") => {
                return Ok(
                    json!({"version":env!("CARGO_PKG_VERSION"),"database":self.database_backend,"users":s.users.len(),"profiles":s.profiles.len(),"upstreams":s.upstreams.len(),"registration_enabled":self.registration_enabled}),
                )
            }
            ("GET", "capabilities") => {
                return Ok(
                    json!({"uuid_schemes":["random_v4","offline_player","fixed","mojang_mirror","name_v3","name_v5"],"public_registration":self.registration_enabled,"upstream_kinds":["yggdrasil","http"],"transports":["direct","http_proxy","socks5","socks5h"],"federation_strategies":["priority_failover","merge_priority","pinned"],"federation_sources":["local","upstream"],"profile_keys":false,"databases":["sqlite","postgresql","mysql"],"cache":{"storage":"disk","etag":true,"last_modified":true,"stale_if_error":true}}),
                )
            }
            ("GET", "cache/stats") => return self.cache.stats().await,
            ("POST", "cache/purge") => {
                return Ok(
                    json!({"purged":self.cache.purge(b["route_id"].as_str(),b["key"].as_str()).await?}),
                )
            }
            ("POST", "federation/rules/validate") => {
                let rule: Rule = parse(b)?;
                validate_rule(&rule, &s)?;
                return Ok(json!({"valid":true}));
            }
            _ => {}
        }
        if group == "users" || group == "admins" {
            let admin = group == "admins";
            let users = if admin { &s.admins } else { &s.users };
            if method == Method::GET {
                return if let Some(id) = rid {
                    Ok(public_user(users.get(id).ok_or(Error::NotFound)?))
                } else {
                    page(users.values().map(public_user).collect(), &q)
                };
            }
            if method == Method::POST && rid.is_none() {
                return Ok(public_user(
                    &self
                        .service
                        .create_user(text(&b, "username")?, text(&b, "password")?, admin)
                        .await?,
                ));
            }
            let id = rid.ok_or(Error::NotFound)?.to_owned();
            if method == Method::POST && action == Some("password") {
                let password = password_hash(text(&b, "password")?).await?;
                return transact(self.service.repo.as_ref(), |s| {
                    let users = if admin { &mut s.admins } else { &mut s.users };
                    users.get_mut(&id).ok_or(Error::NotFound)?.password_hash = password.clone();
                    revoke(s, &id, admin);
                    Ok(json!({}))
                })
                .await;
            }
            if method == Method::POST && action == Some("revoke-sessions") {
                return transact(self.service.repo.as_ref(), |s| {
                    revoke(s, &id, admin);
                    Ok(json!({}))
                })
                .await;
            }
            if method == Method::PATCH || method == Method::DELETE {
                return transact(self.service.repo.as_ref(), |s| {
                    let users = if admin { &mut s.admins } else { &mut s.users };
                    let u = users.get(&id).ok_or(Error::NotFound)?;
                    if admin
                        && u.enabled
                        && (method == Method::DELETE || b["enabled"] == false)
                        && users.values().filter(|u| u.enabled).count() <= 1
                    {
                        return Err(Error::Conflict);
                    }
                    if method == Method::DELETE {
                        users.remove(&id);
                        revoke(s, &id, admin);
                        if !admin {
                            s.profiles.retain(|_, p| p.user_id != id);
                        }
                        return Ok(json!({}));
                    }
                    if let Some(name) = b["username"].as_str() {
                        if name.is_empty() || name.len() > 128 || (!admin && name.contains("::")) {
                            return Err(Error::Invalid("Invalid username.".into()));
                        }
                        if users
                            .values()
                            .any(|u| u.id != id && u.username.eq_ignore_ascii_case(name))
                        {
                            return Err(Error::Conflict);
                        }
                    }
                    let u = users.get_mut(&id).ok_or(Error::NotFound)?;
                    if let Some(name) = b["username"].as_str() {
                        u.username = name.into();
                    }
                    if let Some(enabled) = b["enabled"].as_bool() {
                        u.enabled = enabled;
                    }
                    let out = public_user(u);
                    if !u.enabled {
                        revoke(s, &id, admin);
                    }
                    Ok(out)
                })
                .await;
            }
        }
        if group == "account-snapshots" && segments.len() == 2 {
            return self
                .account_snapshot(method, rid.ok_or(Error::NotFound)?, b)
                .await;
        }
        if group == "account-snapshots"
            && segments.len() == 3
            && action == Some("session")
            && method == Method::POST
        {
            return self.snapshot_session(rid.ok_or(Error::NotFound)?).await;
        }
        if group == "profiles" {
            if method == Method::GET && rid.is_none() {
                return page(
                    s.profiles
                        .values()
                        .filter(|p| {
                            q.get("name").is_none_or(|n| p.name.eq_ignore_ascii_case(n))
                                && q.get("source").is_none_or(|src| {
                                    if src == "mojang_mirror" {
                                        p.mirrored_at.is_some()
                                    } else {
                                        p.mirrored_at.is_none()
                                    }
                                })
                        })
                        .map(|p| json!(p))
                        .collect(),
                    &q,
                );
            }
            if method == Method::POST && rid.is_none() {
                let scheme: UuidScheme = if b.get("uuid_scheme").is_some() {
                    parse(b["uuid_scheme"].clone())?
                } else {
                    UuidScheme::default()
                };
                return Ok(json!(
                    self.service
                        .create_profile(text(&b, "user_id")?, text(&b, "name")?, scheme)
                        .await?
                ));
            }
            let id = canonical_uuid(rid.ok_or(Error::NotFound)?)?;
            let p = s.profiles.get(&id).ok_or(Error::NotFound)?;
            if method == Method::GET {
                return match action {
                    None => Ok(json!(p)),
                    Some("history") => Ok(json!(p.history)),
                    _ => Err(Error::NotFound),
                };
            }
            if (method == Method::POST && action == Some("rename")) || method == Method::PATCH {
                return Ok(json!(self.service.rename(&id, text(&b, "name")?).await?));
            }
            if method == Method::POST && segments.get(2..) == Some(&["mirror", "minecraft"][..]) {
                return self.mirror(&id, text(&b, "access_token")?).await;
            }
            if matches!(action, Some("skin" | "cape")) {
                let kind = action.unwrap();
                if method == Method::DELETE {
                    self.service.delete_texture(&id, kind).await?;
                } else if method == Method::PUT {
                    if headers.get("content-type").and_then(|v| v.to_str().ok())
                        != Some("image/png")
                    {
                        return Err(Error::Invalid("Expected image/png body.".into()));
                    }
                    self.service
                        .texture(
                            &id,
                            kind,
                            bytes,
                            q.get("model").is_some_and(|s| s == "slim"),
                        )
                        .await?;
                } else {
                    return Err(Error::NotFound);
                }
                return Ok(json!({}));
            }
            if method == Method::DELETE && action.is_none() {
                return transact(self.service.repo.as_ref(), |s| {
                    s.profiles.remove(&id);
                    s.tokens.retain(|_, t| t.profile_id.as_deref() != Some(&id));
                    s.joins.retain(|_, j| j.profile_id != id);
                    Ok(json!({}))
                })
                .await;
            }
        }
        if group == "tokens" || group == "sessions" {
            if method == Method::GET && rid.is_none() {
                let items = if group == "tokens" {
                    let mut items: Vec<Value> = s.tokens.values().map(|t|json!({"id":t.id,"user_id":t.user_id,"profile_id":t.profile_id,"expires":t.expires,"temporary_invalid":t.temporary_invalid})).collect();
                    items.extend(s.affinities.iter().filter(|(k,_)|k.starts_with("token:")).map(|(k,a)|json!({"id":format!("upstream:{k}"),"upstream_id":a.upstream_id,"profile_name":a.username,"expires":a.expires})));
                    items
                } else {
                    s.admin_sessions
                        .iter()
                        .map(|(k, v)| json!({"id":k,"admin_id":v.admin_id,"expires":v.expires}))
                        .collect()
                };
                return page(items, &q);
            }
            if method == Method::DELETE {
                let id = rid.ok_or(Error::NotFound)?;
                return transact(self.service.repo.as_ref(), |s| {
                    if group == "tokens" {
                        if let Some(key) = id.strip_prefix("upstream:") {
                            if let Some(affinity) = s.affinities.remove(key) {
                                s.affinities
                                    .retain(|_, a| a.token_hash != affinity.token_hash);
                            }
                        }
                        s.tokens.retain(|_, t| t.id != id);
                        s.joins.retain(|_, j| s.tokens.contains_key(&j.token_hash));
                    } else {
                        s.admin_sessions.remove(id);
                    }
                    Ok(json!({}))
                })
                .await;
            }
        }
        if group == "upstreams" {
            if method == Method::POST && action == Some("probe") {
                let u = s
                    .upstreams
                    .get(rid.ok_or(Error::NotFound)?)
                    .ok_or(Error::NotFound)?;
                return Ok(json!(self.clients.probe(u).await));
            }
            if method == Method::GET && action == Some("health") {
                let id = rid.ok_or(Error::NotFound)?;
                if !s.upstreams.contains_key(id) {
                    return Err(Error::NotFound);
                }
                return Ok(json!(self
                    .clients
                    .health
                    .read()
                    .await
                    .get(id)
                    .cloned()
                    .unwrap_or_default()));
            }
            if method == Method::GET {
                return if let Some(id) = rid {
                    Ok(public_upstream(s.upstreams.get(id).ok_or(Error::NotFound)?))
                } else {
                    page(
                        s.upstreams
                            .values()
                            .filter(|u| q.get("enabled").is_none_or(|v| u.enabled == (v == "true")))
                            .map(public_upstream)
                            .collect(),
                        &q,
                    )
                };
            }
            if method == Method::POST && rid.is_none() || method == Method::PATCH {
                let upstream: Upstream = if let Some(id) = rid {
                    parse(merge(
                        json!(s.upstreams.get(id).ok_or(Error::NotFound)?),
                        &b,
                    )?)?
                } else {
                    parse(b)?
                };
                validate_upstream(&upstream)?;
                if rid.is_some_and(|id| id != upstream.id) {
                    return Err(Error::Invalid("Resource id is immutable.".into()));
                }
                return transact(self.service.repo.as_ref(), |s| {
                    if method == Method::POST && s.upstreams.contains_key(&upstream.id) {
                        return Err(Error::Conflict);
                    }
                    s.upstreams.insert(upstream.id.clone(), upstream.clone());
                    Ok(public_upstream(&upstream))
                })
                .await;
            }
            if method == Method::DELETE {
                let id = rid.ok_or(Error::NotFound)?;
                return transact(self.service.repo.as_ref(), |s| {
                    if s.rules
                        .values()
                        .any(|r| r.targets.iter().any(|t| t.source == id))
                        || s.cache_routes.values().any(|r| r.upstream_id == id)
                        || s.affinities
                            .values()
                            .any(|a| a.upstream_id == id && a.expires > now())
                    {
                        return Err(Error::Conflict);
                    }
                    s.upstreams.remove(id).ok_or(Error::NotFound)?;
                    Ok(json!({}))
                })
                .await;
            }
        }
        if path == "federation/rules" || path.starts_with("federation/rules/") {
            let id = segments.get(2).copied();
            if method == Method::GET {
                return if let Some(id) = id {
                    Ok(json!(s.rules.get(id).ok_or(Error::NotFound)?))
                } else {
                    page(s.rules.values().map(|v| json!(v)).collect(), &q)
                };
            }
            if method == Method::DELETE {
                let id = id.ok_or(Error::NotFound)?;
                return transact(self.service.repo.as_ref(), |s| {
                    s.rules.remove(id).ok_or(Error::NotFound)?;
                    Ok(json!({}))
                })
                .await;
            }
            if method == Method::POST || method == Method::PATCH {
                let r: Rule = if method == Method::PATCH {
                    parse(merge(
                        json!(s
                            .rules
                            .get(id.ok_or(Error::NotFound)?)
                            .ok_or(Error::NotFound)?),
                        &b,
                    )?)?
                } else {
                    parse(b)?
                };
                if id.is_some_and(|id| id != r.id) {
                    return Err(Error::Invalid("Resource id is immutable.".into()));
                }
                return transact(self.service.repo.as_ref(), |s| {
                    validate_rule(&r, s)?;
                    if method == Method::POST && s.rules.contains_key(&r.id) {
                        return Err(Error::Conflict);
                    }
                    s.rules.insert(r.id.clone(), r.clone());
                    Ok(json!(r))
                })
                .await;
            }
        }
        if path == "cache/routes" || path.starts_with("cache/routes/") {
            let id = segments.get(2).copied();
            if method == Method::GET {
                return if let Some(id) = id {
                    Ok(json!(s.cache_routes.get(id).ok_or(Error::NotFound)?))
                } else {
                    page(s.cache_routes.values().map(|v| json!(v)).collect(), &q)
                };
            }
            if method == Method::DELETE {
                let id = id.ok_or(Error::NotFound)?;
                return transact(self.service.repo.as_ref(), |s| {
                    s.cache_routes.remove(id).ok_or(Error::NotFound)?;
                    s.cache_entries.retain(|_, e| e.route_id != id);
                    Ok(json!({}))
                })
                .await;
            }
            if method == Method::POST || method == Method::PATCH {
                let r: CacheRoute = if method == Method::PATCH {
                    parse(merge(
                        json!(s
                            .cache_routes
                            .get(id.ok_or(Error::NotFound)?)
                            .ok_or(Error::NotFound)?),
                        &b,
                    )?)?
                } else {
                    parse(b)?
                };
                if id.is_some_and(|id| id != r.id) {
                    return Err(Error::Invalid("Resource id is immutable.".into()));
                }
                return transact(self.service.repo.as_ref(), |s| {
                    validate_route(&r, s)?;
                    if method == Method::POST && s.cache_routes.contains_key(&r.id) {
                        return Err(Error::Conflict);
                    }
                    s.cache_routes.insert(r.id.clone(), r.clone());
                    s.cache_entries.retain(|_, e| e.route_id != r.id);
                    Ok(json!(r))
                })
                .await;
            }
        }
        Err(Error::NotFound)
    }
}
fn revoke(s: &mut State, id: &str, admin: bool) {
    if admin {
        s.admin_sessions.retain(|_, v| v.admin_id != id);
    } else {
        s.tokens.retain(|_, v| v.user_id != id);
        s.joins.retain(|_, j| s.tokens.contains_key(&j.token_hash));
    }
}
async fn dispatch(
    AxumState(m): AxumState<Management>,
    r: Request,
) -> std::result::Result<Response, ApiError> {
    let path = r
        .uri()
        .path()
        .strip_prefix("/api/v1/")
        .ok_or(Error::NotFound)?
        .to_owned();
    let method = r.method().clone();
    let headers = r.headers().clone();
    let peer = r
        .extensions()
        .get::<axum::extract::ConnectInfo<std::net::SocketAddr>>()
        .map(|v| v.0.ip().to_string())
        .unwrap_or_else(|| "embedded".into());
    let q: BTreeMap<String, String> =
        url::form_urlencoded::parse(r.uri().query().unwrap_or("").as_bytes())
            .into_owned()
            .collect();
    let own_account = matches!(path.as_str(), "account" | "account/password");
    let auth = if own_account
        || (matches!(path.as_str(), "auth/login" | "auth/register") && method == Method::POST)
    {
        None
    } else {
        Some(
            m.authorize(&headers, method != Method::GET && method != Method::HEAD)
                .await?,
        )
    };
    let bytes = to_bytes(r.into_body(), MAX_TEXTURE + 65536)
        .await
        .map_err(|_| Error::Invalid("Request body too large.".into()))?
        .to_vec();
    let image = headers.get("content-type").and_then(|v| v.to_str().ok()) == Some("image/png");
    let body = if bytes.is_empty() || image {
        json!({})
    } else {
        if !headers
            .get("content-type")
            .and_then(|v| v.to_str().ok())
            .is_some_and(|v| v.starts_with("application/json"))
        {
            return Err(Error::Invalid("Expected application/json.".into()).into());
        }
        serde_json::from_slice(&bytes).map_err(|_| Error::Invalid("Invalid JSON.".into()))?
    };
    if path == "auth/register" && method == Method::POST {
        return Ok(m.register(body, &headers, &peer).await?);
    }
    if own_account {
        return Ok(m.account(&method, &path, body, &headers).await?);
    }
    if path == "auth/login" && method == Method::POST {
        return Ok(m.login(body, &headers).await?);
    }
    let (session, user) = auth.ok_or(Error::Unauthorized)?;
    if path == "auth/me" && method == Method::GET {
        return Ok(json_ok(public_user(&user)));
    }
    if path == "auth/logout" && method == Method::POST {
        transact(m.service.repo.as_ref(), |s| {
            s.admin_sessions.remove(&session);
            Ok(())
        })
        .await?;
        let mut r = json_ok(json!({}));
        r.headers_mut().insert(
            "set-cookie",
            HeaderValue::from_static(
                "ygg_session=; Path=/api/v1; HttpOnly; SameSite=Strict; Max-Age=0",
            ),
        );
        return Ok(r);
    }
    Ok(json_ok(
        m.resource(&method, &path, body, q, &headers, bytes).await?,
    ))
}
async fn openapi() -> axum::Json<Value> {
    axum::Json(openapi_document())
}
pub fn openapi_document() -> Value {
    let mut paths = serde_json::Map::new();
    for (path, methods) in [
        ("auth/login", vec!["post"]),
        ("auth/register", vec!["post"]),
        ("account", vec!["get"]),
        ("account/password", vec!["post"]),
        ("auth/logout", vec!["post"]),
        ("auth/me", vec!["get"]),
        ("status", vec!["get"]),
        ("capabilities", vec!["get"]),
        ("users", vec!["get", "post"]),
        ("users/{id}", vec!["get", "patch", "delete"]),
        ("users/{id}/password", vec!["post"]),
        ("users/{id}/revoke-sessions", vec!["post"]),
        ("admins", vec!["get", "post"]),
        ("admins/{id}", vec!["get", "patch", "delete"]),
        ("admins/{id}/password", vec!["post"]),
        ("profiles", vec!["get", "post"]),
        ("profiles/{id}", vec!["get", "patch", "delete"]),
        ("profiles/{id}/rename", vec!["post"]),
        ("profiles/{id}/history", vec!["get"]),
        ("profiles/{id}/mirror/minecraft", vec!["post"]),
        ("profiles/{id}/skin", vec!["put", "delete"]),
        ("profiles/{id}/cape", vec!["put", "delete"]),
        ("tokens", vec!["get"]),
        ("tokens/{id}", vec!["delete"]),
        ("sessions", vec!["get"]),
        ("sessions/{id}", vec!["delete"]),
        ("upstreams", vec!["get", "post"]),
        ("upstreams/{id}", vec!["get", "patch", "delete"]),
        ("upstreams/{id}/probe", vec!["post"]),
        ("upstreams/{id}/health", vec!["get"]),
        ("federation/rules", vec!["get", "post"]),
        ("federation/rules/{id}", vec!["get", "patch", "delete"]),
        ("federation/rules/validate", vec!["post"]),
        ("cache/routes", vec!["get", "post"]),
        ("cache/routes/{id}", vec!["get", "patch", "delete"]),
        ("cache/stats", vec!["get"]),
        ("cache/purge", vec!["post"]),
        ("launcher/cache", vec!["put"]),
        ("account-snapshots/{id}", vec!["get", "put", "delete"]),
        ("account-snapshots/{id}/session", vec!["post"]),
    ] {
        let mut ops = serde_json::Map::new();
        for method in methods {
            let mut op = json!({"operationId":format!("{}_{}",method,path.replace('/' ,"_").replace(['{','}'],"")),"responses":{"200":{"description":"Success","content":{"application/json":{"schema":{"type":"object"}}}},"default":{"description":"API error","content":{"application/json":{"schema":{"$ref":"#/components/schemas/Error"}}}}},"security":[{"bearerAuth":[]},{"cookieAuth":[]}]});
            if path.contains("{id}") {
                op["parameters"] =
                    json!([{"name":"id","in":"path","required":true,"schema":{"type":"string"}}]);
            }
            if matches!(method, "post" | "patch" | "put") {
                let (content, schema) = if method == "put" {
                    ("image/png", json!({"type":"string","format":"binary"}))
                } else {
                    (
                        "application/json",
                        json!({"type":"object","additionalProperties":true}),
                    )
                };
                op["requestBody"] = json!({"content":{content:{"schema":schema}}});
            }
            if matches!(path, "auth/login" | "auth/register") {
                op["security"] = json!([]);
            }
            if matches!(path, "account" | "account/password") {
                op["security"] = json!([{"localAccountToken":[]}]);
                op["description"] = json!("Local Yggdrasil access token, not an administrator token or upstream credential.");
            }
            ops.insert(method.into(), op);
        }
        paths.insert(format!("/api/v1/{path}"), Value::Object(ops));
    }
    let mut document = json!({"openapi":"3.0.3","info":{"title":"Yggdrasil management API","version":"1.0.0"},"paths":paths,"components":{"securitySchemes":{"bearerAuth":{"type":"http","scheme":"bearer"},"cookieAuth":{"type":"apiKey","in":"cookie","name":"ygg_session"}},"schemas":{"Error":{"type":"object","properties":{"error":{"type":"object","required":["code","message","details"],"properties":{"code":{"type":"string"},"message":{"type":"string"},"details":{"type":"object"}}}}}}}});
    schemas::enhance(&mut document);
    document["components"]["securitySchemes"]["localAccountToken"] =
        json!({"type":"http","scheme":"bearer"});
    document
}
