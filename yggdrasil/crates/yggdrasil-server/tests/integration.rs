use axum::{
    body::{to_bytes, Body},
    http::{Request, StatusCode},
    routing::{get, post},
    Router,
};
use base64::{engine::general_purpose::STANDARD as B64, Engine};
use http::{HeaderMap, Method};
use rsa::{pkcs8::DecodePublicKey, signature::Verifier};
use serde_json::{json, Value};
use std::{
    io::Cursor,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc,
    },
};
use tempfile::TempDir;
use tower::ServiceExt;
use yggdrasil_core::*;
use yggdrasil_federation::{Federation, Request as FederationRequest};
use yggdrasil_server::{build, Application, Config};
use yggdrasil_service::{Credentials, JoinRequest, ProfileRef, TokenRequest};

struct Fixture {
    dir: TempDir,
    app: Application,
    config: Config,
}
impl Fixture {
    async fn new() -> Self {
        let dir = tempfile::tempdir().unwrap();
        let config = Config {
            data_dir: dir.path().to_owned(),
            ..Config::default()
        };
        let app = build(&config).await.unwrap();
        Self { dir, app, config }
    }
    async fn user(&self, name: &str) -> (User, Profile) {
        let u = self
            .app
            .service
            .create_user(name.into(), "correct horse battery".into(), false)
            .await
            .unwrap();
        let p = self
            .app
            .service
            .create_profile(u.id.clone(), name.into(), UuidScheme::OfflinePlayer)
            .await
            .unwrap();
        (u, p)
    }
    async fn call(
        &self,
        method: &str,
        path: &str,
        body: Value,
        bearer: Option<&str>,
    ) -> (StatusCode, Value) {
        let mut r = Request::builder()
            .method(method)
            .uri(path)
            .header("content-type", "application/json");
        if let Some(raw) = bearer {
            r = r.header("authorization", format!("Bearer {raw}"))
        }
        let response = self
            .app
            .router
            .clone()
            .oneshot(
                r.body(Body::from(serde_json::to_vec(&body).unwrap()))
                    .unwrap(),
            )
            .await
            .unwrap();
        let status = response.status();
        let bytes = to_bytes(response.into_body(), 4 * 1024 * 1024)
            .await
            .unwrap();
        (
            status,
            serde_json::from_slice(&bytes).unwrap_or(Value::Null),
        )
    }
}
fn credentials(name: &str) -> Credentials {
    Credentials {
        username: name.into(),
        password: "correct horse battery".into(),
        client_token: Some("client".into()),
        request_user: true,
    }
}
fn request(raw: &str) -> TokenRequest {
    TokenRequest {
        access_token: raw.into(),
        client_token: Some("client".into()),
        selected_profile: None,
        request_user: false,
    }
}

#[tokio::test]
async fn lifecycle_failed_refresh_rotation_rename_and_restart() {
    let f = Fixture::new().await;
    let (_, p) = f.user("Notch").await;
    assert_eq!(p.id, "b50ad385829d3141a2167e7d7539ba7f");
    let a = f
        .app
        .service
        .authenticate(credentials("Notch"))
        .await
        .unwrap();
    let mut invalid = request(&a.access_token);
    invalid.client_token = Some("wrong".into());
    assert!(f.app.service.refresh(invalid).await.is_err());
    f.app
        .service
        .validate(&request(&a.access_token))
        .await
        .unwrap();
    let new = f
        .app
        .service
        .refresh(request(&a.access_token))
        .await
        .unwrap();
    assert!(f
        .app
        .service
        .validate(&request(&a.access_token))
        .await
        .is_err());
    f.app.service.rename(&p.id, "NewName".into()).await.unwrap();
    assert!(f
        .app
        .service
        .validate(&request(&new.access_token))
        .await
        .is_err());
    let fresh = f
        .app
        .service
        .refresh(request(&new.access_token))
        .await
        .unwrap();
    assert_eq!(fresh.selected_profile.as_ref().unwrap().id, p.id);
    assert_eq!(fresh.selected_profile.unwrap().name, "NewName");
    let key = f.app.service.public_key().unwrap();
    let restarted = build(&f.config).await.unwrap();
    assert_eq!(key, restarted.service.public_key().unwrap());
    restarted
        .service
        .validate(&request(&fresh.access_token))
        .await
        .unwrap();
    let (_, state) = restarted.service.repo.load().await.unwrap();
    let persisted = serde_json::to_string(&state).unwrap();
    assert!(!persisted.contains(&fresh.access_token));
    assert!(!persisted.contains("correct horse battery"));
    assert_eq!(state.profiles[&p.id].history.len(), 2);
    restarted
        .service
        .invalidate(&fresh.access_token)
        .await
        .unwrap();
    restarted
        .service
        .invalidate(&fresh.access_token)
        .await
        .unwrap();
    assert!(restarted
        .service
        .validate(&request(&fresh.access_token))
        .await
        .is_err());
}

#[tokio::test]
async fn multi_profile_binding_is_owned_and_atomic() {
    let f = Fixture::new().await;
    let (u, p) = f.user("Alice").await;
    let p2 = f
        .app
        .service
        .create_profile(u.id.clone(), "Alex".into(), UuidScheme::RandomV4)
        .await
        .unwrap();
    let (_, foreign) = f.user("Bob").await;
    let a = f
        .app
        .service
        .authenticate(credentials("Alice"))
        .await
        .unwrap();
    assert!(a.selected_profile.is_none());
    let mut r = request(&a.access_token);
    r.selected_profile = Some(ProfileRef::from(&foreign));
    assert!(f.app.service.refresh(r).await.is_err());
    let mut r = request(&a.access_token);
    r.selected_profile = Some(ProfileRef::from(&p2));
    let bound = f.app.service.refresh(r).await.unwrap();
    let mut r = request(&bound.access_token);
    r.selected_profile = Some(ProfileRef::from(&p));
    assert!(f.app.service.refresh(r).await.is_err());
    f.app
        .service
        .validate(&request(&bound.access_token))
        .await
        .unwrap();
    f.app.service.signout(credentials("Alice")).await.unwrap();
    assert!(f
        .app
        .service
        .validate(&request(&bound.access_token))
        .await
        .is_err());
}

