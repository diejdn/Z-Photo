#include <QGuiApplication> 
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <windows.h>

#include <QProcess>
#include <QTemporaryFile>

#include <QMediaPlayer>
#include <QVideoSink>
#include <QEventLoop>
#include <QTimer>
#include <QVideoFrame>
#include <QDirIterator>

#include <vector>
#include <unordered_map>

extern "C"{
	#include "sqlite3.h"
}

namespace fs = std::filesystem;
using namespace std::chrono;

/*
	扫描 图片/视频 元信息入库 + 生成缩略图
	
	需要ffmpeg，也支持纯qt生成缩略图
*/

/*
用法：

ImageIndexer D:\Z_project\data\image D:\Z_project\ D:\Z_project\metadata.db
			 参数：
			 1.要入库的图片所在目录（支持递归）
			 
			 2.rootpath，获得data/这样的相对目录
			 
			 3.D:\Z_project\metadata.db 入库数据库的路径
			 
			 自动处理 data\image 下的 图片/视频，入库+产生缩略图
			 缩略图插件产生在rootpath/thumbnail/ 下


ImageIndexer D:\Z_project\data\video D:\Z_project\ D:\Z_project\media.db

ImageIndexer D:\Z_project\data\image\ D:\Z_project\ D:\Z_project\media.db

增量模式，从数据库中查询某目录当前最大的文件修改时间，仅处理目录中 修改时间 大于最大修改时间的
ImageIndexer D:\Z_project\data\video D:\Z_project\ D:\Z_project\media.db --incremental

*/

// ---------- 路径编码转换 关于 gbk 宽字符 utf8 之间的转换 （未使用，实际使用qt app.arguments() 接受命令行参数，自动转换为utf8）----------
std::wstring gbk_to_wstring(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), -1, &wstr[0], len);
    wstr.pop_back();
    return wstr;
}

std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], len, nullptr, nullptr);
    utf8.pop_back();
    return utf8;
}

std::wstring utf8_to_wstring(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], len);
    wstr.pop_back();
    return wstr;
}

// ---------- 图片格式与缩略图配置 ----------
static const QStringList imageExts = {"jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff"};
static const QStringList videoExts = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm"};

// 缩略图的尺寸
const int THUMB_SIZE = 320;

int getMediaType(const QString& filePath) {
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    if (imageExts.contains(ext)) return 0;    // 图片
    if (videoExts.contains(ext)) return 1;    // 视频
    return -1;  // 不支持
}

// 生成缩略图相对路径
QString buildThumbnailPath(const QString& relPath, int type) {
    const QString prefix = "data/";
    if (!relPath.startsWith(prefix))
        return QString();

    // 统一分隔符为 '/'，方便后续处理
    QString thumb = relPath;
    thumb.replace('\\', '/');

    // 替换前缀 data/ -> thumbnail/
    thumb.replace(0, prefix.size(), "thumbnail/");

    // 定位最后一个 '/'，分离目录和文件名
    int pos = thumb.lastIndexOf('/');
    if (pos != -1) {
        QString dir = thumb.left(pos + 1);      // 包含结尾 '/' QString::left(pos) 下标 pos 之前的字符
        QString fname = thumb.mid(pos + 1);     // 文件名 QString::mid(pos, n) 包含起始下标 pos 处的字符。
        int dot = fname.lastIndexOf('.');
        if (dot != -1) {
            QString name = fname.left(dot);
            QString ext = fname.mid(dot);
            // 根据类型添加不同后缀
            if (type == 0) {          // 图片
                thumb = dir + name + "_ithu" + ext;
            } else if (type == 1) {   // 视频
                thumb = dir + name + "_vthu.jpeg";
            }
            // 如果 type 不是 0/1，保持原文件名（不添加后缀）
        }
        // 如果无扩展名，则保持原样
    }

    // 保险起见，再统一一次分隔符
    thumb.replace('\\', '/');
    return thumb;
}

