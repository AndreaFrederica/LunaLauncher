use axum::{
    body::{to_bytes, Body},
    http::{Request, StatusCode},
    routing::get,
    Router,
};
use base64::{engine::general_purpose::STANDARD as B64, Engine};
use http::{HeaderMap, Method};
use serde_json::{json, Value};
use std::{
    io::Cursor,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc,
    },
};
use tower::ServiceExt;
use yggdrasil_core::{now, offline_uuid};
use yggdrasil_server::{build, Config};
use yggdrasil_service::{JoinRequest, TokenRequest};

async fn call(app: &Router, token: &str, method: &str, path: &str, body: Value) -> (u16, Value) {
    let response = app
        .clone()
        .oneshot(
            Request::builder()
                .method(method)
                .uri(format!("/api/v1/{path}"))
                .header("authorization", format!("Bearer {token}"))
                .header("content-type", "application/json")
                .body(Body::from(body.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    let status = response.status().as_u16();
    let bytes = to_bytes(response.into_body(), 4 * 1024 * 1024)
        .await
        .unwrap();
    (status, serde_json::from_slice(&bytes).unwrap())
}

fn snapshot() -> Value {
    let mut png = Cursor::new(Vec::new());
    image::RgbaImage::new(64, 64)
        .write_to(&mut png, image::ImageFormat::Png)
        .unwrap();
    let mut cape = Cursor::new(Vec::new());
    image::RgbaImage::new(64, 32)
        .write_to(&mut cape, image::ImageFormat::Png)
        .unwrap();
    json!({"profile":{"id":"123456789abc4def8123456789abcdef","name":"Alice",
        "skin":{"id":"skin","url":"https://textures.minecraft.net/texture/skin","variant":"SLIM","data":B64.encode(png.into_inner())},
        "cape":"cape","capes":[{"id":"cape","url":"https://textures.minecraft.net/texture/cape","alias":"Cape","data":B64.encode(cape.into_inner())}]},
        "entitlement":{"ownsMinecraft":true,"canPlayMinecraft":true},"verified_at":now()})
}

#[tokio::test]
async fn official_snapshot_becomes_local_session_with_original_uuid_and_signed_textures() {
    let dir = tempfile::tempdir().unwrap();
    let config = Config {
        data_dir: dir.path().into(),
        ..Config::default()
    };
    let app = build(&config).await.unwrap();
    let admin = app.management.launcher_session().await.unwrap();
    let snapshot = snapshot();
    let id = snapshot["profile"]["id"].as_str().unwrap().to_owned();
    let path = format!("account-snapshots/{id}");
    assert_eq!(
        call(&app.router, "invalid", "PUT", &path, snapshot.clone())
            .await
            .0,
        401
    );
    assert_eq!(
        call(&app.router, &admin, "PUT", &path, snapshot.clone())
            .await
            .0,
        200
    );
    // Snapshot storage does not itself create an authenticatable local identity.
    assert!(app.service.repo.load().await.unwrap().1.profiles.is_empty());
    let restarted = build(&config).await.unwrap();
    assert_eq!(
        call(&restarted.router, &admin, "GET", &path, Value::Null)
            .await
            .1["profile"]["name"],
        "Alice"
    );
    let (status, session) = call(
        &restarted.router,
        &admin,
        "POST",
        &format!("{path}/session"),
        json!({}),
    )
    .await;
    assert_eq!(status, 200);
    assert_eq!(session["selectedProfile"]["id"], id);
    assert_ne!(id, offline_uuid("Alice").simple().to_string());
    let token = session["accessToken"].as_str().unwrap();
    let request = TokenRequest {
        access_token: token.into(),
        client_token: Some(session["clientToken"].as_str().unwrap().into()),
        selected_profile: None,
        request_user: false,
    };
    restarted.service.validate(&request).await.unwrap();
    restarted
        .service
        .join(
            JoinRequest {
                access_token: token.into(),
                selected_profile: id.clone().into(),
                server_id: "local-game".into(),
            },
            "127.0.0.1".into(),
        )
        .await
        .unwrap();
    let joined = restarted
        .service
        .has_joined("local-game", "Alice", None)
        .await
        .unwrap()
        .unwrap();
    assert_eq!(joined["id"], id);
    let property = &joined["properties"][0];
    assert!(!property["signature"].as_str().unwrap().is_empty());
    let textures: Value =
        serde_json::from_slice(&B64.decode(property["value"].as_str().unwrap()).unwrap()).unwrap();
    assert_eq!(textures["profileId"], id);
    assert_eq!(textures["textures"]["SKIN"]["metadata"]["model"], "slim");
    for kind in ["SKIN", "CAPE"] {
        let url: http::Uri = textures["textures"][kind]["url"]
            .as_str()
            .unwrap()
            .parse()
            .unwrap();
        assert_eq!(url.host(), Some("127.0.0.1"));
        let response = restarted
            .router
            .clone()
            .oneshot(
                Request::builder()
                    .uri(url.path())
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        assert!(to_bytes(response.into_body(), 1024 * 1024)
            .await
            .unwrap()
            .starts_with(b"\x89PNG"));
    }
    let state = serde_json::to_string(&restarted.service.repo.load().await.unwrap().1).unwrap();
    assert!(!state.contains(token));
    assert_eq!(
        call(&restarted.router, &admin, "DELETE", &path, Value::Null)
            .await
            .0,
        200
    );
    assert!(restarted.service.validate(&request).await.is_err());
    assert_eq!(
        call(
            &restarted.router,
            &admin,
            "POST",
            &format!("{path}/session"),
            json!({})
        )
        .await
        .0,
        404
    );
}

#[tokio::test]
async fn snapshots_reject_tokens_mismatched_id_and_identity_collisions() {
    let dir = tempfile::tempdir().unwrap();
    let app = build(&Config {
        data_dir: dir.path().into(),
        ..Config::default()
    })
    .await
    .unwrap();
    let token = app.management.launcher_session().await.unwrap();
    let valid = snapshot();
    let id = valid["profile"]["id"].as_str().unwrap();
    let path = format!("account-snapshots/{id}");
    let mut invalid = valid.clone();
    invalid["access_token"] = json!("must-not-store");
    assert_eq!(
        call(&app.router, &token, "PUT", &path, invalid).await.0,
        400
    );
    let mut invalid = valid.clone();
    invalid["profile"]["id"] = json!("aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa");
    assert_eq!(
        call(&app.router, &token, "PUT", &path, invalid).await.0,
        400
    );
    let mut invalid = valid.clone();
    invalid["profile"]["skin"]["url"] = json!("http://127.0.0.1/private");
    assert_eq!(
        call(&app.router, &token, "PUT", &path, invalid).await.0,
        400
    );
    assert_eq!(
        call(&app.router, &token, "PUT", &path, valid.clone())
            .await
            .0,
        200
    );
    let mut older = valid.clone();
    older["verified_at"] = json!(now() - 3600);
    assert_eq!(call(&app.router, &token, "PUT", &path, older).await.0, 409);
    let user = app
        .service
        .create_user("local-account".into(), "a-long-test-password".into(), false)
        .await
        .unwrap();
    app.service
        .create_profile(
            user.id,
            "Alice".into(),
            yggdrasil_core::UuidScheme::Fixed {
                uuid: id.parse().unwrap(),
            },
        )
        .await
        .unwrap();
    assert_eq!(
        call(
            &app.router,
            &token,
            "POST",
            &format!("{path}/session"),
            json!({})
        )
        .await
        .0,
        409
    );
    let local = app
        .service
        .authenticate(yggdrasil_service::Credentials {
            username: "local-account".into(),
            password: "a-long-test-password".into(),
            client_token: None,
            request_user: false,
        })
        .await
        .unwrap();
    app.service
        .join(
            JoinRequest {
                access_token: local.access_token,
                selected_profile: id.into(),
                server_id: "independent".into(),
            },
            "127.0.0.1".into(),
        )
        .await
        .unwrap();
    let mut revoked = valid.clone();
    revoked["entitlement"] = json!({"ownsMinecraft":false,"canPlayMinecraft":false});
    assert_eq!(
        call(&app.router, &token, "PUT", &path, revoked).await.0,
        200
    );
    assert_eq!(
        call(
            &app.router,
            &token,
            "POST",
            &format!("{path}/session"),
            json!({})
        )
        .await
        .0,
        403
    );
    assert!(app
        .service
        .has_joined("independent", "Alice", None)
        .await
        .unwrap()
        .is_some());
    assert_eq!(
        call(&app.router, &token, "DELETE", &path, Value::Null)
            .await
            .0,
        200
    );
    assert!(app
        .service
        .has_joined("independent", "Alice", None)
        .await
        .unwrap()
        .is_some());
}

#[tokio::test]
async fn third_party_snapshot_can_issue_local_session_without_entitlement() {
    let dir = tempfile::tempdir().unwrap();
    let app = build(&Config {
        data_dir: dir.path().into(),
        ..Config::default()
    })
    .await
    .unwrap();
    let admin = app.management.launcher_session().await.unwrap();
    let mut snapshot = snapshot();
    snapshot["profile"]["id"] = json!("aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa");
    snapshot["profile"]["name"] = json!("ThirdParty");
    snapshot["entitlement"] = json!({"ownsMinecraft":false,"canPlayMinecraft":false});
    snapshot["local_auth"] = json!(true);
    let id = snapshot["profile"]["id"].as_str().unwrap().to_owned();
    let path = format!("account-snapshots/{id}");
    assert_eq!(call(&app.router, &admin, "PUT", &path, snapshot).await.0, 200);
    let (status, session) = call(
        &app.router,
        &admin,
        "POST",
        &format!("{path}/session"),
        json!({}),
    )
    .await;
    assert_eq!(status, 200);
    assert_eq!(session["selectedProfile"]["id"], id);
    assert_eq!(session["selectedProfile"]["name"], "ThirdParty");
}

#[tokio::test]
async fn launcher_metadata_cache_survives_configuration_restart_and_origin_failure() {
    let failures = Arc::new(AtomicUsize::new(0));
    let status = failures.clone();
    let origin = Router::new().route(
        "/v1/index.json",
        get(move |headers: HeaderMap| {
            let status = status.clone();
            async move {
                let code = match status.load(Ordering::SeqCst) {
                    1 => StatusCode::SERVICE_UNAVAILABLE,
                    2 => StatusCode::UNAUTHORIZED,
                    _ if headers.contains_key("if-none-match") => StatusCode::NOT_MODIFIED,
                    _ => StatusCode::OK,
                };
                (
                    code,
                    [("etag", "metadata-1"), ("cache-control", "max-age=0")],
                    "{\"versions\":[]}",
                )
            }
        }),
    );
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let root = format!("http://{}/v1/", listener.local_addr().unwrap());
    let upstream = tokio::spawn(async move { axum::serve(listener, origin).await.unwrap() });
    let dir = tempfile::tempdir().unwrap();
    let config = Config {
        data_dir: dir.path().into(),
        ..Config::default()
    };
    let app = build(&config).await.unwrap();
    let admin = app.management.launcher_session().await.unwrap();
    let settings = json!({"metadata_url":root,"transport":{"kind":"direct"}});
    let (status, routes) = call(
        &app.router,
        &admin,
        "PUT",
        "launcher/cache",
        settings.clone(),
    )
    .await;
    assert_eq!(status, 200);
    let path = format!("{}index.json", routes["metadata_path"].as_str().unwrap());
    let response = app
        .management
        .cache
        .handle(Method::GET, &path, HeaderMap::new())
        .await
        .unwrap();
    assert_eq!(response.headers["x-yggdrasil-cache"], "MISS");
    assert_eq!(response.body, b"{\"versions\":[]}");
    let restarted = build(&config).await.unwrap();
    assert_eq!(
        call(
            &restarted.router,
            &admin,
            "PUT",
            "launcher/cache",
            settings.clone()
        )
        .await
        .0,
        200
    );
    assert_eq!(
        restarted
            .management
            .cache
            .handle(Method::GET, &path, HeaderMap::new())
            .await
            .unwrap()
            .headers["x-yggdrasil-cache"],
        "REVALIDATED"
    );
    failures.store(1, Ordering::SeqCst);
    assert_eq!(
        restarted
            .management
            .cache
            .handle(Method::GET, &path, HeaderMap::new())
            .await
            .unwrap()
            .headers["x-yggdrasil-cache"],
        "STALE"
    );
    failures.store(2, Ordering::SeqCst);
    assert_eq!(
        restarted
            .management
            .cache
            .handle(Method::GET, &path, HeaderMap::new())
            .await
            .unwrap()
            .status,
        401
    );
    failures.store(0, Ordering::SeqCst);
    assert_eq!(
        restarted
            .management
            .cache
            .handle(Method::GET, &path, HeaderMap::new())
            .await
            .unwrap()
            .status,
        200
    );
    upstream.abort();
    assert_eq!(
        restarted
            .management
            .cache
            .handle(Method::GET, &path, HeaderMap::new())
            .await
            .unwrap()
            .headers["x-yggdrasil-cache"],
        "STALE"
    );
    let changed =
        json!({"metadata_url":root.replace("/v1/", "/v2/"),"transport":{"kind":"direct"}});
    let (_, routes2) = call(&restarted.router, &admin, "PUT", "launcher/cache", changed).await;
    assert_ne!(routes2["metadata_path"], routes["metadata_path"]);
    assert!(restarted
        .management
        .cache
        .handle(
            Method::GET,
            &format!("{}index.json", routes2["metadata_path"].as_str().unwrap()),
            HeaderMap::new()
        )
        .await
        .is_err());
}
