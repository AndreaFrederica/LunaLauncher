//! JSON schemas originate in the Rust resource DTOs, so downstream clients see
//! the actual configuration fields and tagged enum variants.
use schemars::{schema_for, JsonSchema};
use serde::Deserialize;
use serde_json::{json, Value};
use yggdrasil_core::{AccountSnapshot, CacheRoute, Profile, Rule, Upstream, UuidScheme};

#[derive(Deserialize, JsonSchema)]
#[allow(dead_code)]
struct UserCreate {
    username: String,
    password: String,
}
#[derive(Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub(crate) struct Register {
    pub username: String,
    pub password: String,
    #[serde(rename = "profileName", alias = "profile_name")]
    pub profile_name: Option<String>,
    #[serde(rename = "clientToken", alias = "client_token")]
    pub client_token: Option<String>,
    #[serde(default, rename = "requestUser")]
    pub request_user: bool,
}
#[derive(Deserialize, JsonSchema)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub(crate) struct OwnPassword {
    pub current_password: String,
    pub password: String,
}
#[derive(Deserialize, JsonSchema)]
#[allow(dead_code)]
struct UserPatch {
    username: Option<String>,
    enabled: Option<bool>,
}
#[derive(Deserialize, JsonSchema)]
#[allow(dead_code)]
struct ProfileCreate {
    user_id: String,
    name: String,
    #[serde(default)]
    uuid_scheme: UuidScheme,
}
#[derive(Deserialize, JsonSchema)]
#[allow(dead_code)]
struct Rename {
    name: String,
}
#[derive(Deserialize, JsonSchema)]
#[allow(dead_code)]
struct Password {
    password: String,
}
#[derive(Deserialize, JsonSchema)]
#[allow(dead_code)]
struct Mirror {
    access_token: String,
}
#[derive(Deserialize, JsonSchema)]
#[allow(dead_code)]
struct Purge {
    route_id: Option<String>,
    key: Option<String>,
}

fn references(v: &mut Value) {
    match v {
        Value::Object(obj) => {
            for (k, v) in obj {
                if k == "$ref" {
                    if let Some(s) = v.as_str() {
                        *v = json!(s.replace("#/$defs/", "#/components/schemas/"));
                    }
                } else {
                    references(v)
                }
            }
        }
        Value::Array(values) => {
            for v in values {
                references(v)
            }
        }
        _ => {}
    }
}
fn add<T: JsonSchema>(out: &mut serde_json::Map<String, Value>, name: &str) {
    let mut value = serde_json::to_value(schema_for!(T)).expect("JSON schema is serializable");
    references(&mut value);
    if let Some(defs) = value
        .as_object_mut()
        .and_then(|v| v.remove("$defs"))
        .and_then(|v| v.as_object().cloned())
    {
        out.extend(defs)
    }
    value.as_object_mut().unwrap().remove("$schema");
    out.insert(name.into(), value);
}
pub fn enhance(doc: &mut Value) {
    doc["openapi"] = json!("3.1.0");
    let out = doc["components"]["schemas"].as_object_mut().unwrap();
    add::<Upstream>(out, "Upstream");
    add::<Rule>(out, "Rule");
    add::<CacheRoute>(out, "CacheRoute");
    add::<Profile>(out, "Profile");
    add::<UserCreate>(out, "UserCreate");
    add::<Register>(out, "Register");
    add::<OwnPassword>(out, "OwnPassword");
    add::<UserPatch>(out, "UserPatch");
    add::<ProfileCreate>(out, "ProfileCreate");
    add::<Rename>(out, "Rename");
    add::<Password>(out, "Password");
    add::<Mirror>(out, "Mirror");
    add::<Purge>(out, "Purge");
    add::<AccountSnapshot>(out, "AccountSnapshot");
    add::<crate::launcher::LauncherCacheConfig>(out, "LauncherCacheConfig");
    for name in ["Upstream", "Rule", "CacheRoute"] {
        let mut patch = out[name].clone();
        patch.as_object_mut().unwrap().remove("required");
        out.insert(format!("{name}Patch"), patch);
    }
    for (path, ops) in doc["paths"].as_object_mut().unwrap() {
        for (method, op) in ops.as_object_mut().unwrap() {
            let schema = if path == "/api/v1/launcher/cache" && method == "put" {
                Some("LauncherCacheConfig")
            } else if path == "/api/v1/account-snapshots/{id}" && method == "put" {
                Some("AccountSnapshot")
            } else if path.ends_with("auth/login") {
                Some("UserCreate")
            } else if path.ends_with("auth/register") {
                Some("Register")
            } else if path == "/api/v1/account/password" {
                Some("OwnPassword")
            } else if path.ends_with("/password") {
                Some("Password")
            } else if path.ends_with("/rename")
                || (path == "/api/v1/profiles/{id}" && method == "patch")
            {
                Some("Rename")
            } else if path.ends_with("mirror/minecraft") {
                Some("Mirror")
            } else if path == "/api/v1/cache/purge" {
                Some("Purge")
            } else if path == "/api/v1/profiles" && method == "post" {
                Some("ProfileCreate")
            } else if matches!(path.as_str(), "/api/v1/users" | "/api/v1/admins")
                && method == "post"
            {
                Some("UserCreate")
            } else if matches!(path.as_str(), "/api/v1/users/{id}" | "/api/v1/admins/{id}")
                && method == "patch"
            {
                Some("UserPatch")
            } else if path.starts_with("/api/v1/upstreams")
                && matches!(method.as_str(), "post" | "patch")
                && !path.ends_with("probe")
            {
                Some("Upstream")
            } else if path.starts_with("/api/v1/federation/rules")
                && matches!(method.as_str(), "post" | "patch")
            {
                Some("Rule")
            } else if path.starts_with("/api/v1/cache/routes")
                && matches!(method.as_str(), "post" | "patch")
            {
                Some("CacheRoute")
            } else {
                None
            };
            if let Some(name) = schema {
                let name =
                    if method == "patch" && matches!(name, "Upstream" | "Rule" | "CacheRoute") {
                        format!("{name}Patch")
                    } else {
                        name.into()
                    };
                op["requestBody"] = json!({"required":true,"content":{"application/json":{"schema":{"$ref":format!("#/components/schemas/{name}")}}}});
            }
            if method == "get"
                && !path.contains("{id}")
                && path != "/api/v1/account"
                && !path.ends_with("status")
                && !path.ends_with("capabilities")
                && !path.ends_with("stats")
                && !path.ends_with("me")
            {
                op["parameters"] = json!([{"name":"limit","in":"query","schema":{"type":"integer","minimum":1,"maximum":200,"default":50}},{"name":"cursor","in":"query","schema":{"type":"string"}}]);
                op["responses"]["200"]["content"]["application/json"]["schema"] = json!({"type":"object","required":["items","next_cursor"],"properties":{"items":{"type":"array","items":{"type":"object"}},"next_cursor":{"type":["string","null"]}}});
            }
        }
    }
}