// ---------- 缩略图生成（Qt） ----------
QImage createSquareThumbnail(const QImage& src) {
    QImage scaled = src.scaled(THUMB_SIZE, THUMB_SIZE,
                               Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    int w = scaled.width(), h = scaled.height();
    int cropSize = qMin(w, h);
    int x = (w - cropSize) / 2;
    int y = (h - cropSize) / 2;
    return scaled.copy(x, y, cropSize, cropSize);
}

bool saveThumbnail(const QImage& thumb, const QString& qPath) {
    QImageWriter writer(qPath);
    if (qPath.endsWith(".jpg", Qt::CaseInsensitive) ||
        qPath.endsWith(".jpeg", Qt::CaseInsensitive) ||
        qPath.endsWith(".webp", Qt::CaseInsensitive)) {
        writer.setQuality(85);
    }
    return writer.write(thumb);
}

// 为图片生成缩略图（如果目标已存在则跳过）纯qt方式
bool generateThumbnailForImage(const QString& fileAbsPath, const QString& thumbAbsPath) {
    QFileInfo thumbInfo(thumbAbsPath);
    if (thumbInfo.exists()) return true;  // 缩略图已存在，跳过

    // 确保缩略图目录存在
    QDir().mkpath(thumbInfo.absolutePath());

    // 读取原图
    QImageReader reader(fileAbsPath);
    QImage src;
    if (!reader.read(&src)) {
        qWarning() << "Error: Cannot decode image:" << fileAbsPath;
        return false;
    }

    QImage thumb = createSquareThumbnail(src);
    if (thumb.isNull()) {
        qWarning() << "Error: Thumbnail generation failed:" << fileAbsPath;
        return false;
    }

    return saveThumbnail(thumb, thumbAbsPath);
}

// 为视频生成缩略图（如果目标已存在则跳过） 纯qt方式
// 从视频文件中提取第一帧（或任意一帧）并保存为正方形缩略图，与图片缩略图的风格保持一致。
bool generateThumbnailForVideo(const QString& fileAbsPath, const QString& thumbAbsPath) {
    QFileInfo thumbInfo(thumbAbsPath);
    if (thumbInfo.exists()) return true; // 缩略图已存在，跳过

	// 确保缩略图目录存在 确保缩略图所在目录存在（如果不存在则递归创建），这是写入文件的前置条件。
    QDir().mkpath(thumbInfo.absolutePath());

	/*
		QMediaPlayer：负责加载和播放视频，但不渲染到屏幕（因为我们不关心显示）。

		QVideoSink：是 Qt 6 中用于接收视频帧的类，
		可以捕获解码后的每一帧图像。
		我们将它设置给播放器，这样当播放器产生视频帧时，sink 就会发出 videoFrameChanged 信号。
	*/
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);

	/*
		QEventLoop：用于阻塞当前函数，直到我们捕获到帧或超时。因为视频加载和解码是异步的，我们需要等待。
		QTimer：设置单次超时（5 秒），防止视频无法解码或损坏时无限等待。
		frameCaptured 和 capturedImage 用于在信号槽中传递捕获状态和图像数据。
	*/
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool frameCaptured = false;
    QImage capturedImage;

	/*
		当视频开始播放后，第一帧会触发 videoFrameChanged，在槽函数中：
			检查帧有效性。
			转换为 QImage（QVideoFrame::toImage() 是 Qt 6 提供的方法，返回一个 QImage 副本）。
			保存图像，标记 frameCaptured = true，并退出事件循环。

		同时，如果 5 秒内没有收到帧，timer 超时也会退出事件循环，防止死锁。

		player.play() 会开始异步解码，此时事件循环开始等待，直到其中一个条件满足。
	*/
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame& frame) {
        if (!frame.isValid()) return;
        // 将 QVideoFrame 转为 QImage
        QImage img = frame.toImage();
        if (img.isNull()) return;
		// 捕获图片 以及 设置捕获成功
        capturedImage = img;
        frameCaptured = true;
        loop.quit(); // 直接关闭loop 不阻塞
		timer.stop(); // 关闭计时器
    });
	
	// 计时器事件到，关闭 loop
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

	// 开始播放视频
    player.setSource(QUrl::fromLocalFile(fileAbsPath));
    player.play();

    timer.start(5000); // 5秒超时
    loop.exec();

    player.stop();

    if (!frameCaptured || capturedImage.isNull()) {
        qWarning() << "Error: Failed to capture video frame from" << fileAbsPath;
        return false;
    }

    QImage thumb = createSquareThumbnail(capturedImage);
    if (thumb.isNull()) {
        qWarning() << "Error: Thumbnail generation failed for video" << fileAbsPath;
        return false;
    }

    return saveThumbnail(thumb, thumbAbsPath);
}

