use axum::{
    body::{to_bytes, Body},
    http::Request,
};
use serde_json::{json, Value};
use tower::ServiceExt;
use yggdrasil_server::{build, Config};

async fn call(
    router: &axum::Router,
    token: &str,
    method: &str,
    path: &str,
    value: Value,
) -> (u16, Value) {
    let request = Request::builder()
        .method(method)
        .uri(format!("/api/v1/{path}"))
        .header("authorization", format!("Bearer {token}"))
        .header("content-type", "application/json")
        .body(Body::from(serde_json::to_vec(&value).unwrap()))
        .unwrap();
    let response = router.clone().oneshot(request).await.unwrap();
    let status = response.status().as_u16();
    let bytes = to_bytes(response.into_body(), 4 * 1024 * 1024)
        .await
        .unwrap();
    (status, serde_json::from_slice(&bytes).unwrap())
}

#[tokio::test]
async fn resources_validate_references_redact_secrets_and_delete_atomically() {
    let dir = tempfile::tempdir().unwrap();
    let app = build(&Config {
        data_dir: dir.path().to_owned(),
        ..Config::default()
    })
    .await
    .unwrap();
    let token = app.management.launcher_session().await.unwrap();
    let router = &app.router;
    let (status, user) = call(
        router,
        &token,
        "POST",
        "users",
        json!({"username":"Alice","password":"password-for-tests"}),
    )
    .await;
    assert_eq!(status, 200);
    assert!(user.get("password_hash").is_none());
    let (status, profile) = call(
        router,
        &token,
        "POST",
        "profiles",
        json!({"user_id":user["id"],"name":"Alice","uuid_scheme":{"kind":"offline_player"}}),
    )
    .await;
    assert_eq!(status, 200);
    assert_eq!(
        call(
            router,
            &token,
            "POST",
            &format!("profiles/{}/rename", profile["id"].as_str().unwrap()),
            json!({"name":"NewName"})
        )
        .await
        .0,
        200
    );
    assert_eq!(
        call(
            router,
            &token,
            "GET",
            &format!("profiles/{}/history", profile["id"].as_str().unwrap()),
            Value::Null
        )
        .await
        .1
        .as_array()
        .unwrap()
        .len(),
        2
    );
    let(status,upstream)=call(router,&token,"POST","upstreams",json!({"id":"source","base_url":"https://example.invalid","capabilities":["http","profiles"],"transport":{"kind":"socks5h","proxy_url":"socks5h://username:password@127.0.0.1:1080"},"extra_headers":{"x-api-key":"private-header"}})).await;
    assert_eq!(status, 200);
    let encoded = upstream.to_string();
    assert!(!encoded.contains("password"));
    assert!(!encoded.contains("private-header"));
    let route = json!({"id":"assets","upstream_id":"source","listen_prefix":"/cache/assets"});
    assert_eq!(
        call(router, &token, "POST", "cache/routes", route).await.0,
        200
    );
    assert_eq!(
        call(router, &token, "DELETE", "upstreams/source", Value::Null)
            .await
            .0,
        409
    );
    assert_eq!(
        call(
            router,
            &token,
            "PATCH",
            "cache/routes/assets",
            json!({"ttl_seconds":12})
        )
        .await
        .1["ttl_seconds"],
        12
    );
    let rule = json!({"id":"lookup","operation":"profile_batch","match_priority":0,"strategy":"merge_priority","targets":[{"source":"source","priority":0}],"enabled":true});
    assert_eq!(
        call(
            router,
            &token,
            "POST",
            "federation/rules/validate",
            rule.clone()
        )
        .await
        .0,
        200
    );
    assert_eq!(
        call(router, &token, "POST", "federation/rules", rule)
            .await
            .0,
        200
    );
    assert_eq!(
        call(router, &token, "DELETE", "cache/routes/assets", Value::Null)
            .await
            .0,
        200
    );
    assert_eq!(
        call(router, &token, "DELETE", "upstreams/source", Value::Null)
            .await
            .0,
        409
    );
    assert_eq!(
        call(
            router,
            &token,
            "DELETE",
            "federation/rules/lookup",
            Value::Null
        )
        .await
        .0,
        200
    );
    assert_eq!(
        call(router, &token, "DELETE", "upstreams/source", Value::Null)
            .await
            .0,
        200
    );
    assert_eq!(
        call(
            router,
            &token,
            "DELETE",
            &format!("users/{}", user["id"].as_str().unwrap()),
            Value::Null
        )
        .await
        .0,
        200
    );
    assert_eq!(
        call(
            router,
            &token,
            "GET",
            &format!("profiles/{}", profile["id"].as_str().unwrap()),
            Value::Null
        )
        .await
        .0,
        404
    );
}