#[tokio::test]
async fn concurrent_refresh_has_exactly_one_winner() {
    let f = Fixture::new().await;
    f.user("Alice").await;
    let a = f
        .app
        .service
        .authenticate(credentials("Alice"))
        .await
        .unwrap();
    let (r1, r2) = tokio::join!(
        f.app.service.refresh(request(&a.access_token)),
        f.app.service.refresh(request(&a.access_token))
    );
    assert_ne!(r1.is_ok(), r2.is_ok());
}

#[tokio::test]
async fn joins_check_binding_ip_expiry_and_signature() {
    let f = Fixture::new().await;
    let (_, p) = f.user("Alice").await;
    let a = f
        .app
        .service
        .authenticate(credentials("Alice"))
        .await
        .unwrap();
    f.app
        .service
        .join(
            JoinRequest {
                access_token: a.access_token.clone(),
                selected_profile: p.id.clone(),
                server_id: "server".into(),
            },
            "127.0.0.1".into(),
        )
        .await
        .unwrap();
    assert!(f
        .app
        .service
        .has_joined("server", "Alice", Some("10.0.0.1"))
        .await
        .unwrap()
        .is_none());
    assert!(f
        .app
        .service
        .has_joined("server", "Bob", None)
        .await
        .unwrap()
        .is_none());
    let profile = f
        .app
        .service
        .has_joined("server", "Alice", None)
        .await
        .unwrap()
        .unwrap();
    let prop = &profile["properties"][0];
    let key = rsa::RsaPublicKey::from_public_key_pem(&f.app.service.public_key().unwrap()).unwrap();
    let verifier = rsa::pkcs1v15::VerifyingKey::<sha1::Sha1>::new(key);
    let signature = rsa::pkcs1v15::Signature::try_from(
        B64.decode(prop["signature"].as_str().unwrap())
            .unwrap()
            .as_slice(),
    )
    .unwrap();
    verifier
        .verify(prop["value"].as_str().unwrap().as_bytes(), &signature)
        .unwrap();
    transact(f.app.service.repo.as_ref(), |s| {
        for j in s.joins.values_mut() {
            j.expires = 0
        }
        Ok(())
    })
    .await
    .unwrap();
    assert!(f
        .app
        .service
        .has_joined("server", "Alice", None)
        .await
        .unwrap()
        .is_none());
}

#[tokio::test]
async fn protocol_statuses_and_metadata_are_compatible() {
    let f = Fixture::new().await;
    let (_, p) = f.user("Steve").await;
    let (s, meta) = f.call("GET", "/", Value::Null, None).await;
    assert_eq!(s, 200);
    assert_eq!(meta["meta"]["feature.non_email_login"], true);
    assert_eq!(meta["meta"]["feature.enable_profile_key"], false);
    let (s, a) = f
        .call(
            "POST",
            "/authserver/authenticate",
            json!({"username":"Steve","password":"correct horse battery","clientToken":"client"}),
            None,
        )
        .await;
    assert_eq!(s, 200);
    let token = a["accessToken"].as_str().unwrap();
    assert_eq!(
        f.call("GET", "/minecraftservices/privileges", Value::Null, None)
            .await
            .0,
        403
    );
    assert_eq!(
        f.call(
            "GET",
            "/minecraftservices/privileges",
            Value::Null,
            Some(token)
        )
        .await
        .0,
        200
    );
    transact(f.app.service.repo.as_ref(), |s| {
        s.tokens.get_mut(&hash(token)).unwrap().temporary_invalid = true;
        Ok(())
    })
    .await
    .unwrap();
    assert_eq!(
        f.call(
            "GET",
            "/minecraftservices/privileges",
            Value::Null,
            Some(token)
        )
        .await
        .0,
        403
    );
    transact(f.app.service.repo.as_ref(), |s| {
        s.tokens.get_mut(&hash(token)).unwrap().temporary_invalid = false;
        Ok(())
    })
    .await
    .unwrap();
    assert_eq!(
        f.call(
            "POST",
            "/authserver/validate",
            json!({"accessToken":token,"clientToken":"client"}),
            None
        )
        .await
        .0,
        204
    );
    assert_eq!(
        f.call(
            "GET",
            &format!("/sessionserver/session/minecraft/profile/{}", p.id),
            Value::Null,
            None
        )
        .await
        .1["properties"][0]
            .get("signature"),
        None
    );
    assert_eq!(
        f.call(
            "GET",
            "/sessionserver/session/minecraft/hasJoined?username=Steve&serverId=none",
            Value::Null,
            None
        )
        .await
        .0,
        204
    );
    let (s, e) = f
        .call(
            "POST",
            "/authserver/validate",
            json!({"accessToken":"bogus"}),
            None,
        )
        .await;
    assert_eq!(s, 403);
    assert_eq!(e["error"], "ForbiddenOperationException");
    assert_eq!(
        f.call(
            "POST",
            "/api/profiles/minecraft",
            json!(["STEVE", "absent"]),
            None
        )
        .await
        .1
        .as_array()
        .unwrap()
        .len(),
        1
    );
}

