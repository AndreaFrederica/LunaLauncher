use base64::{engine::general_purpose::STANDARD as B64, Engine};
use http::{HeaderMap, HeaderValue, Method};
use serde_json::{json, Value};
use std::{
    collections::{BTreeMap, BTreeSet},
    sync::Arc,
};
use yggdrasil_core::*;
use yggdrasil_service::{Credentials, JoinRequest, Service, TokenRequest};
use yggdrasil_upstream::{Clients, Response};

#[derive(Clone)]
pub struct Federation {
    pub local: Arc<Service>,
    pub clients: Clients,
}
pub struct Request {
    pub operation: Operation,
    pub method: Method,
    pub path: String,
    pub headers: HeaderMap,
    pub body: Vec<u8>,
    pub ip: String,
}
pub fn json_response(value: Value) -> Result<Response> {
    Ok(Response {
        status: 200,
        headers: HeaderMap::from_iter([(
            http::header::CONTENT_TYPE,
            HeaderValue::from_static("application/json"),
        )]),
        body: serde_json::to_vec(&value).map_err(|_| Error::Internal)?,
    })
}
pub fn empty() -> Response {
    Response {
        status: 204,
        headers: HeaderMap::new(),
        body: vec![],
    }
}
pub fn validate_rule(rule: &Rule, state: &State) -> Result<()> {
    if rule.id.is_empty()
        || rule.targets.is_empty()
        || rule.strategy == Strategy::MergePriority && rule.operation != Operation::ProfileBatch
        || rule.strategy == Strategy::Pinned
            && (rule.targets.len() != 1 || rule.targets[0].source == "*")
    {
        return Err(Error::Invalid(
            "Invalid federation strategy or targets.".into(),
        ));
    }
    for t in &rule.targets {
        if t.source != "local" && t.source != "*" {
            let u = state
                .upstreams
                .get(&t.source)
                .ok_or_else(|| Error::Invalid("Unknown upstream target.".into()))?;
            if !u
                .capabilities
                .iter()
                .any(|c| c == rule.operation.capability())
            {
                return Err(Error::Invalid("Upstream lacks required capability.".into()));
            }
        }
    }
    if !rule.targets.iter().any(|t| {
        t.source == "local"
            || state.upstreams.values().any(|u| {
                u.enabled
                    && (t.source == "*" || t.source == u.id)
                    && u.capabilities
                        .iter()
                        .any(|c| c == rule.operation.capability())
            })
    }) {
        return Err(Error::Invalid("No eligible federation target.".into()));
    }
    Ok(())
}
impl Federation {
    async fn mirror_properties(
        &self,
        mut profile: Value,
        upstream: &Upstream,
        signed: bool,
    ) -> Result<Value> {
        if let Some(properties) = profile["properties"].as_array_mut() {
            for property in properties {
                if property["name"] != "textures" {
                    continue;
                }
                let encoded = property["value"].as_str().ok_or(Error::Upstream)?;
                let decoded = B64.decode(encoded).map_err(|_| Error::Upstream)?;
                let mut value: Value =
                    serde_json::from_slice(&decoded).map_err(|_| Error::Upstream)?;
                if !value.is_object() {
                    return Err(Error::Upstream);
                }
                value["signatureRequired"] = json!(signed);
                let base = url::Url::parse(&upstream.base_url).map_err(|_| Error::Upstream)?;
                for (key, kind) in [("SKIN", "skin"), ("CAPE", "cape")] {
                    if let Some(raw) = value["textures"][key]["url"].as_str() {
                        let target = url::Url::parse(raw).map_err(|_| Error::Upstream)?;
                        let host = target.host_str().ok_or(Error::Upstream)?;
                        if !matches!(target.scheme(), "http" | "https")
                            || !target.username().is_empty()
                            || target.password().is_some()
                            || !(target.origin() == base.origin()
                                || (target.scheme() == "https"
                                    && target.port().is_none()
                                    && upstream
                                        .texture_domains
                                        .iter()
                                        .any(|d| d.eq_ignore_ascii_case(host))))
                        {
                            return Err(Error::Upstream);
                        }
                        let mut transport = upstream.clone();
                        transport.id = format!(
                            "{}:textures:{}",
                            upstream.id,
                            target.origin().ascii_serialization()
                        );
                        transport.base_url = target.origin().ascii_serialization();
                        // Authority credentials must not travel to a texture CDN.
                        transport.extra_headers.clear();
                        let path = match target.query() {
                            Some(q) => format!("{}?{}", target.path(), q),
                            None => target.path().to_owned(),
                        };
                        let response = self
                            .clients
                            .fetch(
                                &transport,
                                Method::GET,
                                &path,
                                HeaderMap::new(),
                                vec![],
                                true,
                                yggdrasil_service::MAX_TEXTURE,
                            )
                            .await
                            .map_err(|_| Error::Upstream)?;
                        if response.status != 200 {
                            return Err(Error::Upstream);
                        }
                        let kind = kind.to_owned();
                        let clean = tokio::task::spawn_blocking(move || {
                            yggdrasil_service::sanitize_texture(&response.body, &kind)
                        })
                        .await
                        .map_err(|_| Error::Internal)??;
                        let digest = self.local.blobs.put(&clean).await?;
                        value["textures"][key]["url"] =
                            json!(format!("{}/textures/{}", self.local.public_url, digest));
                    }
                }
                property["value"] =
                    json!(B64.encode(serde_json::to_vec(&value).map_err(|_| Error::Internal)?));
            }
        }
        Ok(self.local.resign(profile, signed))
    }
    pub async fn execute(&self, mut r: Request) -> Result<Response> {
        let (_, s) = self.local.repo.load().await?;
        let mut body: Value = if r.body.is_empty() {
            Value::Null
        } else {
            serde_json::from_slice(&r.body).unwrap_or(Value::Null)
        };
        // Explicit source selection works with existing Yggdrasil clients by
        // qualifying the login name. Never forward the qualifier as a username.
        let mut credential_source = None;
        if matches!(r.operation, Operation::Authenticate | Operation::Signout) {
            let username = body["username"]
                .as_str()
                .ok_or_else(|| Error::Invalid("username is required.".into()))?
                .to_owned();
            if let Some((source, principal)) = username.split_once("::") {
                if principal.is_empty()
                    || (source != "local"
                        && !s.upstreams.get(source).is_some_and(|u| {
                            u.enabled && u.capabilities.iter().any(|c| c == "auth")
                        }))
                {
                    return Err(Error::Invalid(
                        "Unknown or unavailable login source.".into(),
                    ));
                }
                credential_source = Some(source.to_owned());
                body["username"] = json!(principal);
                r.body = serde_json::to_vec(&body).map_err(|_| Error::Internal)?;
            } else if s
                .users
                .values()
                .any(|u| u.username.eq_ignore_ascii_case(&username))
            {
                // Local ownership (including disabled accounts) is authoritative:
                // invalid local passwords must never be tried on remote sources.
                credential_source = Some("local".into());
            }
        }
        let query: BTreeMap<String, String> = url::form_urlencoded::parse(
            r.path.split_once('?').map(|v| v.1).unwrap_or("").as_bytes(),
        )
        .into_owned()
        .collect();
        let bearer = r
            .headers
            .get("authorization")
            .and_then(|v| v.to_str().ok())
            .and_then(|s| s.strip_prefix("Bearer "));
        let token = body["accessToken"].as_str().or(bearer);
        let affinity_key = if r.operation == Operation::HasJoined {
            query
                .get("serverId")
                .zip(query.get("username"))
                .map(|(sid, name)| {
                    format!("join:{}", hash(format!("{}:{}", sid, name.to_lowercase())))
                })
        } else {
            token.map(|t| format!("token:{}", hash(t)))
        };
        let forced = affinity_key
            .as_ref()
            .and_then(|k| s.affinities.get(k))
            .filter(|a| a.expires > now())
            .map(|a| a.upstream_id.clone());
        if matches!(r.operation, Operation::Authenticate | Operation::Signout) {
            let username = body["username"]
                .as_str()
                .ok_or_else(|| Error::Invalid("username is required.".into()))?;
            self.local
                .rate_limit(&format!("federation:{username}"))
                .await?;
        }
        // Only a join observed by this gateway can establish remote ownership.
        // This also makes management revocation effective for outstanding joins.
        if r.operation == Operation::HasJoined && forced.is_none() {
            return self.execute_local(&r, &body, &query).await;
        }
        let local_token = token.is_some_and(|t| s.tokens.contains_key(&hash(t)));
        let stateful = matches!(
            r.operation,
            Operation::Refresh
                | Operation::Validate
                | Operation::Invalidate
                | Operation::Join
                | Operation::TextureUpload
                | Operation::TextureDelete
                | Operation::PlayerCertificates
                | Operation::Privileges
                | Operation::PlayerAttributes
                | Operation::Blocklist
        );
        // Unknown bearer credentials must never be broadcast to candidate authorities.
        if (stateful || token.is_some()) && !local_token && forced.is_none() {
            if r.operation == Operation::Invalidate {
                return Ok(empty());
            }
            return Err(Error::Forbidden);
        }
        let mut rules: Vec<&Rule> = s
            .rules
            .values()
            .filter(|rule| rule.enabled && rule.operation == r.operation)
            .collect();
        rules.sort_by_key(|rule| (rule.match_priority, &rule.id));
        let rule = rules.first().copied();
        let mut candidates: Vec<(i32, String)> = vec![];
        if local_token {
            candidates.push((0, "local".into()));
        } else if let Some(source) = forced {
            candidates.push((0, source));
        } else if let Some(source) = &credential_source {
            candidates.push((0, source.clone()));
        } else if let Some(rule) = rule {
            for target in &rule.targets {
                if target.source == "local" {
                    candidates.push((target.priority.unwrap_or(0), "local".into()))
                } else {
                    for u in s.upstreams.values().filter(|u| {
                        u.enabled
                            && (target.source == "*" || target.source == u.id)
                            && u.capabilities.iter().any(|c| c == r.operation.capability())
                    }) {
                        candidates.push((target.priority.unwrap_or(u.priority), u.id.clone()));
                    }
                }
            }
        } else {
            candidates.push((0, "local".into()));
        }
        candidates.sort();
        let mut seen = BTreeSet::new();
        candidates.retain(|(_, source)| seen.insert(source.clone()));
        let merge = rule.is_some_and(|r| r.strategy == Strategy::MergePriority);
        let pinned = credential_source.is_some()
            || stateful
            || affinity_key
                .as_ref()
                .is_some_and(|k| s.affinities.contains_key(k))
            || rule.is_some_and(|r| r.strategy == Strategy::Pinned);
        let signed = r.operation == Operation::HasJoined
            || query.get("unsigned").is_some_and(|v| v == "false");
        let mut merged: BTreeMap<String, Value> = BTreeMap::new();
        let mut available = false;
        for (_, source) in candidates {
            let result = if source == "local" {
                self.execute_local(&r, &body, &query).await
            } else {
                let u = s
                    .upstreams
                    .get(&source)
                    .filter(|u| u.enabled)
                    .ok_or(Error::Upstream)?;
                if !u.capabilities.iter().any(|c| c == r.operation.capability()) {
                    return Err(Error::Upstream);
                }
                if !pinned
                    && self
                        .clients
                        .health
                        .read()
                        .await
                        .get(&source)
                        .is_some_and(|h| h.healthy == Some(false))
                {
                    continue;
                }
                self.clients
                    .fetch(
                        u,
                        r.method.clone(),
                        &r.path,
                        r.headers.clone(),
                        r.body.clone(),
                        r.operation.safe(),
                        8 * 1024 * 1024,
                    )
                    .await
                    .map_err(|_| Error::Upstream)
            };
            let mut response = match result {
                Ok(v) => v,
                Err(Error::Upstream) if !pinned => continue,
                Err(e) => return Err(e),
            };
            if response.status >= 500 && !pinned {
                continue;
            }
            if (response.status == 204 || response.status == 404) && r.operation.safe() && !pinned {
                available = true;
                continue;
            }
            if response.status >= 400 {
                return Ok(response);
            } // Credentials and permissions are terminal.
            available = true;
            if source != "local" && response.status != 204 {
                let mut value: Value =
                    serde_json::from_slice(&response.body).map_err(|_| Error::Upstream)?;
                if matches!(r.operation, Operation::ProfileLookup | Operation::HasJoined) {
                    value = self
                        .mirror_properties(
                            value,
                            s.upstreams.get(&source).ok_or(Error::Upstream)?,
                            signed,
                        )
                        .await?;
                    response = json_response(value.clone())?;
                }
                if matches!(r.operation, Operation::Authenticate | Operation::Refresh) {
                    let raw = value["accessToken"].as_str().ok_or(Error::Upstream)?;
                    let username = value["selectedProfile"]["name"]
                        .as_str()
                        .map(str::to_owned)
                        .or_else(|| {
                            affinity_key
                                .as_ref()
                                .and_then(|k| s.affinities.get(k))
                                .and_then(|a| a.username.clone())
                        });
                    transact(self.local.repo.as_ref(), |s| {
                        let principal_hash = if r.operation == Operation::Authenticate {
                            body["username"].as_str().map(|u| hash(u.to_lowercase()))
                        } else {
                            affinity_key
                                .as_ref()
                                .and_then(|k| s.affinities.get(k))
                                .and_then(|a| a.principal_hash.clone())
                        };
                        if r.operation == Operation::Refresh {
                            if let Some(k) = &affinity_key {
                                s.affinities.remove(k);
                            }
                            if let Some(token) = token {
                                let h = hash(token);
                                s.affinities
                                    .retain(|_, a| a.token_hash.as_deref() != Some(&h));
                            }
                        }
                        s.affinities.insert(
                            format!("token:{}", hash(raw)),
                            Affinity {
                                upstream_id: source.clone(),
                                expires: now() + 30 * 86400,
                                username: username.clone(),
                                principal_hash: principal_hash.clone(),
                                token_hash: Some(hash(raw)),
                            },
                        );
                        Ok(())
                    })
                    .await?;
                }
            }
            if source != "local" && response.status < 300 {
                if r.operation == Operation::Join {
                    let sid = body["serverId"].as_str().ok_or(Error::Forbidden)?;
                    let affinity = affinity_key
                        .as_ref()
                        .and_then(|k| s.affinities.get(k))
                        .ok_or(Error::Forbidden)?;
                    let username = affinity.username.as_ref().ok_or(Error::Forbidden)?;
                    transact(self.local.repo.as_ref(), |s| {
                        s.affinities.insert(
                            format!(
                                "join:{}",
                                hash(format!("{}:{}", sid, username.to_lowercase()))
                            ),
                            Affinity {
                                upstream_id: source.clone(),
                                expires: now() + 30,
                                username: Some(username.clone()),
                                principal_hash: affinity.principal_hash.clone(),
                                token_hash: token.map(hash),
                            },
                        );
                        Ok(())
                    })
                    .await?;
                } else if r.operation == Operation::Invalidate {
                    if let Some(k) = &affinity_key {
                        transact(self.local.repo.as_ref(), |s| {
                            s.affinities.remove(k);
                            if let Some(token) = token {
                                let h = hash(token);
                                s.affinities
                                    .retain(|_, a| a.token_hash.as_deref() != Some(&h));
                            }
                            Ok(())
                        })
                        .await?;
                    }
                } else if r.operation == Operation::Signout {
                    let principal = body["username"].as_str().map(|u| hash(u.to_lowercase()));
                    transact(self.local.repo.as_ref(), |s| {
                        s.affinities.retain(|_, a| {
                            a.upstream_id != source || a.principal_hash != principal
                        });
                        Ok(())
                    })
                    .await?;
                }
            }
            if merge {
                let items: Vec<Value> =
                    serde_json::from_slice(&response.body).map_err(|_| Error::Upstream)?;
                for item in items {
                    let key = item["name"].as_str().ok_or(Error::Upstream)?.to_lowercase();
                    if let Some(old) = merged.get(&key) {
                        if rule.is_some_and(|r| r.reject_conflicts) && old["id"] != item["id"] {
                            return Err(Error::Conflict);
                        }
                    } else {
                        merged.insert(key, item);
                    }
                }
            } else {
                return Ok(response);
            }
        }
        if merge && available {
            return json_response(json!(merged.values().collect::<Vec<_>>()));
        }
        if available {
            return Ok(empty());
        }
        Err(Error::Upstream)
    }
    async fn execute_local(
        &self,
        r: &Request,
        b: &Value,
        q: &BTreeMap<String, String>,
    ) -> Result<Response> {
        if matches!(
            r.operation,
            Operation::Privileges
                | Operation::PlayerAttributes
                | Operation::Blocklist
                | Operation::PlayerCertificates
        ) {
            let raw = r
                .headers
                .get("authorization")
                .and_then(|h| h.to_str().ok())
                .and_then(|h| h.strip_prefix("Bearer "))
                .ok_or(Error::Forbidden)?;
            self.local
                .validate(&TokenRequest {
                    access_token: raw.into(),
                    client_token: None,
                    selected_profile: None,
                    request_user: false,
                })
                .await?;
        }
        let parse = |v: Value| {
            serde_json::from_value(v).map_err(|_| Error::Invalid("Invalid request body.".into()))
        };
        match r.operation {
            Operation::Authenticate => json_response(
                serde_json::to_value(self.local.authenticate(parse(b.clone())?).await?)
                    .map_err(|_| Error::Internal)?,
            ),
            Operation::Refresh => {
                let r: TokenRequest = serde_json::from_value(b.clone())
                    .map_err(|_| Error::Invalid("Invalid token request.".into()))?;
                json_response(
                    serde_json::to_value(self.local.refresh(r).await?)
                        .map_err(|_| Error::Internal)?,
                )
            }
            Operation::Validate => {
                self.local
                    .validate(
                        &serde_json::from_value(b.clone())
                            .map_err(|_| Error::Invalid("Invalid token request.".into()))?,
                    )
                    .await?;
                Ok(empty())
            }
            Operation::Invalidate => {
                self.local
                    .invalidate(b["accessToken"].as_str().ok_or(Error::Forbidden)?)
                    .await?;
                Ok(empty())
            }
            Operation::Signout => {
                let c: Credentials = serde_json::from_value(b.clone())
                    .map_err(|_| Error::Invalid("Invalid credentials.".into()))?;
                self.local.signout(c).await?;
                Ok(empty())
            }
            Operation::Join => {
                let j: JoinRequest = serde_json::from_value(b.clone())
                    .map_err(|_| Error::Invalid("Invalid join request.".into()))?;
                self.local.join(j, r.ip.clone()).await?;
                Ok(empty())
            }
            Operation::HasJoined => {
                let sid = q
                    .get("serverId")
                    .ok_or_else(|| Error::Invalid("serverId is required.".into()))?;
                let name = q
                    .get("username")
                    .ok_or_else(|| Error::Invalid("username is required.".into()))?;
                match self
                    .local
                    .has_joined(sid, name, q.get("ip").map(String::as_str))
                    .await?
                {
                    Some(p) => json_response(p),
                    None => Ok(empty()),
                }
            }
            Operation::ProfileLookup
            | Operation::ProfileByName
            | Operation::NameHistory
            | Operation::ProfileBatch => {
                let (_, s) = self.local.repo.load().await?;
                let path = r.path.split('?').next().unwrap_or("");
                if r.operation == Operation::ProfileBatch {
                    let names: Vec<String> = serde_json::from_value(b.clone())
                        .map_err(|_| Error::Invalid("Expected an array of names.".into()))?;
                    if names.len() > 100 {
                        return Err(Error::Invalid("At most 100 names per request.".into()));
                    }
                    return json_response(json!(s
                        .profiles
                        .values()
                        .filter(|p| names.iter().any(|n| n.eq_ignore_ascii_case(&p.name)))
                        .map(|p| json!({"id":p.id,"name":p.name}))
                        .collect::<Vec<_>>()));
                }
                let key = if r.operation == Operation::NameHistory {
                    path.strip_prefix("/user/profiles/")
                        .and_then(|p| p.strip_suffix("/names"))
                        .unwrap_or("")
                } else {
                    path.rsplit('/').next().unwrap_or("")
                };
                let p = if r.operation == Operation::ProfileByName {
                    s.profiles
                        .values()
                        .find(|p| p.name.eq_ignore_ascii_case(key))
                } else {
                    s.profiles.get(&canonical_uuid(key)?)
                };
                match p {
                    None => Ok(empty()),
                    Some(p) => match r.operation {
                        Operation::NameHistory => json_response(json!(p.history)),
                        Operation::ProfileByName => json_response(json!({"id":p.id,"name":p.name})),
                        _ => json_response(
                            self.local
                                .profile_json(p, q.get("unsigned").is_some_and(|v| v == "false"))?,
                        ),
                    },
                }
            }
            Operation::Privileges => json_response(
                json!({"privileges":{"onlineChat":{"enabled":true},"multiplayerServer":{"enabled":true},"multiplayerRealms":{"enabled":false},"telemetry":{"enabled":false}}}),
            ),
            Operation::PlayerAttributes => json_response(
                json!({"privileges":{"onlineChat":{"enabled":true},"multiplayerServer":{"enabled":true}},"profanityFilterPreferences":{"profanityFilterOn":false},"banStatus":{}}),
            ),
            Operation::Blocklist => json_response(json!({"blockedProfiles":[]})),
            Operation::PlayerCertificates => Err(Error::NotFound), // Never advertise unverified secure-chat certificates.
            Operation::TextureUpload | Operation::TextureDelete => Err(Error::Invalid(
                "Local textures require multipart handling.".into(),
            )),
        }
    }
}