#[tokio::test]
async fn public_registration_creates_local_source_and_can_be_disabled() {
    let dir = tempfile::tempdir().unwrap();
    let app = build(&Config {
        data_dir: dir.path().to_owned(),
        ..Config::default()
    })
    .await
    .unwrap();
    let request = Request::builder()
        .method("POST")
        .uri("/api/v1/auth/register")
        .header("content-type", "application/json")
        .body(Body::from(
            serde_json::to_vec(&json!({
                "username":"PublicUser",
                "password":"a-secure-registration-password",
                "profileName":"PublicUser",
                "requestUser":true
            }))
            .unwrap(),
        ))
        .unwrap();
    let response = app.router.clone().oneshot(request).await.unwrap();
    assert_eq!(response.status(), 200);
    let body = to_bytes(response.into_body(), 1024 * 1024).await.unwrap();
    let value: Value = serde_json::from_slice(&body).unwrap();
    assert!(value["accessToken"].as_str().is_some_and(|v| !v.is_empty()));
    assert_eq!(value["user"]["username"], "PublicUser");
    assert_eq!(value["selectedProfile"]["name"], "PublicUser");
    let (_, state) = app.service.repo.load().await.unwrap();
    assert_eq!(state.users.len(), 1);
    assert_eq!(state.profiles.len(), 1);
    let token = value["accessToken"].as_str().unwrap();
    app.service
        .validate(&yggdrasil_service::TokenRequest {
            access_token: token.into(),
            client_token: value["clientToken"].as_str().map(str::to_owned),
            selected_profile: None,
            request_user: false,
        })
        .await
        .unwrap();
    let account = call(&app.router, token, "GET", "account", Value::Null).await;
    assert_eq!(account.0, 200);
    assert_eq!(
        account.1["profiles"][0]["id"],
        value["selectedProfile"]["id"]
    );
    assert!(!account.1.to_string().contains("password"));
    app.service
        .join(
            yggdrasil_service::JoinRequest {
                access_token: token.into(),
                selected_profile: value["selectedProfile"]["id"].as_str().unwrap().into(),
                server_id: "password-change".into(),
            },
            "127.0.0.1".into(),
        )
        .await
        .unwrap();
    assert_eq!(
        call(
            &app.router,
            token,
            "POST",
            "account/password",
            json!({
                "currentPassword":"wrong","password":"new-secure-password"
            })
        )
        .await
        .0,
        403
    );
    assert_eq!(
        call(&app.router, token, "GET", "account", Value::Null)
            .await
            .0,
        200
    );
    assert_eq!(
        call(
            &app.router,
            token,
            "POST",
            "account/password",
            json!({
                "currentPassword":"a-secure-registration-password","password":"new-secure-password"
            })
        )
        .await
        .0,
        200
    );
    assert_eq!(
        call(&app.router, token, "GET", "account", Value::Null)
            .await
            .0,
        403
    );
    assert!(app
        .service
        .has_joined("password-change", "PublicUser", None)
        .await
        .unwrap()
        .is_none());
    let credentials = |password: &str| yggdrasil_service::Credentials {
        username: "PublicUser".into(),
        password: password.into(),
        client_token: None,
        request_user: false,
    };
    assert!(app
        .service
        .authenticate(credentials("a-secure-registration-password"))
        .await
        .is_err());
    assert!(app
        .service
        .authenticate(credentials("new-secure-password"))
        .await
        .is_ok());
    let admin = app.management.launcher_session().await.unwrap();
    assert_eq!(
        call(&app.router, &admin, "GET", "account", Value::Null)
            .await
            .0,
        403
    );

    let closed_dir = tempfile::tempdir().unwrap();
    let closed = build(&Config {
        data_dir: closed_dir.path().to_owned(),
        registration_enabled: false,
        ..Config::default()
    })
    .await
    .unwrap();
    let request = Request::builder()
        .method("POST")
        .uri("/api/v1/auth/register")
        .header("content-type", "application/json")
        .body(Body::from(
            br#"{"username":"Closed","password":"a-secure-registration-password"}"#.to_vec(),
        ))
        .unwrap();
    assert_eq!(
        closed
            .router
            .clone()
            .oneshot(request)
            .await
            .unwrap()
            .status(),
        403
    );
}