#[tokio::test]
async fn textures_are_sanitized_bounded_and_persistent() {
    let f = Fixture::new().await;
    let (_, p) = f.user("Alice").await;
    let mut out = Cursor::new(vec![]);
    image::RgbaImage::new(64, 64)
        .write_to(&mut out, image::ImageFormat::Png)
        .unwrap();
    let mut input = out.into_inner();
    input.extend_from_slice(b"private metadata trailing bytes");
    f.app
        .service
        .texture(&p.id, "skin", input, true)
        .await
        .unwrap();
    let (_, s) = f.app.service.repo.load().await.unwrap();
    let h = s.profiles[&p.id].skin.as_ref().unwrap();
    let bytes = f.app.service.blobs.get(h).await.unwrap();
    assert!(!bytes.windows(16).any(|s| s == b"private metadata"));
    assert_eq!(hash(&bytes), *h);
    let mut wrong = Cursor::new(vec![]);
    image::RgbaImage::new(512, 512)
        .write_to(&mut wrong, image::ImageFormat::Png)
        .unwrap();
    assert!(f
        .app
        .service
        .texture(&p.id, "skin", wrong.into_inner(), false)
        .await
        .is_err());
    assert!(f
        .app
        .service
        .blobs
        .get("../../signing-key.pem")
        .await
        .is_err());
    let mut cape = Cursor::new(vec![]);
    image::RgbaImage::new(22, 17)
        .write_to(&mut cape, image::ImageFormat::Png)
        .unwrap();
    f.app
        .service
        .texture(&p.id, "cape", cape.into_inner(), false)
        .await
        .unwrap();
    let (_, s) = f.app.service.repo.load().await.unwrap();
    let bytes = f
        .app
        .service
        .blobs
        .get(s.profiles[&p.id].cape.as_ref().unwrap())
        .await
        .unwrap();
    let image = image::load_from_memory(&bytes).unwrap();
    assert_eq!((image.width(), image.height()), (64, 32));
}

#[tokio::test]
async fn management_auth_csrf_redaction_last_admin_and_revocation() {
    let f = Fixture::new().await;
    let admin = f
        .app
        .service
        .create_user("admin".into(), "correct horse battery".into(), true)
        .await
        .unwrap();
    assert_eq!(
        f.call("GET", "/api/v1/users", Value::Null, None).await.0,
        401
    );
    let (s, login) = f
        .call(
            "POST",
            "/api/v1/auth/login",
            json!({"username":"admin","password":"correct horse battery"}),
            None,
        )
        .await;
    assert_eq!(s, 200);
    let token = login["token"].as_str().unwrap();
    let (s, u) = f
        .call(
            "POST",
            "/api/v1/users",
            json!({"username":"Alice","password":"correct horse battery"}),
            Some(token),
        )
        .await;
    assert_eq!(s, 200);
    assert!(u.get("password_hash").is_none());
    assert_eq!(
        f.call(
            "DELETE",
            &format!("/api/v1/admins/{}", admin.id),
            Value::Null,
            Some(token)
        )
        .await
        .0,
        409
    );
    let req = Request::builder()
        .method("POST")
        .uri("/api/v1/users")
        .header("cookie", format!("ygg_session={token}"))
        .header("content-type", "application/json")
        .body(Body::from("{}"))
        .unwrap();
    assert_eq!(
        f.app.router.clone().oneshot(req).await.unwrap().status(),
        403
    );
    let req = Request::builder()
        .method("POST")
        .uri("/api/v1/auth/logout")
        .header("cookie", format!("ygg_session={token}"))
        .header("origin", "http://127.0.0.1:25585")
        .header("content-type", "application/json")
        .body(Body::from("{}"))
        .unwrap();
    assert_eq!(
        f.app.router.clone().oneshot(req).await.unwrap().status(),
        200
    );
    assert_eq!(
        f.call("GET", "/api/v1/users", Value::Null, Some(token))
            .await
            .0,
        401
    );
}

async fn mock(router: Router) -> (String, tokio::task::JoinHandle<()>) {
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let url = format!("http://{}", listener.local_addr().unwrap());
    let task = tokio::spawn(async move { axum::serve(listener, router).await.unwrap() });
    (url, task)
}
fn fed_request(op: Operation, path: &str, body: Value) -> FederationRequest {
    FederationRequest {
        operation: op,
        method: if op == Operation::HasJoined {
            Method::GET
        } else {
            Method::POST
        },
        path: path.into(),
        headers: HeaderMap::from_iter([(
            http::header::CONTENT_TYPE,
            http::HeaderValue::from_static("application/json"),
        )]),
        body: if op == Operation::HasJoined {
            vec![]
        } else {
            serde_json::to_vec(&body).unwrap()
        },
        ip: "127.0.0.1".into(),
    }
}

