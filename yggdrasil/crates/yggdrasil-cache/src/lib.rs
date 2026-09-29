//! Explicit reverse routes with a persistent content-addressed cache.
use http::{HeaderMap, HeaderValue, Method};
use serde_json::{json, Value};
use std::{
    collections::BTreeMap,
    sync::{
        atomic::{AtomicU64, Ordering},
        Arc,
    },
};
use yggdrasil_core::*;
use yggdrasil_upstream::{safe_url, Clients, Response};

#[derive(Default)]
pub struct Counters {
    hit: AtomicU64,
    miss: AtomicU64,
    stale: AtomicU64,
    revalidated: AtomicU64,
    fetch: AtomicU64,
    bypass: AtomicU64,
    error: AtomicU64,
    bytes_served: AtomicU64,
    bytes_fetched: AtomicU64,
}
#[derive(Clone)]
pub struct Cache {
    pub repo: Arc<dyn Repository>,
    pub blobs: Arc<dyn BlobStore>,
    pub clients: Clients,
    pub counters: Arc<Counters>,
}
fn inc(c: &AtomicU64) {
    c.fetch_add(1, Ordering::Relaxed);
}
pub fn validate_route(r: &CacheRoute, s: &State) -> Result<()> {
    let u = s
        .upstreams
        .get(&r.upstream_id)
        .ok_or_else(|| Error::Invalid("Unknown cache upstream.".into()))?;
    if !u.capabilities.iter().any(|c| c == "http")
        || !r.listen_prefix.starts_with("/cache/")
        || r.listen_prefix.ends_with('/')
        || r.listen_prefix.contains('?')
        || r.methods.is_empty()
        || !r.methods.iter().all(|m| m == "GET" || m == "HEAD")
        || r.max_object_bytes == 0
        || r.max_object_bytes > 256 * 1024 * 1024
        || r.cache_statuses.contains(&206)
    {
        return Err(Error::Invalid("Invalid cache route or policy.".into()));
    }
    if s.cache_routes
        .values()
        .any(|v| v.id != r.id && v.listen_prefix == r.listen_prefix)
    {
        return Err(Error::Conflict);
    }
    for h in &r.vary_headers {
        http::HeaderName::from_bytes(h.as_bytes())
            .map_err(|_| Error::Invalid("Invalid vary header.".into()))?;
    }
    safe_url(u, &r.upstream_path_prefix)?;
    Ok(())
}
impl Cache {
    pub fn new(repo: Arc<dyn Repository>, blobs: Arc<dyn BlobStore>, clients: Clients) -> Self {
        Self {
            repo,
            blobs,
            clients,
            counters: Arc::default(),
        }
    }
    pub async fn stats(&self) -> Result<Value> {
        let (_, s) = self.repo.load().await?;
        let c = &self.counters;
        Ok(
            json!({"hit":c.hit.load(Ordering::Relaxed),"miss":c.miss.load(Ordering::Relaxed),"stale":c.stale.load(Ordering::Relaxed),"revalidated":c.revalidated.load(Ordering::Relaxed),"upstream_fetch":c.fetch.load(Ordering::Relaxed),"bypass":c.bypass.load(Ordering::Relaxed),"error":c.error.load(Ordering::Relaxed),"bytes_served":c.bytes_served.load(Ordering::Relaxed),"bytes_fetched":c.bytes_fetched.load(Ordering::Relaxed),"entries":s.cache_entries.len(),"bytes":s.cache_entries.values().map(|e|e.size as u64).sum::<u64>()}),
        )
    }
    pub async fn purge(&self, route: Option<&str>, key: Option<&str>) -> Result<usize> {
        transact(self.repo.as_ref(), |s| {
            let before = s.cache_entries.len();
            s.cache_entries.retain(|k, e| {
                !(route.is_none_or(|r| r == e.route_id) && key.is_none_or(|v| v == k))
            });
            Ok(before - s.cache_entries.len())
        })
        .await
    }
    async fn cached(&self, e: &CacheEntry, head: bool, status: &'static str) -> Result<Response> {
        let body = if head {
            vec![]
        } else {
            self.blobs.get(&e.blob).await?
        };
        self.counters
            .bytes_served
            .fetch_add(body.len() as u64, Ordering::Relaxed);
        let mut headers = HeaderMap::new();
        for (k, v) in &e.headers {
            if let (Ok(k), Ok(v)) = (
                http::HeaderName::from_bytes(k.as_bytes()),
                HeaderValue::from_str(v),
            ) {
                headers.insert(k, v);
            }
        }
        headers.insert("x-yggdrasil-cache", HeaderValue::from_static(status));
        Ok(Response {
            status: e.status,
            headers,
            body,
        })
    }
    pub async fn handle(&self, method: Method, path: &str, headers: HeaderMap) -> Result<Response> {
        let (_, s) = self.repo.load().await?;
        let path_only = path.split('?').next().unwrap_or("");
        let r = s
            .cache_routes
            .values()
            .filter(|r| {
                r.enabled
                    && (path_only == r.listen_prefix
                        || path_only.starts_with(&format!("{}/", r.listen_prefix)))
            })
            .max_by_key(|r| r.listen_prefix.len())
            .ok_or(Error::NotFound)?;
        if !r.methods.iter().any(|m| m == method.as_str()) {
            return Err(Error::Invalid("Method not allowed for this route.".into()));
        }
        let u = s
            .upstreams
            .get(&r.upstream_id)
            .filter(|u| u.enabled)
            .ok_or(Error::Upstream)?;
        let tail = &path_only[r.listen_prefix.len()..];
        let mapped = format!("{}{}", r.upstream_path_prefix.trim_end_matches('/'), tail);
        let mapped = if mapped.is_empty() {
            "/".into()
        } else {
            mapped
        };
        let raw_query = path.split_once('?').map(|p| p.1).unwrap_or("");
        let mut query: Vec<(String, String)> = url::form_urlencoded::parse(raw_query.as_bytes())
            .into_owned()
            .filter(|(k, _)| !r.ignore_query.contains(k))
            .collect();
        query.sort();
        let normalized_query = url::form_urlencoded::Serializer::new(String::new())
            .extend_pairs(&query)
            .finish();
        let destination = if raw_query.is_empty() {
            mapped.clone()
        } else {
            format!("{mapped}?{raw_query}")
        };
        safe_url(u, &destination)?;
        let authenticated = headers.contains_key("authorization") || headers.contains_key("cookie");
        let req_cc = headers
            .get("cache-control")
            .and_then(|v| v.to_str().ok())
            .unwrap_or("")
            .to_lowercase();
        let bypass = authenticated && !r.cache_authenticated
            || headers.contains_key("range")
            || headers.contains_key("if-range")
            || headers.contains_key("if-none-match")
            || headers.contains_key("if-modified-since")
            || req_cc.contains("no-store");
        let mut vary = BTreeMap::new();
        for h in &r.vary_headers {
            vary.insert(
                h.clone(),
                headers
                    .get(h)
                    .and_then(|v| v.to_str().ok())
                    .unwrap_or("")
                    .to_owned(),
            );
        }
        // Explicit authenticated caching still isolates credentials in the key (only a hash is stored).
        if authenticated {
            for h in ["authorization", "cookie"] {
                vary.insert(
                    h.into(),
                    hash(headers.get(h).map(|v| v.as_bytes()).unwrap_or_default()),
                );
            }
        }
        let key = hash(
            serde_json::to_vec(&(r, u, &mapped, &normalized_query, vary))
                .map_err(|_| Error::Internal)?,
        );
        let entry = if bypass {
            None
        } else {
            s.cache_entries.get(&key)
        };
        let t = now();
        let head = method == Method::HEAD;
        if let Some(e) = entry {
            if e.fresh_until > t && !req_cc.contains("no-cache") {
                if let Ok(v) = self.cached(e, head, "HIT").await {
                    inc(&self.counters.hit);
                    return Ok(v);
                }
            }
        }
        if bypass {
            inc(&self.counters.bypass)
        } else {
            inc(&self.counters.miss)
        }
        inc(&self.counters.fetch);
        let mut outgoing = headers.clone();
        if !bypass && r.revalidate {
            if let Some(e) = entry {
                for (a, b) in [
                    ("etag", "if-none-match"),
                    ("last-modified", "if-modified-since"),
                ] {
                    if let Some(v) = e.headers.get(a) {
                        outgoing.insert(b, HeaderValue::from_str(v).map_err(|_| Error::Internal)?);
                    }
                }
            }
        }
        // Fetch GET for HEAD misses so a header-only response never poisons the GET cache.
        let fetched = self
            .clients
            .fetch(
                u,
                if bypass { method } else { Method::GET },
                &destination,
                outgoing,
                vec![],
                true,
                256 * 1024 * 1024,
            )
            .await;
        let mut response = match fetched {
            Ok(v) if v.status < 500 => v,
            failed => {
                inc(&self.counters.error);
                if let Some(e) = entry {
                    if e.stale_until > t {
                        inc(&self.counters.stale);
                        return self.cached(e, head, "STALE").await;
                    }
                }
                return failed.map_err(|_| Error::Upstream);
            }
        };
        self.counters
            .bytes_fetched
            .fetch_add(response.body.len() as u64, Ordering::Relaxed);
        if response.status == 304 {
            if let Some(e) = entry {
                let mut renewed = e.clone();
                for (k, v) in response.headers.iter() {
                    if let Ok(v) = v.to_str() {
                        renewed.headers.insert(k.to_string(), v.to_owned());
                    }
                }
                let h = map_headers(&renewed.headers);
                let ttl = ttl(r, &h, e.status);
                renewed.fresh_until = t + ttl;
                renewed.stale_until = renewed.fresh_until + r.stale_if_error_seconds;
                let reusable = response_cacheable(r, &h);
                transact(self.repo.as_ref(), |s| {
                    if reusable {
                        s.cache_entries.insert(key.clone(), renewed.clone());
                    } else {
                        s.cache_entries.remove(&key);
                    }
                    Ok(())
                })
                .await?;
                inc(&self.counters.revalidated);
                return self.cached(&renewed, head, "REVALIDATED").await;
            }
        }
        let eligible = !bypass
            && r.cache_statuses.contains(&response.status)
            && response.status != 206
            && response_cacheable(r, &response.headers)
            && response.body.len() <= r.max_object_bytes;
        if eligible {
            let blob = self.blobs.put(&response.body).await?;
            let ttl = ttl(r, &response.headers, response.status);
            let e = CacheEntry {
                route_id: r.id.clone(),
                blob,
                status: response.status,
                headers: response
                    .headers
                    .iter()
                    .filter_map(|(k, v)| v.to_str().ok().map(|v| (k.to_string(), v.to_owned())))
                    .collect(),
                fresh_until: t + ttl,
                stale_until: t + ttl + r.stale_if_error_seconds,
                size: response.body.len(),
                accessed: t,
            };
            transact(self.repo.as_ref(), |s| {
                s.cache_entries.insert(key.clone(), e.clone());
                let mut total: usize = s.cache_entries.values().map(|e| e.size).sum();
                while s.cache_entries.len() > 10000 || total > 2 * 1024 * 1024 * 1024 {
                    if let Some(k) = s
                        .cache_entries
                        .iter()
                        .min_by_key(|(_, e)| e.accessed)
                        .map(|(k, _)| k.clone())
                    {
                        if let Some(removed) = s.cache_entries.remove(&k) {
                            total -= removed.size;
                        }
                    }
                }
                Ok(())
            })
            .await?;
        } else if entry.is_some() {
            transact(self.repo.as_ref(), |s| {
                s.cache_entries.remove(&key);
                Ok(())
            })
            .await?;
        }
        if head {
            response.body.clear();
        }
        response.headers.insert(
            "x-yggdrasil-cache",
            HeaderValue::from_static(if bypass { "BYPASS" } else { "MISS" }),
        );
        Ok(response)
    }
}
fn response_cacheable(r: &CacheRoute, headers: &HeaderMap) -> bool {
    !headers.contains_key("set-cookie")
        && headers.get_all("cache-control").iter().all(|v| {
            v.to_str().is_ok_and(|s| {
                let s = s.to_lowercase();
                !s.contains("no-store") && !s.contains("private")
            })
        })
        && headers.get_all("vary").iter().all(|v| {
            v.to_str().is_ok_and(|s| {
                s.split(',').all(|name| {
                    r.vary_headers
                        .iter()
                        .any(|h| h.eq_ignore_ascii_case(name.trim()))
                })
            })
        })
}
fn map_headers(map: &BTreeMap<String, String>) -> HeaderMap {
    map.iter()
        .filter_map(|(k, v)| {
            Some((
                http::HeaderName::from_bytes(k.as_bytes()).ok()?,
                HeaderValue::from_str(v).ok()?,
            ))
        })
        .collect()
}
fn ttl(r: &CacheRoute, h: &HeaderMap, status: u16) -> u64 {
    let ceiling = if status >= 400 {
        r.negative_ttl_seconds
    } else {
        r.ttl_seconds
    };
    if !r.respect_cache_control {
        return ceiling;
    }
    let cc = h
        .get("cache-control")
        .and_then(|v| v.to_str().ok())
        .unwrap_or("")
        .to_lowercase();
    let directives: Vec<&str> = cc.split(',').map(str::trim).collect();
    if directives.contains(&"no-cache") {
        return 0;
    }
    for prefix in ["s-maxage=", "max-age="] {
        if let Some(n) = directives.iter().find_map(|s| {
            s.strip_prefix(prefix)
                .and_then(|v| v.trim_matches('"').parse::<u64>().ok())
        }) {
            let age = h
                .get("age")
                .and_then(|v| v.to_str().ok())
                .and_then(|s| s.parse::<u64>().ok())
                .unwrap_or(0);
            return ceiling.min(n.saturating_sub(age));
        }
    }
    ceiling
}
