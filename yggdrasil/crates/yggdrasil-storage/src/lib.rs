//! Portable SeaORM persistence. Revisioned aggregate commits make token rotation,
//! identity changes and configuration updates atomic on all three SQL backends.
use async_trait::async_trait;
use sea_orm::{
    entity::prelude::*, ActiveValue::Set, ConnectionTrait, Database, DatabaseConnection,
    QueryFilter, Schema,
};
use std::path::{Path, PathBuf};
use yggdrasil_core::{hash, id, BlobStore, Error, Repository, Result, State};

mod aggregate {
    use sea_orm::entity::prelude::*;
    #[derive(Clone, Debug, PartialEq, DeriveEntityModel)]
    #[sea_orm(table_name = "yggdrasil_state_v1")]
    pub struct Model {
        #[sea_orm(primary_key, auto_increment = false)]
        pub id: i32,
        pub revision: i64,
        #[sea_orm(column_type = "Text")]
        pub payload: String,
    }
    #[derive(Copy, Clone, Debug, EnumIter, DeriveRelation)]
    pub enum Relation {}
    impl ActiveModelBehavior for ActiveModel {}
}

mod migration {
    use sea_orm::entity::prelude::*;
    #[derive(Clone, Debug, PartialEq, DeriveEntityModel)]
    #[sea_orm(table_name = "yggdrasil_migrations")]
    pub struct Model {
        #[sea_orm(primary_key, auto_increment = false)]
        pub version: i32,
    }
    #[derive(Copy, Clone, Debug, EnumIter, DeriveRelation)]
    pub enum Relation {}
    impl ActiveModelBehavior for ActiveModel {}
}

pub struct SqlRepository {
    db: DatabaseConnection,
}
impl SqlRepository {
    pub async fn connect(url: &str) -> Result<Self> {
        let mut options = sea_orm::ConnectOptions::new(url);
        options.sqlx_logging(false);
        // A single connection is required for sqlite::memory: and avoids write-lock churn.
        if url.starts_with("sqlite:") {
            options.max_connections(1);
        }
        let db = Database::connect(options)
            .await
            .map_err(|_| Error::Internal)?;
        let backend = db.get_database_backend();
        let schema = Schema::new(backend);
        let mut migrations = schema.create_table_from_entity(migration::Entity);
        migrations.if_not_exists();
        db.execute(backend.build(&migrations))
            .await
            .map_err(|_| Error::Internal)?;
        if migration::Entity::find()
            .all(&db)
            .await
            .map_err(|_| Error::Internal)?
            .iter()
            .any(|m| m.version > 1)
        {
            return Err(Error::Invalid(
                "The database was created by a newer service version.".into(),
            ));
        }
        // Migration 1: identity/configuration aggregate plus optimistic revision.
        // DDL is idempotent, including after a crash before recording the version.
        let model = schema.create_table_from_entity(aggregate::Entity);
        let mut stmt = sea_orm::sea_query::Table::create();
        stmt.table(aggregate::Entity).if_not_exists();
        for column in model.get_columns() {
            let mut column = column.clone();
            if backend == sea_orm::DbBackend::MySql && column.get_column_name() == "payload" {
                column.custom("LONGTEXT");
            }
            stmt.col(&mut column);
        }
        db.execute(backend.build(&stmt))
            .await
            .map_err(|_| Error::Internal)?;
        aggregate::Entity::insert(aggregate::ActiveModel {
            id: Set(1),
            revision: Set(0),
            payload: Set(serde_json::to_string(&State::default()).map_err(|_| Error::Internal)?),
        })
        .on_conflict(
            sea_orm::sea_query::OnConflict::column(aggregate::Column::Id)
                .do_nothing()
                .to_owned(),
        )
        .do_nothing()
        .exec(&db)
        .await
        .map_err(|_| Error::Internal)?;
        migration::Entity::insert(migration::ActiveModel { version: Set(1) })
            .on_conflict(
                sea_orm::sea_query::OnConflict::column(migration::Column::Version)
                    .do_nothing()
                    .to_owned(),
            )
            .do_nothing()
            .exec(&db)
            .await
            .map_err(|_| Error::Internal)?;
        Ok(Self { db })
    }
}
#[async_trait]
impl Repository for SqlRepository {
    async fn load(&self) -> Result<(i64, State)> {
        let row = aggregate::Entity::find_by_id(1)
            .one(&self.db)
            .await
            .map_err(|_| Error::Internal)?
            .ok_or(Error::Internal)?;
        Ok((
            row.revision,
            serde_json::from_str(&row.payload).map_err(|_| Error::Internal)?,
        ))
    }
    async fn compare_exchange(&self, revision: i64, state: &State) -> Result<bool> {
        let result = aggregate::Entity::update_many()
            .col_expr(
                aggregate::Column::Revision,
                sea_orm::sea_query::Expr::value(revision + 1),
            )
            .col_expr(
                aggregate::Column::Payload,
                sea_orm::sea_query::Expr::value(
                    serde_json::to_string(state).map_err(|_| Error::Internal)?,
                ),
            )
            .filter(aggregate::Column::Id.eq(1))
            .filter(aggregate::Column::Revision.eq(revision))
            .exec(&self.db)
            .await
            .map_err(|_| Error::Internal)?;
        Ok(result.rows_affected == 1)
    }
}

