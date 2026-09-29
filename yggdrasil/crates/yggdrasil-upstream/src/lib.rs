//! Per-authority connection pools; no ambient proxy or cross-origin redirects.
use http::{HeaderMap, HeaderName, HeaderValue, Method};
use serde::Serialize;
use std::{
    collections::BTreeMap,
    sync::Arc,
    time::{Duration, Instant},
};
use tokio::sync::RwLock;
use yggdrasil_core::*;

#[derive(Clone)]
pub struct Response {
    pub status: u16,
    pub headers: HeaderMap,
    pub body: Vec<u8>,
}
#[derive(Clone, Debug)]
pub enum FetchError {
    Transport,
    TooLarge,
    Invalid,
}
#[derive(Clone, Serialize, Default)]
pub struct Health {
    pub healthy: Option<bool>,
    pub failures: u32,
    pub recoveries: u32,
    pub checked_at: u64,
    pub elapsed_ms: u64,
}
type Pool = BTreeMap<String, (String, reqwest::Client)>;
#[derive(Clone, Default)]
pub struct Clients {
    pool: Arc<RwLock<Pool>>,
    pub health: Arc<RwLock<BTreeMap<String, Health>>>,
}

pub fn validate_upstream(u: &Upstream) -> Result<()> {
    let url =
        url::Url::parse(&u.base_url).map_err(|_| Error::Invalid("Invalid upstream URL.".into()))?;
    if !matches!(url.scheme(), "http" | "https")
        || url.host_str().is_none()
        || !url.username().is_empty()
        || url.password().is_some()
        || url.query().is_some()
        || url.fragment().is_some()
    {
        return Err(Error::Invalid(
            "Upstream must be an HTTP(S) root without credentials, query or fragment.".into(),
        ));
    }
    if u.id.is_empty()
        || u.id == "local"
        || u.id == "*"
        || u.id.contains("::")
        || u.id.len() > 128
        || u.retry_count > 3
        || u.connect_timeout_ms == 0
        || u.request_timeout_ms == 0
        || u.request_timeout_ms > 300000
        || u.health.interval_seconds == 0
        || u.health.failure_threshold == 0
        || u.health.recovery_threshold == 0
    {
        return Err(Error::Invalid("Invalid upstream policy.".into()));
    }
    if !u.capabilities.iter().all(|c| {
        matches!(
            c.as_str(),
            "auth" | "session" | "profiles" | "textures" | "minecraft_services" | "http"
        )
    }) {
        return Err(Error::Invalid("Unknown upstream capability.".into()));
    }
    safe_url(u, &u.health.path)?;
    client_builder(u)?
        .build()
        .map_err(|_| Error::Invalid("Invalid upstream transport.".into()))?;
    Ok(())
}
fn client_builder(u: &Upstream) -> Result<reqwest::ClientBuilder> {
    let mut builder = reqwest::Client::builder()
        .no_proxy()
        .redirect(reqwest::redirect::Policy::none())
        .connect_timeout(Duration::from_millis(u.connect_timeout_ms))
        .timeout(Duration::from_millis(u.request_timeout_ms));
    let (proxy, schemes) = match &u.transport {
        Transport::Direct => (None, vec![]),
        Transport::HttpProxy { proxy_url } => (Some(proxy_url), vec!["http", "https"]),
        Transport::Socks5 { proxy_url } => (Some(proxy_url), vec!["socks5"]),
        Transport::Socks5h { proxy_url } => (Some(proxy_url), vec!["socks5h"]),
    };
    if let Some(proxy) = proxy {
        let parsed =
            url::Url::parse(proxy).map_err(|_| Error::Invalid("Invalid proxy URL.".into()))?;
        if !schemes.contains(&parsed.scheme()) {
            return Err(Error::Invalid(
                "Proxy URL scheme does not match transport kind.".into(),
            ));
        }
        builder = builder.proxy(
            reqwest::Proxy::all(proxy).map_err(|_| Error::Invalid("Invalid proxy.".into()))?,
        );
    }
    let mut headers = HeaderMap::new();
    for (k, v) in &u.extra_headers {
        let name = HeaderName::from_bytes(k.as_bytes())
            .map_err(|_| Error::Invalid("Invalid header.".into()))?;
        if matches!(
            name.as_str(),
            "host" | "connection" | "content-length" | "transfer-encoding"
        ) {
            return Err(Error::Invalid("Reserved upstream header.".into()));
        }
        headers.insert(
            name,
            HeaderValue::from_str(v).map_err(|_| Error::Invalid("Invalid header.".into()))?,
        );
    }
    Ok(builder.default_headers(headers))
}
/// Append a relative path without letting URL resolution escape the configured root.
pub fn safe_url(u: &Upstream, path: &str) -> Result<url::Url> {
    if !path.starts_with('/') || path.starts_with("//") || path.contains('\\') || path.contains('#')
    {
        return Err(Error::Invalid("Invalid upstream path.".into()));
    }
    let path_only = path.split('?').next().unwrap_or("");
    let decoded = url_decode_path(path_only)?;
    if decoded.split('/').any(|x| x == ".." || x == ".") || decoded.contains('\\') {
        return Err(Error::Invalid("Path traversal is not allowed.".into()));
    }
    let base =
        url::Url::parse(&u.base_url).map_err(|_| Error::Invalid("Invalid upstream URL.".into()))?;
    let target = url::Url::parse(&format!("{}{}", u.base_url.trim_end_matches('/'), path))
        .map_err(|_| Error::Invalid("Invalid path.".into()))?;
    if target.origin() != base.origin() {
        return Err(Error::Invalid("Upstream origin mismatch.".into()));
    }
    Ok(target)
}
fn url_decode_path(path: &str) -> Result<String> {
    let mut bytes = Vec::new();
    let mut it = path.bytes();
    while let Some(b) = it.next() {
        if b == b'%' {
            let h = it
                .next()
                .and_then(|c| (c as char).to_digit(16))
                .ok_or_else(|| Error::Invalid("Invalid path escape.".into()))?;
            let l = it
                .next()
                .and_then(|c| (c as char).to_digit(16))
                .ok_or_else(|| Error::Invalid("Invalid path escape.".into()))?;
            bytes.push((h * 16 + l) as u8)
        } else {
            bytes.push(b)
        }
    }
    String::from_utf8(bytes).map_err(|_| Error::Invalid("Invalid path encoding.".into()))
}
pub fn clean_headers(headers: &HeaderMap) -> HeaderMap {
    let connection: Vec<String> = headers
        .get("connection")
        .and_then(|v| v.to_str().ok())
        .unwrap_or("")
        .split(',')
        .map(|s| s.trim().to_lowercase())
        .collect();
    headers
        .iter()
        .filter(|(k, _)| {
            !matches!(
                k.as_str(),
                "host"
                    | "connection"
                    | "keep-alive"
                    | "proxy-authenticate"
                    | "proxy-authorization"
                    | "te"
                    | "trailer"
                    | "transfer-encoding"
                    | "upgrade"
                    | "content-length"
            ) && !connection.iter().any(|v| v == k.as_str())
        })
        .map(|(k, v)| (k.clone(), v.clone()))
        .collect()
}
impl Clients {
    async fn client(&self, u: &Upstream) -> Result<reqwest::Client> {
        let fingerprint = hash(serde_json::to_vec(u).map_err(|_| Error::Internal)?);
        if let Some((f, c)) = self.pool.read().await.get(&u.id) {
            if f == &fingerprint {
                return Ok(c.clone());
            }
        }
        let c = client_builder(u)?
            .build()
            .map_err(|_| Error::Invalid("Cannot build upstream transport.".into()))?;
        self.pool
            .write()
            .await
            .insert(u.id.clone(), (fingerprint, c.clone()));
        Ok(c)
    }
    // Keep method, headers, payload, retry safety and limit explicit at every
    // authority boundary; callers must deliberately choose each policy.
    #[allow(clippy::too_many_arguments)]
    pub async fn fetch(
        &self,
        u: &Upstream,
        method: Method,
        path: &str,
        headers: HeaderMap,
        body: Vec<u8>,
        safe: bool,
        limit: usize,
    ) -> std::result::Result<Response, FetchError> {
        let url = safe_url(u, path).map_err(|_| FetchError::Invalid)?;
        let client = self.client(u).await.map_err(|_| FetchError::Invalid)?;
        let count = if safe { u.retry_count.min(3) } else { 0 };
        for attempt in 0..=count {
            let result = client
                .request(method.clone(), url.clone())
                .headers(clean_headers(&headers))
                .body(body.clone())
                .send()
                .await;
            match result {
                Ok(mut r) => {
                    if r.status().is_server_error() && attempt < count {
                        continue;
                    }
                    if r.content_length().is_some_and(|n| n > limit as u64) {
                        return Err(FetchError::TooLarge);
                    }
                    let status = r.status().as_u16();
                    let headers = clean_headers(r.headers());
                    let mut out = Vec::new();
                    while let Some(chunk) = r.chunk().await.map_err(|_| FetchError::Transport)? {
                        if chunk.len() > limit.saturating_sub(out.len()) {
                            return Err(FetchError::TooLarge);
                        }
                        out.extend_from_slice(&chunk);
                    }
                    return Ok(Response {
                        status,
                        headers,
                        body: out,
                    });
                }
                Err(_) if attempt < count => continue,
                Err(_) => return Err(FetchError::Transport),
            }
        }
        Err(FetchError::Transport)
    }
    pub async fn probe(&self, u: &Upstream) -> Health {
        let start = Instant::now();
        let success = self
            .fetch(
                u,
                Method::GET,
                &u.health.path,
                HeaderMap::new(),
                vec![],
                true,
                1024 * 1024,
            )
            .await
            .is_ok_and(|r| (200..400).contains(&r.status));
        let mut all = self.health.write().await;
        let h = all.entry(u.id.clone()).or_default();
        h.checked_at = now();
        h.elapsed_ms = start.elapsed().as_millis() as u64;
        if success {
            h.failures = 0;
            h.recoveries += 1;
            if h.healthy != Some(false) || h.recoveries >= u.health.recovery_threshold {
                h.healthy = Some(true)
            }
        } else {
            h.recoveries = 0;
            h.failures += 1;
            if h.failures >= u.health.failure_threshold {
                h.healthy = Some(false)
            }
        }
        h.clone()
    }
    pub async fn health_tick(&self, repo: &dyn Repository) -> Result<()> {
        let (_, s) = repo.load().await?;
        for u in s
            .upstreams
            .values()
            .filter(|u| u.enabled && u.health.enabled)
        {
            let due = self
                .health
                .read()
                .await
                .get(&u.id)
                .is_none_or(|h| h.checked_at + u.health.interval_seconds <= now());
            if due {
                self.probe(u).await;
            }
        }
        Ok(())
    }
}