// 使用ffmpeg为视频生成缩略图
bool generateThumbnailForVideo_ff(const QString& fileAbsPath, const QString& thumbAbsPath) {
    QFileInfo thumbInfo(thumbAbsPath);
    if (thumbInfo.exists()) return true; // 已存在，跳过

    // 确保缩略图目录存在
    QDir().mkpath(thumbInfo.absolutePath());

    // 使用 ffmpeg 提取第 1 秒的帧，生成正方形缩略图
    // 命令参数：
    // -ss 00:00:01   : 跳转到第 1 秒
    // -vframes 1     : 只取一帧
    // -vf "scale=320:320:force_original_aspect_ratio=decrease,pad=320:320:(ow-iw)/2:(oh-ih)/2"
    //                : 缩放并居中填充为 320x320 正方形
    // -q:v 2         : JPEG 质量（2 为高质量，对应 Qt 的 85%）
    // -y             : 覆盖已有文件（但我们已经检查过，不会覆盖）
    QStringList args;
    args << "-ss" << "00:00:01"
         << "-i" << fileAbsPath
         << "-vframes" << "1"
         << "-vf" << "scale=320:320:force_original_aspect_ratio=decrease,pad=320:320:(ow-iw)/2:(oh-ih)/2"
         << "-q:v" << "2"
         << "-y"
         << thumbAbsPath;

    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", args);
    if (!ffmpeg.waitForFinished(10000)) { // 10秒超时
        qWarning() << "ffmpeg timeout for" << fileAbsPath;
        ffmpeg.kill();
        return false;
    }
    if (ffmpeg.exitCode() != 0) {
        qWarning() << "ffmpeg error for" << fileAbsPath << ":" << ffmpeg.readAllStandardError();
        return false;
    }

    // 验证缩略图是否真的生成
    return QFileInfo::exists(thumbAbsPath);
}

// ---------- 时间戳转换 文件的修改时间转换为 标准UTC时间（未使用，实际使用QFileInfo实现）----------
long long fileTimeToUnixTimestamp(const fs::file_time_type& ftime) {
#if __cpp_lib_chrono >= 201907
    auto sys_tp = std::chrono::file_clock::to_sys(ftime);
#else
    auto delta = ftime.time_since_epoch() - fs::file_time_type::clock::now().time_since_epoch();
    auto sys_tp = system_clock::now() + delta;
#endif
    auto sec = duration_cast<seconds>(sys_tp.time_since_epoch());
    return sec.count();
}

// ---------- 数据库操作 ----------
void initDatabase(sqlite3* db) {
    // 1. 外键约束（保证数据完整性）
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    
    // 2. WAL 模式（允许读与写并发执行）
    //    你的离线扫描工具在写入时，仍然可以查询浏览，不会被阻塞
    sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    
    // 3. 增大缓存（10 万+ 数据必备）
    //    -20000 表示 20,000 KB（约 20MB）缓存，让索引常驻内存，查询速度毫秒级
    sqlite3_exec(db, "PRAGMA cache_size = -20000;", nullptr, nullptr, nullptr);
}

// 辅助函数：增量模式 获取 source_folder 最大 mtime（针对 media 表）
std::unordered_map<std::string, long long> getMaxMtimeMap(sqlite3* db) {
    std::unordered_map<std::string, long long> map;
    const char* sql = "SELECT source_folder, MAX(mtime) FROM media GROUP BY source_folder;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "Prepare max_mtime query failed:" << sqlite3_errmsg(db);
        return map;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* folder = (const char*)sqlite3_column_text(stmt, 0);
        long long maxMtime = sqlite3_column_int64(stmt, 1);
        if (folder) map[folder] = maxMtime;
    }
    sqlite3_finalize(stmt);
    return map;
}