pub struct DiskBlobs {
    root: PathBuf,
}
impl DiskBlobs {
    pub async fn new(root: impl AsRef<Path>) -> Result<Self> {
        tokio::fs::create_dir_all(&root)
            .await
            .map_err(|_| Error::Internal)?;
        Ok(Self {
            root: root.as_ref().to_owned(),
        })
    }
    /// Remove only unreferenced cache objects older than the grace period. A
    /// grace period protects uploads still between blob publication and commit.
    pub async fn collect(&self, live: &std::collections::BTreeSet<String>) -> Result<usize> {
        let mut entries = tokio::fs::read_dir(&self.root)
            .await
            .map_err(|_| Error::Internal)?;
        let mut count = 0;
        while let Some(entry) = entries.next_entry().await.map_err(|_| Error::Internal)? {
            let name = entry.file_name().to_string_lossy().into_owned();
            if live.contains(&name) {
                continue;
            }
            let metadata = entry.metadata().await.map_err(|_| Error::Internal)?;
            let old = metadata
                .modified()
                .ok()
                .and_then(|t| t.elapsed().ok())
                .is_some_and(|age| age.as_secs() > 3600);
            if metadata.is_file() && old && (name.len() == 64 || name.ends_with(".tmp")) {
                tokio::fs::remove_file(entry.path())
                    .await
                    .map_err(|_| Error::Internal)?;
                count += 1;
            }
        }
        Ok(count)
    }
}
#[async_trait]
impl BlobStore for DiskBlobs {
    async fn put(&self, bytes: &[u8]) -> Result<String> {
        use tokio::io::AsyncWriteExt;
        let digest = hash(bytes);
        let destination = self.root.join(&digest);
        if tokio::fs::try_exists(&destination).await.unwrap_or(false) {
            return Ok(digest);
        }
        let temp = self.root.join(format!(".{}.tmp", id()));
        let mut file = tokio::fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temp)
            .await
            .map_err(|_| Error::Internal)?;
        file.write_all(bytes).await.map_err(|_| Error::Internal)?;
        file.sync_all().await.map_err(|_| Error::Internal)?;
        drop(file);
        if tokio::fs::rename(&temp, &destination).await.is_err() {
            let _ = tokio::fs::remove_file(&temp).await;
            if !tokio::fs::try_exists(&destination).await.unwrap_or(false) {
                return Err(Error::Internal);
            }
        }
        Ok(digest)
    }
    async fn get(&self, digest: &str) -> Result<Vec<u8>> {
        if digest.len() != 64 || !digest.bytes().all(|c| c.is_ascii_hexdigit()) {
            return Err(Error::NotFound);
        }
        tokio::fs::read(self.root.join(digest))
            .await
            .map_err(|_| Error::NotFound)
    }
}