#[tokio::test]
async fn federation_affinity_survives_rule_changes_and_restart() {
    let calls = Arc::new(AtomicUsize::new(0));
    let c = calls.clone();
    let router=Router::new().route("/authserver/authenticate",post(||async{axum::Json(json!({"accessToken":"remote-secret","clientToken":"client","selectedProfile":{"id":"b50ad385829d3141a2167e7d7539ba7f","name":"Alice"}}))}))
        .route("/authserver/refresh",post(move||{let c=c.clone();async move{c.fetch_add(1,Ordering::SeqCst);axum::Json(json!({"accessToken":"rotated-secret","clientToken":"client","selectedProfile":{"id":"b50ad385829d3141a2167e7d7539ba7f","name":"Alice"}}))}}))
        .route("/sessionserver/session/minecraft/join",post(||async{StatusCode::NO_CONTENT}))
        .route("/sessionserver/session/minecraft/hasJoined",get(||async{axum::Json(json!({"id":"b50ad385829d3141a2167e7d7539ba7f","name":"Alice","properties":[{"name":"textures","value":"e30=","signature":"untrusted"}]}))}));
    let (url, task) = mock(router).await;
    let f = Fixture::new().await;
    let upstream = Upstream {
        id: "remote".into(),
        base_url: url,
        ..Upstream::default()
    };
    transact(f.app.service.repo.as_ref(), |s| {
        s.upstreams.insert(upstream.id.clone(), upstream.clone());
        s.rules.insert(
            "login".into(),
            Rule {
                id: "login".into(),
                operation: Operation::Authenticate,
                match_priority: 0,
                strategy: Strategy::Pinned,
                targets: vec![Target {
                    source: "remote".into(),
                    priority: None,
                }],
                enabled: true,
                reject_conflicts: false,
            },
        );
        Ok(())
    })
    .await
    .unwrap();
    let fed = Federation {
        local: f.app.service.clone(),
        clients: f.app.clients.clone(),
    };
    fed.execute(fed_request(
        Operation::Authenticate,
        "/authserver/authenticate",
        json!({"username":"Alice","password":"pw"}),
    ))
    .await
    .unwrap();
    let restarted = build(&f.config).await.unwrap();
    let fed = Federation {
        local: restarted.service.clone(),
        clients: restarted.clients.clone(),
    };
    fed.execute(fed_request(
        Operation::Refresh,
        "/authserver/refresh",
        json!({"accessToken":"remote-secret"}),
    ))
    .await
    .unwrap();
    assert_eq!(calls.load(Ordering::SeqCst), 1);
    assert!(fed
        .execute(fed_request(
            Operation::Refresh,
            "/authserver/refresh",
            json!({"accessToken":"unknown"})
        ))
        .await
        .is_err());
    fed.execute(fed_request(Operation::Join,"/sessionserver/session/minecraft/join",json!({"accessToken":"rotated-secret","selectedProfile":"b50ad385829d3141a2167e7d7539ba7f","serverId":"s"}))).await.unwrap();
    let r = fed
        .execute(fed_request(
            Operation::HasJoined,
            "/sessionserver/session/minecraft/hasJoined?serverId=s&username=Alice",
            Value::Null,
        ))
        .await
        .unwrap();
    let v: Value = serde_json::from_slice(&r.body).unwrap();
    assert_ne!(v["properties"][0]["signature"], "untrusted");
    let (_, state) = restarted.service.repo.load().await.unwrap();
    assert!(!serde_json::to_string(&state)
        .unwrap()
        .contains("rotated-secret"));
    task.abort();
}

#[tokio::test]
async fn merge_priority_and_conflict_rejection() {
    let (url, task) = mock(Router::new().route(
        "/api/profiles/minecraft",
        post(|| async {
            axum::Json(json!([{"id":"different","name":"ALICE"},{"id":"other","name":"Bob"}]))
        }),
    ))
    .await;
    let f = Fixture::new().await;
    let (_, p) = f.user("Alice").await;
    let u = Upstream {
        id: "remote".into(),
        base_url: url,
        ..Upstream::default()
    };
    transact(f.app.service.repo.as_ref(), |s| {
        s.upstreams.insert(u.id.clone(), u.clone());
        s.rules.insert(
            "merge".into(),
            Rule {
                id: "merge".into(),
                operation: Operation::ProfileBatch,
                match_priority: 0,
                strategy: Strategy::MergePriority,
                targets: vec![
                    Target {
                        source: "local".into(),
                        priority: Some(0),
                    },
                    Target {
                        source: "remote".into(),
                        priority: Some(1),
                    },
                ],
                enabled: true,
                reject_conflicts: false,
            },
        );
        Ok(())
    })
    .await
    .unwrap();
    let fed = Federation {
        local: f.app.service.clone(),
        clients: f.app.clients.clone(),
    };
    let r = fed
        .execute(fed_request(
            Operation::ProfileBatch,
            "/api/profiles/minecraft",
            json!(["Alice", "Bob"]),
        ))
        .await
        .unwrap();
    let v: Vec<Value> = serde_json::from_slice(&r.body).unwrap();
    assert_eq!(v.len(), 2);
    assert_eq!(v[0]["id"], p.id);
    transact(f.app.service.repo.as_ref(), |s| {
        s.rules.get_mut("merge").unwrap().reject_conflicts = true;
        Ok(())
    })
    .await
    .unwrap();
    assert!(matches!(
        fed.execute(fed_request(
            Operation::ProfileBatch,
            "/api/profiles/minecraft",
            json!(["Alice"])
        ))
        .await,
        Err(Error::Conflict)
    ));
    task.abort();
}

