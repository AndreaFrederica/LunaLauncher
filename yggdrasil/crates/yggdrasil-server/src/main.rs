use clap::Parser;
use std::{io::Write, path::PathBuf};
use yggdrasil_core::{hash, transact};
use yggdrasil_server::{build, Config};

#[derive(Parser)]
#[command(
    version,
    about = "Yggdrasil authentication, federation and reverse cache for Luna Launcher"
)]
struct Args {
    #[arg(long)]
    config: Option<PathBuf>,
    #[arg(long)]
    data_dir: Option<PathBuf>,
    #[arg(long)]
    listen: Option<String>,
    #[arg(long)]
    public_url: Option<String>,
    #[arg(long, env = "YGGDRASIL_DATABASE_URL", hide_env_values = true)]
    database_url: Option<String>,
    #[arg(long, default_value = "admin")]
    admin_user: String,
    /// Used only to create the first administrator; never written to disk.
    #[arg(long, env = "YGGDRASIL_ADMIN_PASSWORD", hide_env_values = true)]
    admin_password: Option<String>,
    /// Loopback-only child process; emits one JSON ready message and exits at stdin EOF.
    #[arg(long)]
    launcher: bool,
}
#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let args = Args::parse();
    let mut config = if let Some(path) = args.config {
        toml::from_str::<Config>(&tokio::fs::read_to_string(path).await?)?
    } else {
        Config::default()
    };
    if let Some(dir) = args.data_dir {
        config.data_dir = dir
    }
    if let Some(listen) = args.listen {
        config.listen = listen
    }
    if let Some(url) = args.public_url {
        config.public_url = url
    }
    if let Some(db) = args.database_url {
        config.database_url = Some(db)
    }
    let addr: std::net::SocketAddr = config.listen.parse()?;
    if args.launcher && !addr.ip().is_loopback() {
        anyhow::bail!("Launcher mode requires a loopback address.")
    }
    let listener = tokio::net::TcpListener::bind(addr).await?;
    if args.launcher {
        config.public_url = format!("http://{}", listener.local_addr()?);
        // Launcher-managed identities are provisioned through its private API.
        config.registration_enabled = false;
    }
    let app = build(&config).await?;
    if let Some(password) = args.admin_password {
        if app.service.repo.load().await?.1.admins.is_empty() {
            app.service
                .create_user(args.admin_user, password, true)
                .await?;
        }
    }
    let launcher_token = if args.launcher {
        Some(app.management.launcher_session().await?)
    } else {
        None
    };
    if !args.launcher && app.service.repo.load().await?.1.admins.is_empty() {
        anyhow::bail!("Set YGGDRASIL_ADMIN_PASSWORD to initialize the first administrator.")
    }
    let (shutdown_tx, shutdown_rx) = tokio::sync::watch::channel(false);
    let service = app.service.clone();
    let clients = app.clients.clone();
    let cache_directory = config.data_dir.join("cache");
    let mut bg_rx = shutdown_rx.clone();
    let background = tokio::spawn(async move {
        let mut interval = tokio::time::interval(std::time::Duration::from_secs(5));
        loop {
            tokio::select! {_ = bg_rx.changed()=>break,_=interval.tick()=>{tokio::select!{_ = bg_rx.changed()=>break,_=async {
                let _=clients.health_tick(service.repo.as_ref()).await;
                let _=service.cleanup().await;
                if let (Ok((_, state)), Ok(blobs)) = (service.repo.load().await, yggdrasil_storage::DiskBlobs::new(&cache_directory).await) {
                    let live = state.cache_entries.values().map(|e| e.blob.clone()).collect();
                    let _ = blobs.collect(&live).await;
                }
            }=>{}}}}
        }
    });
    if let Some(token) = &launcher_token {
        println!(
            "{}",
            serde_json::json!({"ready":true,"url":config.public_url,"managementToken":token})
        );
        std::io::stdout().flush()?;
    } else {
        eprintln!("Yggdrasil listening on {}", listener.local_addr()?);
    }
    let mut rx = shutdown_rx.clone();
    let notify = shutdown_tx.clone();
    let launcher = args.launcher;
    tokio::spawn(async move {
        if launcher {
            use tokio::io::AsyncReadExt;
            let mut stdin = tokio::io::stdin();
            let mut buf = [0u8; 256];
            loop {
                match stdin.read(&mut buf).await {
                    Ok(0) | Err(_) => break,
                    Ok(_) => {}
                }
            }
        } else {
            wait_for_signal().await;
        }
        let _ = notify.send(true);
    });
    let result = axum::serve(
        listener,
        app.router
            .into_make_service_with_connect_info::<std::net::SocketAddr>(),
    )
    .with_graceful_shutdown(async move {
        if !*rx.borrow() {
            let _ = rx.changed().await;
        }
    })
    .await;
    let _ = shutdown_tx.send(true);
    let _ = background.await;
    if let Some(token) = launcher_token {
        let h = hash(token);
        let _ = transact(app.service.repo.as_ref(), |s| {
            s.admin_sessions.remove(&h);
            Ok(())
        })
        .await;
    }
    result?;
    Ok(())
}

async fn wait_for_signal() {
    #[cfg(unix)]
    {
        if let Ok(mut terminate) =
            tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())
        {
            tokio::select! { _ = tokio::signal::ctrl_c() => {}, _ = terminate.recv() => {} }
            return;
        }
    }
    let _ = tokio::signal::ctrl_c().await;
}
