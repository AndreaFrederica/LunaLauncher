use argon2::{
    password_hash::{PasswordHash, SaltString},
    Argon2, PasswordHasher, PasswordVerifier,
};
use base64::{engine::general_purpose::STANDARD as B64, Engine};
use rsa::{
    pkcs8::{DecodePrivateKey, EncodePrivateKey, EncodePublicKey, LineEnding},
    signature::{SignatureEncoding, Signer},
    RsaPrivateKey,
};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::{collections::BTreeMap, io::Cursor, path::Path, sync::Arc};
use tokio::sync::Mutex;
use yggdrasil_core::*;

pub const MAX_TEXTURE: usize = 2 * 1024 * 1024;
#[derive(Clone)]
pub struct Service {
    pub repo: Arc<dyn Repository>,
    pub blobs: Arc<dyn BlobStore>,
    pub public_url: String,
    key: Arc<RsaPrivateKey>,
    attempts: Arc<Mutex<BTreeMap<String, (u64, u32)>>>,
}
#[derive(Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Credentials {
    pub username: String,
    pub password: String,
    pub client_token: Option<String>,
    #[serde(default)]
    pub request_user: bool,
}
#[derive(Clone, Serialize, Deserialize)]
pub struct ProfileRef {
    pub id: String,
    pub name: String,
}
impl From<&Profile> for ProfileRef {
    fn from(p: &Profile) -> Self {
        Self {
            id: p.id.clone(),
            name: p.name.clone(),
        }
    }
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct AuthResponse {
    pub access_token: String,
    pub client_token: String,
    pub available_profiles: Vec<ProfileRef>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub selected_profile: Option<ProfileRef>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub user: Option<Value>,
}
#[derive(Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct TokenRequest {
    pub access_token: String,
    pub client_token: Option<String>,
    pub selected_profile: Option<ProfileRef>,
    #[serde(default)]
    pub request_user: bool,
}
#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct JoinRequest {
    pub access_token: String,
    pub selected_profile: String,
    pub server_id: String,
}

