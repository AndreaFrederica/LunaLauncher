//! Embedding entry point. Call `build` and serve the returned Router on a listener
//! owned by the host; the host also owns shutdown and background maintenance.
use serde::{Deserialize, Serialize};
use std::{path::PathBuf, sync::Arc};
use yggdrasil_cache::Cache;
use yggdrasil_core::*;
use yggdrasil_federation::Federation;
use yggdrasil_management_api::Management;
use yggdrasil_protocol_http::Protocol;
use yggdrasil_service::Service;
use yggdrasil_storage::{DiskBlobs, SqlRepository};
use yggdrasil_upstream::Clients;

#[derive(Clone, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Config {
    pub listen: String,
    pub public_url: String,
    pub data_dir: PathBuf,
    pub database_url: Option<String>,
    pub static_dir: Option<PathBuf>,
    /// Enable the public `/api/v1/auth/register` endpoint. Keep false for
    /// closed installations where operators provision every local account.
    pub registration_enabled: bool,
}
impl Default for Config {
    fn default() -> Self {
        Self {
            listen: "127.0.0.1:25585".into(),
            public_url: "http://127.0.0.1:25585".into(),
            data_dir: "data".into(),
            database_url: None,
            static_dir: None,
            registration_enabled: true,
        }
    }
}
pub struct Application {
    pub router: axum::Router,
    pub service: Arc<Service>,
    pub management: Management,
    pub clients: Clients,
}
pub async fn build(config: &Config) -> anyhow::Result<Application> {
    let new_directory = !config.data_dir.exists();
    tokio::fs::create_dir_all(&config.data_dir).await?;
    #[cfg(unix)]
    if new_directory {
        use std::os::unix::fs::PermissionsExt;
        tokio::fs::set_permissions(&config.data_dir, std::fs::Permissions::from_mode(0o700))
            .await?;
    }
    #[cfg(not(unix))]
    let _ = new_directory;
    let db_url = config.database_url.clone().unwrap_or_else(|| {
        format!(
            "sqlite://{}?mode=rwc",
            config
                .data_dir
                .join("identity.sqlite3")
                .to_string_lossy()
                .replace('\\', "/")
        )
    });
    let repo: Arc<dyn Repository> = Arc::new(SqlRepository::connect(&db_url).await?);
    let blobs: Arc<dyn BlobStore> =
        Arc::new(DiskBlobs::new(config.data_dir.join("textures")).await?);
    let cache_blobs: Arc<dyn BlobStore> =
        Arc::new(DiskBlobs::new(config.data_dir.join("cache")).await?);
    let service = Arc::new(
        Service::open(
            repo.clone(),
            blobs,
            config.public_url.clone(),
            &config.data_dir.join("signing-key.pem"),
        )
        .await?,
    );
    let clients = Clients::default();
    let cache = Cache::new(repo, cache_blobs, clients.clone());
    let federation = Arc::new(Federation {
        local: service.clone(),
        clients: clients.clone(),
    });
    let database_backend = if db_url.starts_with("postgres") {
        "postgresql"
    } else if db_url.starts_with("mysql") {
        "mysql"
    } else {
        "sqlite"
    }
    .to_owned();
    let management = Management {
        service: service.clone(),
        clients: clients.clone(),
        cache: cache.clone(),
        database_backend,
        registration_enabled: config.registration_enabled,
    };
    let mut router = yggdrasil_protocol_http::router(Protocol { federation, cache })
        .merge(yggdrasil_management_api::router(management.clone()));
    if let Some(dir) = &config.static_dir {
        router = router.fallback_service(
            tower_http::services::ServeDir::new(dir).append_index_html_on_directories(true),
        );
    }
    Ok(Application {
        router,
        service,
        management,
        clients,
    })
}