#[tokio::test]
async fn registration_is_atomic_rejects_privileged_fields_and_limits_peers() {
    let dir = tempfile::tempdir().unwrap();
    let app = build(&Config {
        data_dir: dir.path().to_owned(),
        ..Config::default()
    })
    .await
    .unwrap();
    let payload =
        json!({"username":"Alice","password":"registration-password","profileName":"Alice"});
    let (first, second) = tokio::join!(
        call(&app.router, "", "POST", "auth/register", payload.clone()),
        call(&app.router, "", "POST", "auth/register", payload.clone())
    );
    assert!((first.0 == 200 && second.0 == 409) || (first.0 == 409 && second.0 == 200));
    let token = if first.0 == 200 {
        &first.1["accessToken"]
    } else {
        &second.1["accessToken"]
    };
    assert_eq!(
        call(
            &app.router,
            token.as_str().unwrap(),
            "GET",
            "users",
            Value::Null
        )
        .await
        .0,
        401
    );
    let mut conflict = payload.clone();
    conflict["username"] = json!("Bob");
    assert_eq!(
        call(&app.router, "", "POST", "auth/register", conflict)
            .await
            .0,
        409
    );
    for (field, value) in [
        ("admin", json!(true)),
        (
            "uuid_scheme",
            json!({"kind":"fixed","uuid":"123456789abc4def8123456789abcdef"}),
        ),
        ("profileName", json!(42)),
    ] {
        let mut invalid = payload.clone();
        invalid[field] = value;
        assert_eq!(
            call(&app.router, "", "POST", "auth/register", invalid)
                .await
                .0,
            400
        );
    }
    let request = Request::builder()
        .method("POST")
        .uri("/api/v1/auth/register")
        .header("origin", "https://foreign.invalid")
        .header("content-type", "application/json")
        .body(Body::from(payload.to_string()))
        .unwrap();
    assert_eq!(
        app.router.clone().oneshot(request).await.unwrap().status(),
        403
    );
    let (_, state) = app.service.repo.load().await.unwrap();
    assert_eq!(
        (
            state.users.len(),
            state.profiles.len(),
            state.tokens.len(),
            state.admins.len()
        ),
        (1, 1, 1, 0)
    );
    assert!(!serde_json::to_string(&state)
        .unwrap()
        .contains("registration-password"));
    // Varying usernames must not bypass the per-peer budget.
    for i in 0..4 {
        assert_eq!(
            call(
                &app.router,
                "",
                "POST",
                "auth/register",
                json!({
                    "username":format!("invalid{i}"),"password":"short"
                })
            )
            .await
            .0,
            400
        );
    }
    assert_eq!(
        call(
            &app.router,
            "",
            "POST",
            "auth/register",
            json!({
                "username":"fresh","password":"registration-password"
            })
        )
        .await
        .0,
        429
    );
}

#[tokio::test]
async fn protocol_multipart_texture_upload_requires_owned_token() {
    let dir = tempfile::tempdir().unwrap();
    let app = build(&Config {
        data_dir: dir.path().to_owned(),
        ..Config::default()
    })
    .await
    .unwrap();
    let user = app
        .service
        .create_user("Alice".into(), "password-for-tests".into(), false)
        .await
        .unwrap();
    let profile = app
        .service
        .create_profile(
            user.id,
            "Alice".into(),
            yggdrasil_core::UuidScheme::default(),
        )
        .await
        .unwrap();
    let auth = app
        .service
        .authenticate(yggdrasil_service::Credentials {
            username: "Alice".into(),
            password: "password-for-tests".into(),
            client_token: None,
            request_user: false,
        })
        .await
        .unwrap();
    let mut png = std::io::Cursor::new(vec![]);
    image::RgbaImage::new(64, 64)
        .write_to(&mut png, image::ImageFormat::Png)
        .unwrap();
    let mut body=b"--texture-boundary\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nslim\r\n--texture-boundary\r\nContent-Disposition: form-data; name=\"file\"; filename=\"skin.png\"\r\nContent-Type: image/png\r\n\r\n".to_vec();
    body.extend(png.into_inner());
    body.extend(b"\r\n--texture-boundary--\r\n");
    let request = Request::builder()
        .method("PUT")
        .uri(format!("/api/user/profile/{}/skin", profile.id))
        .header("authorization", format!("Bearer {}", auth.access_token))
        .header(
            "content-type",
            "multipart/form-data; boundary=texture-boundary",
        )
        .body(Body::from(body))
        .unwrap();
    assert_eq!(
        app.router.clone().oneshot(request).await.unwrap().status(),
        204
    );
    let (_, state) = app.service.repo.load().await.unwrap();
    assert!(state.profiles[&profile.id].slim);
    assert!(state.profiles[&profile.id].skin.is_some());
    let request = Request::builder()
        .method("DELETE")
        .uri(format!("/api/user/profile/{}/skin", profile.id))
        .header("authorization", "Bearer invalid")
        .body(Body::empty())
        .unwrap();
    assert_eq!(
        app.router.clone().oneshot(request).await.unwrap().status(),
        403
    );
}