#[tokio::test]
async fn gateway_serves_local_accounts_and_two_independent_authorities() {
    let a = Fixture::new().await;
    let b = Fixture::new().await;
    let (_, pa) = a.user("RemoteA").await;
    let (_, pb) = b.user("RemoteB").await;
    a.app
        .service
        .create_user("Local".into(), "remote-only-password".into(), false)
        .await
        .unwrap();
    let (url_a, task_a) = mock(a.app.router.clone()).await;
    let (url_b, task_b) = mock(b.app.router.clone()).await;
    let f = Fixture::new().await;
    let admin = f.app.management.launcher_session().await.unwrap();
    for (id, url) in [("a", url_a), ("b", url_b)] {
        let (status, _) = f
            .call(
                "POST",
                "/api/v1/upstreams",
                json!({
                    "id":id,"base_url":url,"retry_count":0,"request_timeout_ms":15000
                }),
                Some(&admin),
            )
            .await;
        assert_eq!(status, 200);
    }
    for (op, strategy, targets) in [
        (
            "authenticate",
            "pinned",
            json!([{"source":"a","priority":0}]),
        ),
        (
            "profile_batch",
            "merge_priority",
            json!([{"source":"local","priority":0},{"source":"*","priority":10}]),
        ),
        (
            "profile_lookup",
            "priority_failover",
            json!([{"source":"local","priority":0},{"source":"*","priority":10}]),
        ),
        (
            "profile_by_name",
            "priority_failover",
            json!([{"source":"local","priority":0},{"source":"*","priority":10}]),
        ),
    ] {
        assert_eq!(
            f.call(
                "POST",
                "/api/v1/federation/rules",
                json!({
                    "id":op,"operation":op,"match_priority":0,"strategy":strategy,
                    "targets":targets,"enabled":true
                }),
                Some(&admin)
            )
            .await
            .0,
            200
        );
    }
    let (status, local) = f
        .call(
            "POST",
            "/api/v1/auth/register",
            json!({
                "username":"Local","password":"correct horse battery","profileName":"Local"
            }),
            None,
        )
        .await;
    assert_eq!(status, 200);
    // A pinned remote login rule must not suppress the gateway's own accounts.
    let (status, login) = f
        .call(
            "POST",
            "/authserver/authenticate",
            json!({
                "username":"Local","password":"correct horse battery"
            }),
            None,
        )
        .await;
    assert_eq!(status, 200);
    assert_eq!(
        login["selectedProfile"]["id"],
        local["selectedProfile"]["id"]
    );
    assert_eq!(
        f.call(
            "POST",
            "/authserver/authenticate",
            json!({
                "username":"Local","password":"remote-only-password"
            }),
            None
        )
        .await
        .0,
        403
    );
    // Explicit prefixes select only that authority, and are stripped upstream.
    for (name, profile) in [("a::RemoteA", &pa), ("b::RemoteB", &pb)] {
        let (status, login) = f.call("POST", "/authserver/authenticate", json!({
            "username":name,"password":"correct horse battery","clientToken":"gateway-client"
        }), None).await;
        assert_eq!(status, 200);
        assert_eq!(login["selectedProfile"]["id"], profile.id);
        let (status, refreshed) = f
            .call(
                "POST",
                "/authserver/refresh",
                json!({
                    "accessToken":login["accessToken"],"clientToken":"gateway-client"
                }),
                None,
            )
            .await;
        assert_eq!(status, 200);
        assert_eq!(f.call("POST", "/sessionserver/session/minecraft/join", json!({
            "accessToken":refreshed["accessToken"],"selectedProfile":profile.id,"serverId":"gateway-test"
        }), None).await.0, 204);
        let (status, joined) = f
            .call(
                "GET",
                &format!(
                    "/sessionserver/session/minecraft/hasJoined?username={}&serverId=gateway-test",
                    profile.name
                ),
                Value::Null,
                None,
            )
            .await;
        assert_eq!(status, 200);
        assert_eq!(joined["id"], profile.id);
        let property = &joined["properties"][0];
        let verifier = rsa::pkcs1v15::VerifyingKey::<sha1::Sha1>::new(
            rsa::RsaPublicKey::from_public_key_pem(&f.app.service.public_key().unwrap()).unwrap(),
        );
        verifier
            .verify(
                property["value"].as_str().unwrap().as_bytes(),
                &rsa::pkcs1v15::Signature::try_from(
                    B64.decode(property["signature"].as_str().unwrap())
                        .unwrap()
                        .as_slice(),
                )
                .unwrap(),
            )
            .unwrap();
        assert_eq!(
            f.call(
                "POST",
                "/authserver/signout",
                json!({
                    "username":name,"password":"correct horse battery"
                }),
                None
            )
            .await
            .0,
            204
        );
        assert_eq!(
            f.call(
                "POST",
                "/authserver/validate",
                json!({
                    "accessToken":refreshed["accessToken"]
                }),
                None
            )
            .await
            .0,
            403
        );
    }
    let (status, profiles) = f
        .call(
            "POST",
            "/api/profiles/minecraft",
            json!(["Local", "RemoteA", "RemoteB"]),
            None,
        )
        .await;
    assert_eq!(status, 200);
    assert_eq!(profiles.as_array().unwrap().len(), 3);
    for p in [&pa, &pb] {
        assert_eq!(
            f.call(
                "GET",
                &format!("/users/profiles/minecraft/{}", p.name),
                Value::Null,
                None
            )
            .await
            .1["id"],
            p.id
        );
        assert_eq!(
            f.call(
                "GET",
                &format!("/sessionserver/session/minecraft/profile/{}", p.id),
                Value::Null,
                None
            )
            .await
            .1["id"],
            p.id
        );
    }
    assert_eq!(
        f.call(
            "POST",
            "/authserver/authenticate",
            json!({
                "username":"missing::Local","password":"correct horse battery"
            }),
            None
        )
        .await
        .0,
        400
    );
    task_a.abort();
    task_b.abort();
    let _ = task_a.await;
    let _ = task_b.await;
    assert_eq!(
        f.call(
            "POST",
            "/authserver/authenticate",
            json!({
                "username":"b::RemoteB","password":"correct horse battery"
            }),
            None
        )
        .await
        .0,
        502
    );
    assert_eq!(
        f.call(
            "POST",
            "/authserver/authenticate",
            json!({
                "username":"local::Local","password":"correct horse battery"
            }),
            None
        )
        .await
        .0,
        200
    );
}