pub async fn password_hash(password: String) -> Result<String> {
    if password.len() < 8 || password.len() > 1024 {
        return Err(Error::Invalid(
            "Password must contain 8 to 1024 bytes.".into(),
        ));
    }
    tokio::task::spawn_blocking(move || {
        Argon2::default()
            .hash_password(
                password.as_bytes(),
                &SaltString::generate(&mut rand::rngs::OsRng),
            )
            .map(|v| v.to_string())
            .map_err(|_| Error::Internal)
    })
    .await
    .map_err(|_| Error::Internal)?
}
async fn verify(password: String, encoded: String) -> Result<()> {
    if password.len() > 1024 {
        return Err(Error::Forbidden);
    }
    tokio::task::spawn_blocking(move || {
        let parsed = PasswordHash::new(&encoded).map_err(|_| Error::Forbidden)?;
        Argon2::default()
            .verify_password(password.as_bytes(), &parsed)
            .map_err(|_| Error::Forbidden)
    })
    .await
    .map_err(|_| Error::Internal)?
}
impl Service {
    pub async fn open(
        repo: Arc<dyn Repository>,
        blobs: Arc<dyn BlobStore>,
        public_url: String,
        key_file: &Path,
    ) -> Result<Self> {
        let url = url::Url::parse(&public_url)
            .map_err(|_| Error::Invalid("Invalid public_url.".into()))?;
        if !matches!(url.scheme(), "http" | "https")
            || url.host_str().is_none()
            || url.query().is_some()
            || url.fragment().is_some()
        {
            return Err(Error::Invalid(
                "public_url must be an HTTP(S) API root.".into(),
            ));
        }
        let path = key_file.to_owned();
        let key = tokio::task::spawn_blocking(move || -> Result<RsaPrivateKey> {
            if path.exists() {
                return RsaPrivateKey::from_pkcs8_pem(
                    &std::fs::read_to_string(&path).map_err(|_| Error::Internal)?,
                )
                .map_err(|_| Error::Internal);
            }
            let key =
                RsaPrivateKey::new(&mut rand::rngs::OsRng, 2048).map_err(|_| Error::Internal)?;
            let pem = key
                .to_pkcs8_pem(LineEnding::LF)
                .map_err(|_| Error::Internal)?;
            if let Some(parent) = path.parent() {
                std::fs::create_dir_all(parent).map_err(|_| Error::Internal)?;
            }
            let mut opts = std::fs::OpenOptions::new();
            opts.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                opts.mode(0o600);
            }
            use std::io::Write;
            // Publish a complete private file atomically, without replacing a
            // key another process may have created concurrently.
            let temp = path.with_extension(format!("{}.tmp", id()));
            let mut f = opts.open(&temp).map_err(|_| Error::Internal)?;
            f.write_all(pem.as_bytes()).map_err(|_| Error::Internal)?;
            f.sync_all().map_err(|_| Error::Internal)?;
            drop(f);
            let published = std::fs::hard_link(&temp, &path);
            let _ = std::fs::remove_file(&temp);
            match published {
                Ok(()) => Ok(key),
                Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {
                    RsaPrivateKey::from_pkcs8_pem(
                        &std::fs::read_to_string(path).map_err(|_| Error::Internal)?,
                    )
                    .map_err(|_| Error::Internal)
                }
                Err(_) => Err(Error::Internal),
            }
        })
        .await
        .map_err(|_| Error::Internal)??;
        Ok(Self {
            repo,
            blobs,
            public_url: public_url.trim_end_matches('/').into(),
            key: Arc::new(key),
            attempts: Arc::default(),
        })
    }
    pub fn public_key(&self) -> Result<String> {
        self.key
            .to_public_key()
            .to_public_key_pem(LineEnding::LF)
            .map_err(|_| Error::Internal)
    }
    pub fn sign(&self, value: &str) -> String {
        B64.encode(
            rsa::pkcs1v15::SigningKey::<sha1::Sha1>::new((*self.key).clone())
                .sign(value.as_bytes())
                .to_bytes(),
        )
    }
    pub fn metadata(&self) -> Result<Value> {
        Ok(
            json!({"meta":{"serverName":"Luna Enhanced Offline","implementationName":"yggdrasil-rs","implementationVersion":env!("CARGO_PKG_VERSION"),"feature.non_email_login":true,"feature.legacy_skin_api":true,"feature.no_mojang_namespace":true,"feature.enable_mojang_anti_features":false,"feature.enable_profile_key":false,"feature.username_check":true},"skinDomains":[url::Url::parse(&self.public_url).map_err(|_|Error::Internal)?.host_str()],"signaturePublickey":self.public_key()?}),
        )
    }
    pub async fn rate_limit(&self, username: &str) -> Result<()> {
        let mut map = self.attempts.lock().await;
        let t = now();
        map.retain(|_, (start, _)| *start + 60 > t);
        if map.len() >= 4096 && !map.contains_key(username) {
            return Err(Error::RateLimited);
        }
        let e = map.entry(username.to_lowercase()).or_insert((t, 0));
        e.1 += 1;
        if e.1 > 10 {
            Err(Error::RateLimited)
        } else {
            Ok(())
        }
    }
    pub async fn check_credentials(&self, c: &Credentials, admin: bool) -> Result<User> {
        if c.username.is_empty()
            || c.username.len() > 128
            || c.password.len() > 1024
            || c.client_token.as_ref().is_some_and(|t| t.len() > 256)
        {
            return Err(Error::Invalid(
                "Credential fields exceed supported limits.".into(),
            ));
        }
        self.rate_limit(&format!("{}:{}", admin, c.username))
            .await?;
        let (_, s) = self.repo.load().await?;
        let users = if admin { &s.admins } else { &s.users };
        let user = users
            .values()
            .find(|u| u.enabled && u.username.eq_ignore_ascii_case(&c.username))
            .ok_or(Error::Forbidden)?
            .clone();
        verify(c.password.clone(), user.password_hash.clone()).await?;
        Ok(user)
    }
    pub async fn create_user(
        &self,
        username: String,
        password: String,
        admin: bool,
    ) -> Result<User> {
        if username.is_empty() || username.len() > 128 || (!admin && username.contains("::")) {
            return Err(Error::Invalid("Invalid username.".into()));
        }
        let user = User {
            id: id(),
            username,
            password_hash: password_hash(password).await?,
            enabled: true,
        };
        transact(self.repo.as_ref(), |s| {
            let users = if admin { &mut s.admins } else { &mut s.users };
            if users
                .values()
                .any(|u| u.username.eq_ignore_ascii_case(&user.username))
            {
                return Err(Error::Conflict);
            }
            users.insert(user.id.clone(), user.clone());
            Ok(user.clone())
        })
        .await
    }
    /// Register a local account atomically. Public registration only creates an
    /// offline-player profile, so callers cannot claim an arbitrary official UUID.
    pub async fn register(
        &self,
        username: String,
        password: String,
        profile_name: Option<String>,
        client_token: Option<String>,
        request_user: bool,
    ) -> Result<(User, AuthResponse)> {
        if username.trim().is_empty() || username.len() > 128 || username.contains("::") {
            return Err(Error::Invalid("Invalid username.".into()));
        }
        if profile_name.as_ref().is_some_and(|name| !valid_name(name)) {
            return Err(Error::Invalid("Invalid profile name.".into()));
        }
        if client_token.as_ref().is_some_and(|t| t.len() > 256) {
            return Err(Error::Invalid("Client token is too long.".into()));
        }
        self.rate_limit(&format!("register:{username}")).await?;
        let password_hash = password_hash(password).await?;
        let user = User {
            id: id(),
            username,
            password_hash,
            enabled: true,
        };
        let profile = profile_name.map(|name| {
            let scheme = UuidScheme::OfflinePlayer;
            Profile {
                id: scheme.resolve(&name).simple().to_string(),
                user_id: user.id.clone(),
                name: name.clone(),
                uuid_scheme: scheme,
                skin: None,
                cape: None,
                slim: false,
                history: vec![NameHistory {
                    name,
                    changed_to_at: now() * 1000,
                }],
                mirrored_at: None,
            }
        });
        let raw = secret();
        let client = client_token.unwrap_or_else(secret);
        let selected = profile.as_ref().map(ProfileRef::from);
        let response = AuthResponse {
            access_token: raw.clone(),
            client_token: client.clone(),
            available_profiles: selected.iter().cloned().collect(),
            selected_profile: selected,
            user: request_user.then(|| json!({"id":user.id,"properties":[]})),
        };
        transact(self.repo.as_ref(), |s| {
            if s.users
                .values()
                .any(|u| u.username.eq_ignore_ascii_case(&user.username))
            {
                return Err(Error::Conflict);
            }
            if let Some(p) = &profile {
                if s.profiles.contains_key(&p.id)
                    || s.profiles
                        .values()
                        .any(|x| x.name.eq_ignore_ascii_case(&p.name))
                {
                    return Err(Error::Conflict);
                }
                s.profiles.insert(p.id.clone(), p.clone());
            }
            s.users.insert(user.id.clone(), user.clone());
            s.tokens.insert(
                hash(&raw),
                Token {
                    id: id(),
                    user_id: user.id.clone(),
                    client_token: client.clone(),
                    profile_id: profile.as_ref().map(|p| p.id.clone()),
                    expires: now() + 30 * 86400,
                    temporary_invalid: false,
                },
            );
            Ok((user.clone(), response.clone()))
        })
        .await
    }
    pub async fn create_profile(
        &self,
        user_id: String,
        name: String,
        scheme: UuidScheme,
    ) -> Result<Profile> {
        if !valid_name(&name) {
            return Err(Error::Invalid(
                "Profile names must contain 1–16 ASCII letters, digits or underscores.".into(),
            ));
        }
        let p = Profile {
            id: scheme.resolve(&name).simple().to_string(),
            user_id,
            name: name.clone(),
            uuid_scheme: scheme,
            skin: None,
            cape: None,
            slim: false,
            history: vec![NameHistory {
                name,
                changed_to_at: now() * 1000,
            }],
            mirrored_at: None,
        };
        transact(self.repo.as_ref(), |s| {
            if !s.users.contains_key(&p.user_id) {
                return Err(Error::NotFound);
            }
            if s.profiles.contains_key(&p.id)
                || s.profiles
                    .values()
                    .any(|x| x.name.eq_ignore_ascii_case(&p.name))
            {
                return Err(Error::Conflict);
            }
            s.profiles.insert(p.id.clone(), p.clone());
            Ok(p.clone())
        })
        .await
    }
    pub async fn rename(&self, id: &str, name: String) -> Result<Profile> {
        if !valid_name(&name) {
            return Err(Error::Invalid("Invalid profile name.".into()));
        }
        transact(self.repo.as_ref(), |s| {
            if s.profiles
                .values()
                .any(|p| p.id != id && p.name.eq_ignore_ascii_case(&name))
            {
                return Err(Error::Conflict);
            }
            let p = s.profiles.get_mut(id).ok_or(Error::NotFound)?;
            if p.name != name {
                p.name = name.clone();
                p.history.push(NameHistory {
                    name: name.clone(),
                    changed_to_at: now() * 1000,
                });
                for t in s
                    .tokens
                    .values_mut()
                    .filter(|t| t.profile_id.as_deref() == Some(id))
                {
                    t.temporary_invalid = true;
                }
            }
            Ok(p.clone())
        })
        .await
    }
    pub async fn authenticate(&self, c: Credentials) -> Result<AuthResponse> {
        let u = self.check_credentials(&c, false).await?;
        let raw = secret();
        let client = c.client_token.unwrap_or_else(secret);
        transact(self.repo.as_ref(), |s| {
            if !s
                .users
                .get(&u.id)
                .is_some_and(|v| v.enabled && v.password_hash == u.password_hash)
            {
                return Err(Error::Forbidden);
            }
            let profiles: Vec<ProfileRef> = s
                .profiles
                .values()
                .filter(|p| p.user_id == u.id)
                .map(ProfileRef::from)
                .collect();
            let selected = if profiles.len() == 1 {
                Some(profiles[0].clone())
            } else {
                None
            };
            s.tokens.insert(
                hash(&raw),
                Token {
                    id: id(),
                    user_id: u.id.clone(),
                    client_token: client.clone(),
                    profile_id: selected.as_ref().map(|p| p.id.clone()),
                    expires: now() + 30 * 86400,
                    temporary_invalid: false,
                },
            );
            Ok(AuthResponse {
                access_token: raw.clone(),
                client_token: client.clone(),
                available_profiles: profiles,
                selected_profile: selected,
                user: c.request_user.then(|| json!({"id":u.id,"properties":[]})),
            })
        })
        .await
    }
    pub async fn refresh(&self, r: TokenRequest) -> Result<AuthResponse> {
        let old_hash = hash(&r.access_token);
        let raw = secret();
        transact(self.repo.as_ref(), |s| {
            let mut t = checked_token(s, &r.access_token, r.client_token.as_deref(), true)?.clone();
            if let Some(p) = &r.selected_profile {
                if t.profile_id.is_some() {
                    return Err(Error::Forbidden);
                }
                let pid = canonical_uuid(&p.id)?;
                let profile = s.profiles.get(&pid).ok_or(Error::Forbidden)?;
                if profile.user_id != t.user_id || profile.name != p.name {
                    return Err(Error::Forbidden);
                }
                t.profile_id = Some(pid);
            }
            let profiles: Vec<ProfileRef> = s
                .profiles
                .values()
                .filter(|p| p.user_id == t.user_id)
                .map(ProfileRef::from)
                .collect();
            let selected = t
                .profile_id
                .as_ref()
                .and_then(|id| s.profiles.get(id))
                .map(ProfileRef::from);
            t.expires = now() + 30 * 86400;
            t.temporary_invalid = false;
            t.id = id();
            let response = AuthResponse {
                access_token: raw.clone(),
                client_token: t.client_token.clone(),
                available_profiles: profiles,
                selected_profile: selected,
                user: r
                    .request_user
                    .then(|| json!({"id":t.user_id,"properties":[]})),
            };
            s.tokens.remove(&old_hash);
            s.joins.retain(|_, j| j.token_hash != old_hash);
            s.tokens.insert(hash(&raw), t);
            Ok(response)
        })
        .await
    }
    pub async fn validate(&self, r: &TokenRequest) -> Result<()> {
        let (_, s) = self.repo.load().await?;
        checked_token(&s, &r.access_token, r.client_token.as_deref(), false)?;
        Ok(())
    }
    pub async fn change_own_password(&self, raw: &str, current: String, new: String) -> Result<()> {
        let (_, state) = self.repo.load().await?;
        let token = checked_token(&state, raw, None, false)?;
        let user = state.users.get(&token.user_id).ok_or(Error::Forbidden)?;
        self.rate_limit(&format!("password:{}", user.id)).await?;
        verify(current, user.password_hash.clone()).await?;
        let password = password_hash(new).await?;
        transact(self.repo.as_ref(), |s| {
            let t = checked_token(s, raw, None, false)?;
            if t.user_id != user.id || s.users[&user.id].password_hash != user.password_hash {
                return Err(Error::Forbidden);
            }
            s.users
                .get_mut(&user.id)
                .ok_or(Error::Forbidden)?
                .password_hash = password.clone();
            s.tokens.retain(|_, t| t.user_id != user.id);
            s.joins.retain(|_, j| s.tokens.contains_key(&j.token_hash));
            Ok(())
        })
        .await
    }
    pub async fn invalidate(&self, raw: &str) -> Result<()> {
        let h = hash(raw);
        transact(self.repo.as_ref(), |s| {
            s.tokens.remove(&h);
            s.joins.retain(|_, j| j.token_hash != h);
            Ok(())
        })
        .await
    }
    pub async fn signout(&self, c: Credentials) -> Result<()> {
        let u = self.check_credentials(&c, false).await?;
        transact(self.repo.as_ref(), |s| {
            s.tokens.retain(|_, t| t.user_id != u.id);
            s.joins.retain(|_, j| s.tokens.contains_key(&j.token_hash));
            Ok(())
        })
        .await
    }
    pub async fn join(&self, r: JoinRequest, ip: String) -> Result<()> {
        if r.server_id.is_empty() || r.server_id.len() > 256 {
            return Err(Error::Invalid("Invalid serverId.".into()));
        }
        let pid = canonical_uuid(&r.selected_profile)?;
        transact(self.repo.as_ref(), |s| {
            let t = checked_token(s, &r.access_token, None, false)?;
            if t.profile_id.as_deref() != Some(&pid) {
                return Err(Error::Forbidden);
            }
            s.joins.insert(
                hash(format!("{}:{}", r.server_id, pid)),
                Join {
                    profile_id: pid.clone(),
                    token_hash: hash(&r.access_token),
                    ip: ip.clone(),
                    expires: now() + 30,
                },
            );
            Ok(())
        })
        .await
    }
    pub async fn has_joined(
        &self,
        server: &str,
        name: &str,
        ip: Option<&str>,
    ) -> Result<Option<Value>> {
        let (_, s) = self.repo.load().await?;
        let Some(p) = s
            .profiles
            .values()
            .find(|p| p.name.eq_ignore_ascii_case(name))
        else {
            return Ok(None);
        };
        let Some(j) = s.joins.get(&hash(format!("{}:{}", server, p.id))) else {
            return Ok(None);
        };
        if j.expires <= now()
            || ip.is_some_and(|ip| ip != j.ip)
            || !s
                .tokens
                .get(&j.token_hash)
                .is_some_and(|t| t.expires > now() && !t.temporary_invalid)
        {
            return Ok(None);
        }
        Ok(Some(self.profile_json(p, true)?))
    }
    pub fn profile_json(&self, p: &Profile, signed: bool) -> Result<Value> {
        let mut textures = json!({});
        if let Some(h) = &p.skin {
            textures["SKIN"] = json!({"url":format!("{}/textures/{}",self.public_url,h),"metadata":{"model":if p.slim{"slim"}else{"default"}}});
        }
        if let Some(h) = &p.cape {
            textures["CAPE"] = json!({"url":format!("{}/textures/{}",self.public_url,h)});
        }
        let value=B64.encode(serde_json::to_vec(&json!({"timestamp":now()*1000,"profileId":p.id,"profileName":p.name,"textures":textures,"signatureRequired":signed})).map_err(|_|Error::Internal)?);
        let mut property = json!({"name":"textures","value":value});
        if signed {
            property["signature"] = json!(self.sign(&value));
        }
        Ok(json!({"id":p.id,"name":p.name,"properties":[property]}))
    }
    pub fn resign(&self, mut p: Value, signed: bool) -> Value {
        if let Some(props) = p["properties"].as_array_mut() {
            for prop in props {
                if let Some(v) = prop["value"].as_str() {
                    let sig = self.sign(v);
                    if let Some(obj) = prop.as_object_mut() {
                        obj.remove("signature");
                        if signed {
                            obj.insert("signature".into(), json!(sig));
                        }
                    }
                }
            }
        }
        p
    }
    pub async fn texture(&self, pid: &str, kind: &str, bytes: Vec<u8>, slim: bool) -> Result<()> {
        let kind = kind.to_owned();
        let k = kind.clone();
        let sanitized = tokio::task::spawn_blocking(move || sanitize_texture(&bytes, &k))
            .await
            .map_err(|_| Error::Internal)??;
        let h = self.blobs.put(&sanitized).await?;
        transact(self.repo.as_ref(), |s| {
            let p = s.profiles.get_mut(pid).ok_or(Error::NotFound)?;
            match kind.as_str() {
                "skin" => {
                    p.skin = Some(h.clone());
                    p.slim = slim
                }
                "cape" => p.cape = Some(h.clone()),
                _ => return Err(Error::Invalid("Unknown texture type.".into())),
            }
            Ok(())
        })
        .await
    }
    pub async fn delete_texture(&self, pid: &str, kind: &str) -> Result<()> {
        transact(self.repo.as_ref(), |s| {
            let p = s.profiles.get_mut(pid).ok_or(Error::NotFound)?;
            match kind {
                "skin" => p.skin = None,
                "cape" => p.cape = None,
                _ => return Err(Error::Invalid("Unknown texture type.".into())),
            }
            Ok(())
        })
        .await
    }
    pub async fn authorize_profile(&self, raw: &str, pid: &str) -> Result<()> {
        let (_, s) = self.repo.load().await?;
        let t = checked_token(&s, raw, None, false)?;
        if s.profiles.get(pid).is_none_or(|p| p.user_id != t.user_id) {
            return Err(Error::Forbidden);
        }
        Ok(())
    }
    pub async fn cleanup(&self) -> Result<()> {
        transact(self.repo.as_ref(), |s| {
            let t = now();
            s.tokens.retain(|_, v| v.expires > t);
            s.joins
                .retain(|_, v| v.expires > t && s.tokens.contains_key(&v.token_hash));
            s.admin_sessions.retain(|_, v| v.expires > t);
            s.affinities.retain(|_, v| v.expires > t);
            s.cache_entries.retain(|_, v| v.stale_until > t);
            Ok(())
        })
        .await
    }
}
pub fn checked_token<'a>(
    s: &'a State,
    raw: &str,
    client: Option<&str>,
    temporary: bool,
) -> Result<&'a Token> {
    let t = s.tokens.get(&hash(raw)).ok_or(Error::Forbidden)?;
    if t.expires <= now()
        || (!temporary && t.temporary_invalid)
        || client.is_some_and(|c| c != t.client_token)
        || !s.users.get(&t.user_id).is_some_and(|u| u.enabled)
    {
        return Err(Error::Forbidden);
    }
    Ok(t)
}
pub fn sanitize_texture(bytes: &[u8], kind: &str) -> Result<Vec<u8>> {
    if bytes.len() > MAX_TEXTURE {
        return Err(Error::Invalid("Texture too large.".into()));
    }
    let bad = || Error::Invalid("Invalid PNG or Minecraft texture dimensions.".into());
    let reader = image::ImageReader::with_format(Cursor::new(bytes), image::ImageFormat::Png);
    let (w, h) = reader.into_dimensions().map_err(|_| bad())?;
    let valid = match kind {
        "skin" => w == 64 && (h == 32 || h == 64),
        "cape" => (w == 64 && h == 32) || (w == 22 && h == 17),
        _ => false,
    };
    if !valid || u64::from(w) * u64::from(h) > 4096 {
        return Err(bad());
    }
    let mut img = image::load_from_memory_with_format(bytes, image::ImageFormat::Png)
        .map_err(|_| bad())?
        .to_rgba8();
    if kind == "cape" && w == 22 {
        let mut padded = image::RgbaImage::new(64, 32);
        image::imageops::overlay(&mut padded, &img, 0, 0);
        img = padded;
    }
    let mut out = Cursor::new(Vec::new());
    img.write_to(&mut out, image::ImageFormat::Png)
        .map_err(|_| bad())?;
    Ok(out.into_inner())
}
