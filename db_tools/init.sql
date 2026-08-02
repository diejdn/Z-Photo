PRAGMA foreign_keys = ON;

-- 1. 开启 WAL 模式（提高并发性能，推荐）
PRAGMA journal_mode = WAL;
-- 2. 增大缓存（10 万+ 数据必备）
PRAGMA cache_size = -20000;  -- 20MB 缓存

CREATE TABLE IF NOT EXISTS media (
    media_id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    source_folder TEXT NOT NULL,
    mtime INTEGER NOT NULL,
    thumbnail_path TEXT,
	media_type INTEGER NOT NULL DEFAULT 0   -- 0: 图片, 1: 视频
);

CREATE TABLE IF NOT EXISTS tag (
    tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
    tag_name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS media_tag_rel (
    media_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (media_id, tag_id),
    FOREIGN KEY (media_id) REFERENCES media(media_id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tag(tag_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_media_source_folder ON media(source_folder);
CREATE INDEX IF NOT EXISTS idx_media_mtime ON media(mtime);
CREATE INDEX IF NOT EXISTS idx_media_type ON media(media_type);
CREATE INDEX IF NOT EXISTS idx_rel_media_id ON media_tag_rel(media_id);
CREATE INDEX IF NOT EXISTS idx_rel_tag_id ON media_tag_rel(tag_id);