#[tokio::test]
async fn cache_revalidation_stale_bypass_purge_and_traversal() {
    let hits = Arc::new(AtomicUsize::new(0));
    let h = hits.clone();
    let ua_hits = Arc::new(AtomicUsize::new(0));
    let observed_ua = ua_hits.clone();
    let fail = Arc::new(AtomicUsize::new(0));
    let fails = fail.clone();
    let (url, task) = mock(Router::new().route(
        "/asset",
        get(move |headers: HeaderMap| {
            let h = h.clone();
            let observed_ua = observed_ua.clone();
            let fail = fails.clone();
            async move {
                h.fetch_add(1, Ordering::SeqCst);
                if let Some(value) = headers.get("user-agent") {
                    assert_eq!(value, "LunaLauncher/fixture");
                    observed_ua.fetch_add(1, Ordering::SeqCst);
                }
                if fail.load(Ordering::SeqCst) == 1 {
                    return (
                        StatusCode::SERVICE_UNAVAILABLE,
                        [("etag", "abc"), ("cache-control", "max-age=0")],
                        "down",
                    );
                };
                if headers.contains_key("if-none-match") {
                    (
                        StatusCode::NOT_MODIFIED,
                        [("etag", "abc"), ("cache-control", "max-age=0")],
                        "",
                    )
                } else {
                    (
                        StatusCode::OK,
                        [("etag", "abc"), ("cache-control", "max-age=0")],
                        "content",
                    )
                }
            }
        }),
    ))
    .await;
    let f = Fixture::new().await;
    let u = Upstream {
        id: "assets".into(),
        base_url: url,
        capabilities: vec!["http".into()],
        retry_count: 0,
        ..Upstream::default()
    };
    let route = CacheRoute {
        id: "route".into(),
        listen_prefix: "/cache/test".into(),
        upstream_id: u.id.clone(),
        ..CacheRoute::default()
    };
    transact(f.app.service.repo.as_ref(), |s| {
        s.upstreams.insert(u.id.clone(), u.clone());
        s.cache_routes.insert(route.id.clone(), route.clone());
        Ok(())
    })
    .await
    .unwrap();
    let cache = &f.app.management.cache;
    let mut launcher_headers = HeaderMap::new();
    launcher_headers.insert("user-agent", "LunaLauncher/fixture".parse().unwrap());
    assert_eq!(
        cache
            .handle(Method::GET, "/cache/test/asset", launcher_headers)
            .await
            .unwrap()
            .body,
        b"content"
    );
    assert_eq!(ua_hits.load(Ordering::SeqCst), 1);
    let response = cache
        .handle(Method::GET, "/cache/test/asset", HeaderMap::new())
        .await
        .unwrap();
    assert_eq!(response.body, b"content");
    assert_eq!(response.headers["x-yggdrasil-cache"], "REVALIDATED");
    fail.store(1, Ordering::SeqCst);
    assert_eq!(
        cache
            .handle(Method::GET, "/cache/test/asset", HeaderMap::new())
            .await
            .unwrap()
            .headers["x-yggdrasil-cache"],
        "STALE"
    );
    let mut headers = HeaderMap::new();
    headers.insert("authorization", "Bearer secret".parse().unwrap());
    assert_eq!(
        cache
            .handle(Method::GET, "/cache/test/asset", headers)
            .await
            .unwrap()
            .status,
        503
    );
    assert!(cache
        .handle(Method::GET, "/cache/test/%2e%2e/private", HeaderMap::new())
        .await
        .is_err());
    assert_eq!(cache.purge(Some("route"), None).await.unwrap(), 1);
    assert_eq!(hits.load(Ordering::SeqCst), 4);
    task.abort();
}

#[tokio::test]
async fn cache_private_cookie_and_head_do_not_leak() {
    let (url, task) = mock(
        Router::new()
            .route(
                "/private",
                get(|| async { ([("cache-control", "private")], "secret") }),
            )
            .route(
                "/cookie",
                get(|| async { ([("set-cookie", "sid=x")], "secret") }),
            )
            .route("/public", get(|| async { "public" })),
    )
    .await;
    let f = Fixture::new().await;
    let u = Upstream {
        id: "assets".into(),
        base_url: url,
        capabilities: vec!["http".into()],
        ..Upstream::default()
    };
    let route = CacheRoute {
        id: "route".into(),
        listen_prefix: "/cache/test".into(),
        upstream_id: u.id.clone(),
        ..CacheRoute::default()
    };
    transact(f.app.service.repo.as_ref(), |s| {
        s.upstreams.insert(u.id.clone(), u.clone());
        s.cache_routes.insert(route.id.clone(), route.clone());
        Ok(())
    })
    .await
    .unwrap();
    let cache = &f.app.management.cache;
    for path in ["/cache/test/private", "/cache/test/cookie"] {
        cache
            .handle(Method::GET, path, HeaderMap::new())
            .await
            .unwrap();
    }
    assert_eq!(cache.stats().await.unwrap()["entries"], 0);
    assert!(cache
        .handle(Method::HEAD, "/cache/test/public", HeaderMap::new())
        .await
        .unwrap()
        .body
        .is_empty());
    assert_eq!(
        cache
            .handle(Method::GET, "/cache/test/public", HeaderMap::new())
            .await
            .unwrap()
            .body,
        b"public"
    );
    task.abort();
}

