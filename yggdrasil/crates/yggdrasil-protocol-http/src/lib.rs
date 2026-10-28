//! Thin Minecraft/authlib-injector HTTP adapter; management uses a different envelope.
use axum::{
    body::{to_bytes, Body},
    extract::{ConnectInfo, FromRequest, Request, State},
    http::{Method, StatusCode},
    response::{IntoResponse, Response},
    routing::{any, get},
    Router,
};
use serde_json::json;
use std::{net::SocketAddr, sync::Arc};
use yggdrasil_cache::Cache;
use yggdrasil_core::{hash, Error, Operation};
use yggdrasil_federation::{Federation, Request as FederationRequest};
use yggdrasil_service::MAX_TEXTURE;

#[derive(Clone)]
pub struct Protocol {
    pub federation: Arc<Federation>,
    pub cache: Cache,
}
pub fn router(p: Protocol) -> Router {
    Router::new()
        .route("/", get(metadata))
        .route("/textures/{hash}", get(texture))
        .route("/skins/MinecraftSkins/{name}", get(legacy))
        .route("/authserver/{action}", any(dispatch))
        .route("/sessionserver/session/minecraft/{action}", any(dispatch))
        .route(
            "/sessionserver/session/minecraft/profile/{id}",
            any(dispatch),
        )
        .route("/api/profiles/minecraft", any(dispatch))
        .route("/api/user/profile/{id}/{kind}", any(upload))
        .route("/users/profiles/minecraft/{name}", any(dispatch))
        .route("/user/profiles/{id}/names", any(dispatch))
        .route("/minecraftservices/{*path}", any(dispatch))
        .route("/cache/{*path}", any(cache))
        .layer(axum::extract::DefaultBodyLimit::max(MAX_TEXTURE + 65536))
        .with_state(p)
}
pub struct ProtocolError(pub Error);
impl From<Error> for ProtocolError {
    fn from(e: Error) -> Self {
        Self(e)
    }
}
impl IntoResponse for ProtocolError {
    fn into_response(self) -> Response {
        let (status, name) = match self.0 {
            Error::Forbidden | Error::Unauthorized => {
                (StatusCode::FORBIDDEN, "ForbiddenOperationException")
            }
            Error::NotFound => (StatusCode::NOT_FOUND, "IllegalArgumentException"),
            Error::Invalid(_) => (StatusCode::BAD_REQUEST, "IllegalArgumentException"),
            Error::Conflict => (StatusCode::CONFLICT, "IllegalArgumentException"),
            Error::RateLimited => (StatusCode::TOO_MANY_REQUESTS, "ForbiddenOperationException"),
            Error::Upstream => (StatusCode::BAD_GATEWAY, "ServiceUnavailableException"),
            Error::Internal => (StatusCode::INTERNAL_SERVER_ERROR, "InternalError"),
        };
        (
            status,
            axum::Json(json!({"error":name,"errorMessage":self.0.to_string()})),
        )
            .into_response()
    }
}
pub fn into_response(r: yggdrasil_upstream::Response) -> Response {
    let mut response = Response::new(Body::from(r.body));
    *response.status_mut() = StatusCode::from_u16(r.status).unwrap_or(StatusCode::BAD_GATEWAY);
    *response.headers_mut() = r.headers;
    response
}
async fn metadata(
    State(p): State<Protocol>,
) -> Result<axum::Json<serde_json::Value>, ProtocolError> {
    Ok(axum::Json(p.federation.local.metadata()?))
}
async fn texture(
    State(p): State<Protocol>,
    axum::extract::Path(digest): axum::extract::Path<String>,
) -> Result<Response, ProtocolError> {
    let bytes = p.federation.local.blobs.get(&digest).await?;
    Ok((
        [
            ("content-type", "image/png"),
            ("cache-control", "public, max-age=31536000, immutable"),
        ],
        bytes,
    )
        .into_response())
}
async fn legacy(
    State(p): State<Protocol>,
    axum::extract::Path(name): axum::extract::Path<String>,
) -> Result<Response, ProtocolError> {
    let (_, s) = p.federation.local.repo.load().await?;
    let name = name.strip_suffix(".png").ok_or(Error::NotFound)?;
    let profile = s
        .profiles
        .values()
        .find(|v| v.name.eq_ignore_ascii_case(name))
        .ok_or(Error::NotFound)?;
    let h = profile.skin.as_ref().ok_or(Error::NotFound)?;
    let bytes = p.federation.local.blobs.get(h).await?;
    Ok(([("content-type", "image/png")], bytes).into_response())
}
fn operation(method: &Method, path: &str) -> Option<Operation> {
    use Operation::*;
    match (method.as_str(), path) {
        ("POST", "/authserver/authenticate") => Some(Authenticate),
        ("POST", "/authserver/refresh") => Some(Refresh),
        ("POST", "/authserver/validate") => Some(Validate),
        ("POST", "/authserver/invalidate") => Some(Invalidate),
        ("POST", "/authserver/signout") => Some(Signout),
        ("POST", "/sessionserver/session/minecraft/join") => Some(Join),
        ("GET", "/sessionserver/session/minecraft/hasJoined") => Some(HasJoined),
        ("POST", "/api/profiles/minecraft") => Some(ProfileBatch),
        ("POST", "/minecraftservices/player/certificates") => Some(PlayerCertificates),
        ("GET", "/minecraftservices/privileges") => Some(Privileges),
        ("GET", "/minecraftservices/player/attributes") => Some(PlayerAttributes),
        ("GET", "/minecraftservices/privacy/blocklist") => Some(Blocklist),
        ("GET", p) if p.starts_with("/sessionserver/session/minecraft/profile/") => {
            Some(ProfileLookup)
        }
        ("GET", p) if p.starts_with("/users/profiles/minecraft/") => Some(ProfileByName),
        ("GET", p) if p.starts_with("/user/profiles/") && p.ends_with("/names") => {
            Some(NameHistory)
        }
        _ => None,
    }
}
async fn dispatch(State(p): State<Protocol>, r: Request) -> Result<Response, ProtocolError> {
    let op = operation(r.method(), r.uri().path()).ok_or(Error::NotFound)?;
    forward(p, r, op).await
}
async fn forward(p: Protocol, r: Request, op: Operation) -> Result<Response, ProtocolError> {
    let ip = r
        .extensions()
        .get::<ConnectInfo<SocketAddr>>()
        .map(|v| v.0.ip().to_string())
        .unwrap_or_default();
    let path = r
        .uri()
        .path_and_query()
        .map(|p| p.as_str())
        .unwrap_or("/")
        .to_owned();
    let method = r.method().clone();
    let headers = r.headers().clone();
    let body = to_bytes(r.into_body(), MAX_TEXTURE + 65536)
        .await
        .map_err(|_| Error::Invalid("Request body too large.".into()))?
        .to_vec();
    Ok(into_response(
        p.federation
            .execute(FederationRequest {
                operation: op,
                method,
                path,
                headers,
                body,
                ip,
            })
            .await?,
    ))
}
async fn cache(State(p): State<Protocol>, r: Request) -> Result<Response, ProtocolError> {
    Ok(into_response(
        p.cache
            .handle(
                r.method().clone(),
                r.uri().path_and_query().map(|v| v.as_str()).unwrap_or("/"),
                r.headers().clone(),
            )
            .await?,
    ))
}
async fn upload(
    State(p): State<Protocol>,
    axum::extract::Path((id, kind)): axum::extract::Path<(String, String)>,
    r: Request,
) -> Result<Response, ProtocolError> {
    let op = match r.method().as_str() {
        "PUT" => Operation::TextureUpload,
        "DELETE" => Operation::TextureDelete,
        _ => return Err(Error::NotFound.into()),
    };
    let token = r
        .headers()
        .get("authorization")
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.strip_prefix("Bearer "))
        .ok_or(Error::Forbidden)?
        .to_owned();
    let (_, s) = p.federation.local.repo.load().await?;
    if !s.tokens.contains_key(&hash(&token)) {
        return forward(p, r, op).await;
    }
    let id = yggdrasil_core::canonical_uuid(&id)?;
    p.federation.local.authorize_profile(&token, &id).await?;
    if op == Operation::TextureDelete {
        p.federation.local.delete_texture(&id, &kind).await?;
        return Ok(StatusCode::NO_CONTENT.into_response());
    }
    let mut multipart = axum::extract::Multipart::from_request(r, &())
        .await
        .map_err(|_| Error::Invalid("Expected multipart/form-data.".into()))?;
    let mut image = None;
    let mut slim = false;
    while let Some(field) = multipart
        .next_field()
        .await
        .map_err(|_| Error::Invalid("Invalid multipart data.".into()))?
    {
        match field.name() {
            Some("file") => {
                if image.is_some() || field.content_type() != Some("image/png") {
                    return Err(Error::Invalid("Expected one image/png file.".into()).into());
                }
                image = Some(
                    field
                        .bytes()
                        .await
                        .map_err(|_| Error::Invalid("Invalid texture file.".into()))?
                        .to_vec(),
                );
            }
            Some("model") => {
                slim = field
                    .text()
                    .await
                    .map_err(|_| Error::Invalid("Invalid model.".into()))?
                    == "slim"
            }
            _ => {}
        }
    }
    p.federation
        .local
        .texture(
            &id,
            &kind,
            image.ok_or_else(|| Error::Invalid("Missing file.".into()))?,
            slim,
        )
        .await?;
    Ok(StatusCode::NO_CONTENT.into_response())
}