// 插入 media 记录
bool insertOrUpdateMedia(sqlite3* db, const std::string& filePath, const std::string& name,
                          const std::string& sourceFolder, long long mtime,
                          const std::string& thumbnailPath, int media_type) 
{	
	// 使用ON CONFLICT(file_path) DO UPDATE SET，旧文件被修改，则更新
	const char* sql = "INSERT INTO media (file_path, name, source_folder, mtime, thumbnail_path, media_type) "
                  "VALUES (?, ?, ?, ?, ?, ?) "
                  "ON CONFLICT(file_path) DO UPDATE SET "
                  "name = excluded.name, "
                  "source_folder = excluded.source_folder, "
                  "mtime = excluded.mtime, "
                  "thumbnail_path = excluded.thumbnail_path, "
                  "media_type = excluded.media_type;";
				  
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "Prepare insert failed: " << sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, sourceFolder.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, mtime);
    sqlite3_bind_text(stmt, 5, thumbnailPath.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 6, media_type);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

// 核心方法，递归扫描目标目录，处理其中的 媒体文件 入库 + 缩略图生成
void indexFolder(const QString& baseRelPathQ, const QString& rootPathQ, const QString& dbPath, bool incremental) {
    QDir baseDir(baseRelPathQ);
    if (!baseDir.exists()) {
        qWarning() << "Error: Directory does not exist:" << baseRelPathQ;
        return;
    }

    sqlite3* db;
    if (sqlite3_open(dbPath.toUtf8().constData(), &db) != SQLITE_OK) {
        qWarning() << "Cannot open database:" << sqlite3_errmsg(db);
        return;
    }
    initDatabase(db);

    // 增量模式：获取每个 source_folder 的最大 mtime
    std::unordered_map<std::string, long long> maxMtimeMap;
    if (incremental) {
        maxMtimeMap = getMaxMtimeMap(db);
    }

    // 目录统计信息
    std::unordered_map<std::string, std::pair<int, int>> dirStatMap;

	// 开启事务
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	// 其他统计指标
    int countInsert = 0, countSkip = 0, thumbRelWError = 0, generateThumbError = 0, insertcountError = 0;

    // 使用 QDirIterator 递归遍历
    QDirIterator it(baseRelPathQ,
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
		// 获得文件信息
        QFileInfo fileInfo = it.fileInfo();
		// 文件的绝对路径
        QString fileAbsPath = fileInfo.absoluteFilePath();

        // 判断类型
        int mediaType = getMediaType(fileAbsPath);
        if (mediaType < 0) {
			
			qDebug() << "file format unsupport： " << fileAbsPath ;
			// 不支持的格式
			continue;  
		}

        // 计算相对路径（相对于 rootPathQ）
        QString fileRelPath = QDir(rootPathQ).relativeFilePath(fileAbsPath);
		// 统一分隔符
        fileRelPath.replace('\\', '/');  

        // 提取 source_folder（去除文件名，保留目录）
		/*
			fileRelPath = "data/p1/00001.webp" lastIndexOf 下标从0开始
			lastSlash = 7（指向 00001.webp 前的最后一个 /）
			fileRelPath.left(7) = "data/p1" 末尾不带斜杠 left(7) 7左边的
		*/
        int lastSlash = fileRelPath.lastIndexOf('/');
        QString sourceFolder = (lastSlash != -1) ? fileRelPath.left(lastSlash) : QString();

        // 获取修改时间（Unix 时间戳）fileInfo.lastModified().toSecsSinceEpoch() 返回的是 UTC 秒级整数时间戳
        qint64 mtime = fileInfo.lastModified().toSecsSinceEpoch();

        // 增量模式：检查是否需要处理
        if (incremental) {
            std::string folderUTF8 = sourceFolder.toUtf8().constData();
            auto itMap = maxMtimeMap.find(folderUTF8);
			// 找到（ itMap != maxMtimeMap.end()） 就赋值itMap->second，没找到就为0
            long long maxMtime = (itMap != maxMtimeMap.end()) ? itMap->second : 0;
            if (mtime <= maxMtime) {
                countSkip++;
                continue;
            }
        }

        // 生成缩略图相对路径
        QString thumbRelPath = buildThumbnailPath(fileRelPath, mediaType);
        if (thumbRelPath.isEmpty()) {
            qWarning() << "Cannot build thumbnail path for:" << fileRelPath;
            thumbRelWError++;
            continue;
        }

        // 生成缩略图绝对路径
        QString thumbAbsPath = QDir(rootPathQ).absoluteFilePath(thumbRelPath);
        bool thumbOk;
		//	生成缩略图，图片和视频利用不同方法
        if (mediaType == 0) {
            thumbOk = generateThumbnailForImage(fileAbsPath, thumbAbsPath);
        } else { // mediaType == 1  _ff生成的视频缩略图周围有黑边（播放器中的比例） 普通版没有
            thumbOk = generateThumbnailForVideo_ff(fileAbsPath, thumbAbsPath);
        }
        if (!thumbOk) {
            generateThumbError++;
            // 缩略图生成失败，仍继续入库（可根据需求决定）
        }

        // 入库（使用 UPSERT），构造表中各字段
        std::string fileRelUTF8 = fileRelPath.toUtf8().constData();
        std::string nameUTF8 = fileInfo.fileName().toUtf8().constData();
        std::string folderUTF8 = sourceFolder.toUtf8().constData();
        std::string thumbRelUTF8 = thumbRelPath.toUtf8().constData();

        if (insertOrUpdateMedia(db, fileRelUTF8, nameUTF8, folderUTF8, mtime, thumbRelUTF8, mediaType)) {
            countInsert++;
            // 记录目录统计
            dirStatMap[folderUTF8].first++;
            if (thumbOk) dirStatMap[folderUTF8].second++;
        } else {
            qWarning() << "Failed to insert:" << fileRelPath;
            insertcountError++;
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);

    // 输出统计
    qDebug() << "\nScan complete!";
    qDebug() << "Inserted/Updated: " << countInsert;
    qDebug() << "Skipped (unchanged): " << countSkip;
    qDebug() << "thumbRelWError: " << thumbRelWError;
    qDebug() << "generateThumbError: " << generateThumbError;
    qDebug() << "insertcountErrors: " << insertcountError;

    std::cout << "\n===== 各目录入库&缩略图统计 =====" << std::endl;
    for (const auto& entry : dirStatMap) {
        std::string folder = entry.first;
        int insertCnt = entry.second.first;
        int thumbCnt = entry.second.second;
        std::cout << "[" << folder << "] 入库成功:" << insertCnt << " 缩略图成功:" << thumbCnt << std::endl;
    }
    std::cout << "==================================";
}

int main(int argc, char* argv[]) {
	SetConsoleOutputCP(CP_UTF8);
	
    // GUI 事件循环 QMediaPlayer 需要 GUI 事件循环 才能正常工作。
	QGuiApplication app(argc, argv);

    QStringList args = app.arguments();
    if (args.size() < 3) {   // 至少需要 扫描目录 和 rootpath
        qDebug() << "==== Media Indexer (with thumbnail generation) ====";
        qDebug() << "Usage: Indexer.exe <scan_directory> <root_path> [database_path] [--incremental]";
        qDebug() << "  <scan_directory>  directory to scan (e.g., data/)";
        qDebug() << "  <root_path>       root path for relative paths (e.g., D:/Z_project/)";
        qDebug() << "  [database_path]   optional, default: metadata.db";
        qDebug() << "  [--incremental]   optional, only process newer files";
        qDebug() << "Example: Indexer.exe data/ D:/Z_project/ mydb.db --incremental";
        return 1;
    }

    QString scanInput = args[1];
    QString rootPath = args[2];
    QString dbPath = (args.size() >= 4) ? args[3] : "metadata.db";
    bool incremental = false;
    if (args.size() >= 5 && args[4] == "--incremental") {
        incremental = true;
    }

    indexFolder(scanInput, rootPath, dbPath, incremental);
    return 0;
}