#[tokio::test]
async fn admin_bearer_is_hashed_and_openapi_paths_are_real() {
    let f = Fixture::new().await;
    let token = f.app.management.launcher_session().await.unwrap();
    let (_, s) = f.app.service.repo.load().await.unwrap();
    assert!(!serde_json::to_string(&s).unwrap().contains(&token));
    let (status, doc) = f.call("GET", "/api/openapi.json", Value::Null, None).await;
    assert_eq!(status, 200);
    assert!(doc["paths"]["/api/v1/profiles/{id}/mirror/minecraft"]["post"].is_object());
    assert!(f.dir.path().join("identity.sqlite3").exists());
}

#[tokio::test]
async fn launcher_child_handshake_and_eof_shutdown() {
    use tokio::io::{AsyncBufReadExt, BufReader};
    let dir = tempfile::tempdir().unwrap();
    let mut child = tokio::process::Command::new(env!("CARGO_BIN_EXE_yggdrasil-server"))
        .args(["--launcher", "--listen", "127.0.0.1:0", "--data-dir"])
        .arg(dir.path())
        .stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .kill_on_drop(true)
        .spawn()
        .unwrap();
    let mut lines = BufReader::new(child.stdout.take().unwrap()).lines();
    let line = tokio::time::timeout(std::time::Duration::from_secs(30), lines.next_line())
        .await
        .unwrap()
        .unwrap()
        .unwrap();
    let ready: Value = serde_json::from_str(&line).unwrap();
    assert_eq!(ready["ready"], true);
    let raw = ready["managementToken"].as_str().unwrap();
    let base = ready["url"].as_str().unwrap();
    let upstream = Upstream {
        base_url: base.into(),
        ..Upstream::default()
    };
    let mut headers = HeaderMap::new();
    headers.insert("authorization", format!("Bearer {raw}").parse().unwrap());
    assert_eq!(
        yggdrasil_upstream::Clients::default()
            .fetch(
                &upstream,
                Method::GET,
                "/api/v1/status",
                headers,
                vec![],
                true,
                1024 * 1024
            )
            .await
            .unwrap()
            .status,
        200
    );
    let closed_registration = yggdrasil_upstream::Clients::default().fetch(
        &upstream, Method::POST, "/api/v1/auth/register",
        HeaderMap::from_iter([(http::header::CONTENT_TYPE, http::HeaderValue::from_static("application/json"))]),
        serde_json::to_vec(&json!({"username":"Uninvited","password":"registration-password","profileName":"Uninvited"})).unwrap(),
        false, 1024 * 1024,
    ).await.unwrap();
    assert_eq!(closed_registration.status, 403);
    drop(child.stdin.take());
    let status = tokio::time::timeout(std::time::Duration::from_secs(10), child.wait())
        .await
        .unwrap()
        .unwrap();
    assert!(status.success());
    let app = build(&Config {
        data_dir: dir.path().to_owned(),
        ..Config::default()
    })
    .await
    .unwrap();
    assert!(!app
        .service
        .repo
        .load()
        .await
        .unwrap()
        .1
        .admin_sessions
        .contains_key(&hash(raw)));
}

#[tokio::test]
async fn cache_revalidation_can_revoke_public_storage() {
    let hits = Arc::new(AtomicUsize::new(0));
    let h = hits.clone();
    let (url, task) = mock(Router::new().route(
        "/asset",
        get(move || {
            let h = h.clone();
            async move {
                if h.fetch_add(1, Ordering::SeqCst) == 0 {
                    (
                        StatusCode::OK,
                        [("etag", "abc"), ("cache-control", "max-age=0")],
                        "body",
                    )
                } else {
                    (
                        StatusCode::NOT_MODIFIED,
                        [("etag", "abc"), ("cache-control", "private, max-age=3600")],
                        "",
                    )
                }
            }
        }),
    ))
    .await;
    let f = Fixture::new().await;
    let u = Upstream {
        id: "http".into(),
        base_url: url,
        capabilities: vec!["http".into()],
        ..Upstream::default()
    };
    let route = CacheRoute {
        id: "route".into(),
        upstream_id: u.id.clone(),
        ..CacheRoute::default()
    };
    transact(f.app.service.repo.as_ref(), |s| {
        s.upstreams.insert(u.id.clone(), u.clone());
        s.cache_routes.insert(route.id.clone(), route.clone());
        Ok(())
    })
    .await
    .unwrap();
    let cache = &f.app.management.cache;
    cache
        .handle(Method::GET, "/cache/assets/asset", HeaderMap::new())
        .await
        .unwrap();
    assert_eq!(
        cache
            .handle(Method::GET, "/cache/assets/asset", HeaderMap::new())
            .await
            .unwrap()
            .body,
        b"body"
    );
    assert_eq!(cache.stats().await.unwrap()["entries"], 0);
    task.abort();
}

#[tokio::test]
async fn safe_retries_and_health_thresholds() {
    let hits = Arc::new(AtomicUsize::new(0));
    let h = hits.clone();
    let (url, task) = mock(Router::new().fallback(move || {
        let h = h.clone();
        async move {
            h.fetch_add(1, Ordering::SeqCst);
            StatusCode::SERVICE_UNAVAILABLE
        }
    }))
    .await;
    let u = Upstream {
        base_url: url,
        retry_count: 2,
        health: HealthPolicy {
            failure_threshold: 2,
            recovery_threshold: 2,
            ..HealthPolicy::default()
        },
        ..Upstream::default()
    };
    let clients = yggdrasil_upstream::Clients::default();
    clients
        .fetch(
            &u,
            Method::POST,
            "/authserver/refresh",
            HeaderMap::new(),
            b"{}".to_vec(),
            false,
            1024,
        )
        .await
        .unwrap();
    assert_eq!(hits.load(Ordering::SeqCst), 1);
    clients
        .fetch(&u, Method::GET, "/", HeaderMap::new(), vec![], true, 1024)
        .await
        .unwrap();
    assert_eq!(hits.load(Ordering::SeqCst), 4);
    assert_ne!(clients.probe(&u).await.healthy, Some(false));
    assert_eq!(clients.probe(&u).await.healthy, Some(false));
    task.abort();
}

#[tokio::test]
async fn socks5_local_and_remote_dns_and_http_proxy() {
    use tokio::io::{AsyncReadExt, AsyncWriteExt};
    let clients = yggdrasil_upstream::Clients::default();
    for remote in [false, true] {
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let address = listener.local_addr().unwrap();
        let task = tokio::spawn(async move {
            let (mut stream, _) = listener.accept().await.unwrap();
            let mut hello = [0u8; 2];
            stream.read_exact(&mut hello).await.unwrap();
            assert_eq!(hello[0], 5);
            let mut methods = vec![0; hello[1] as usize];
            stream.read_exact(&mut methods).await.unwrap();
            stream.write_all(&[5, 0]).await.unwrap();
            let mut request = [0u8; 4];
            stream.read_exact(&mut request).await.unwrap();
            assert_eq!(request[0..3], [5, 1, 0]);
            let host = match request[3] {
                1 => {
                    let mut ip = [0u8; 4];
                    stream.read_exact(&mut ip).await.unwrap();
                    std::net::Ipv4Addr::from(ip).to_string()
                }
                3 => {
                    let n = stream.read_u8().await.unwrap();
                    let mut name = vec![0; n as usize];
                    stream.read_exact(&mut name).await.unwrap();
                    String::from_utf8(name).unwrap()
                }
                _ => panic!("unexpected address"),
            };
            assert_eq!(request[3], if remote { 3 } else { 1 });
            assert_eq!(
                host,
                if remote {
                    "example.invalid"
                } else {
                    "127.0.0.1"
                }
            );
            let _port = stream.read_u16().await.unwrap();
            stream
                .write_all(&[5, 0, 0, 1, 127, 0, 0, 1, 0, 80])
                .await
                .unwrap();
            let mut buf = [0; 4096];
            let n = stream.read(&mut buf).await.unwrap();
            let request = String::from_utf8_lossy(&buf[..n]).to_ascii_lowercase();
            assert!(request.contains("get /asset"));
            assert!(request.contains("user-agent: lunalauncher/fixture"));
            stream
                .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok")
                .await
                .unwrap();
        });
        let scheme = if remote { "socks5h" } else { "socks5" };
        let proxy_url = format!("{scheme}://{address}");
        let u = Upstream {
            base_url: if remote {
                "http://example.invalid"
            } else {
                "http://127.0.0.1"
            }
            .into(),
            transport: if remote {
                Transport::Socks5h { proxy_url }
            } else {
                Transport::Socks5 { proxy_url }
            },
            ..Upstream::default()
        };
        assert_eq!(
            clients
                .fetch(
                    &u,
                    Method::GET,
                    "/asset",
                    {
                        let mut headers = HeaderMap::new();
                        headers.insert("user-agent", "LunaLauncher/fixture".parse().unwrap());
                        headers
                    },
                    vec![],
                    true,
                    1024
                )
                .await
                .unwrap()
                .body,
            b"ok"
        );
        task.await.unwrap();
    }
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let task = tokio::spawn(async move {
        let (mut stream, _) = listener.accept().await.unwrap();
        let mut b = [0; 4096];
        let n = stream.read(&mut b).await.unwrap();
        assert!(String::from_utf8_lossy(&b[..n]).starts_with("GET http://example.invalid/asset "));
        let request = String::from_utf8_lossy(&b[..n]).to_ascii_lowercase();
        assert!(request.contains("user-agent: lunalauncher/fixture"));
        stream
            .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok")
            .await
            .unwrap();
    });
    let u = Upstream {
        base_url: "http://example.invalid".into(),
        transport: Transport::HttpProxy {
            proxy_url: format!("http://{address}"),
        },
        // A configured default must not replace the launcher request header.
        extra_headers: [("user-agent".to_owned(), "gateway-default".to_owned())]
            .into_iter()
            .collect(),
        ..Upstream::default()
    };
    assert_eq!(
        clients
            .fetch(
                &u,
                Method::GET,
                "/asset",
                {
                    let mut headers = HeaderMap::new();
                    headers.insert("user-agent", "LunaLauncher/fixture".parse().unwrap());
                    headers
                },
                vec![],
                true,
                1024
            )
            .await
            .unwrap()
            .body,
        b"ok"
    );
    task.await.unwrap();
}

/// Supply a disposable database: this checks the same migration/CAS contract on
/// real PostgreSQL/MySQL instead of claiming SQLite is evidence for both.
#[tokio::test]
#[ignore = "requires YGGDRASIL_TEST_DATABASE_URL pointing to a disposable PostgreSQL or MySQL database"]
async fn external_database_migration_and_compare_exchange() {
    let url = std::env::var("YGGDRASIL_TEST_DATABASE_URL").unwrap();
    let repo = yggdrasil_storage::SqlRepository::connect(&url)
        .await
        .unwrap();
    let (version, s) = repo.load().await.unwrap();
    assert!(repo.compare_exchange(version, &s).await.unwrap());
    assert!(!repo.compare_exchange(version, &s).await.unwrap());
}
