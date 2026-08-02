---

# Z-Photo

[![Qt](https://img.shields.io/badge/Qt-6.7.3-brightgreen)](https://www.qt.io/)
[![MinGW](https://img.shields.io/badge/MinGW-11.2.0-blue)](https://www.mingw-w64.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Z-Photo** 是一个基于 Qt6 和 SQLite 的跨平台媒体管理器，支持图片与视频的统一管理、标签系统、增量索引，并提供本地与 IPv6 网络两种访问模式。它专为个人媒体库设计，即使面对 **10 万+** 文件也能保持流畅的浏览体验。

---

## ✨ 核心特性

- 🖼️ **媒体混排** – 图片和视频在同一个网格视图中展示，按需筛选。
- 🏷️ **标签系统** – 为媒体文件添加/删除标签，支持多标签联合查询。
- ⚡ **懒加载** – 仅加载可视区域内的缩略图（含 400px 缓冲），内存占用恒定。
- 🌐 **双模式** – 支持本地文件系统直接访问，也支持通过自定义 HTTP 服务器进行 IPv6 网络流式播放。
- 🎬 **视频优化** – 配合 FFmpeg 自动转码为 H.264 + faststart，保证网络播放流畅。
- ⌨️ **命令行预览** – 支持 `z-photo.exe <文件路径>` 快速单文件预览，可集成到系统右键菜单。

---

## 🛠️ 构建方法

### 环境要求

- Qt 6.7.3 (MinGW 11.2.0)
- CMake 3.16+
- MinGW 11.2.0（或兼容版本）
- SQLite3（已集成在源码中）

### Windows 构建步骤

```bash
# 克隆或进入项目目录
cd Z-Photo

# 创建构建目录
mkdir build
cd build

# 生成 MinGW Makefile
cmake -G "MinGW Makefiles" ..

# 编译
mingw32-make

# 部署 Qt 依赖（将生成的 Z-Photo.exe 与所需 DLL 放在同一目录）
# 请将下面的路径替换为你自己的 Qt 安装路径
D:\Qt\6.7.3\mingw_64\bin\windeployqt.exe Z-Photo.exe
```

> **注意**：请将 `D:\Qt\6.7.3\mingw_64\` 替换为你自己 Qt 的安装路径。

### Linux / macOS 构建

```bash
mkdir build && cd build
cmake ..
make
```

---

## 📖 使用方法

### GUI 模式

直接运行 `Z-Photo.exe`，启动对话框会提示你选择：

- **本地模式**：指定数据库文件（`media.db`）和媒体根目录（`data/` 所在路径）。
- **网络模式**：输入服务器 IP 和端口（见下文网络模式说明）。

进入主界面后：

- 左侧面板显示所有目录，点击即可在右侧网格中加载对应媒体。
- 顶部工具栏可筛选类型（全部/图片/视频）和排序（时间/文件名）。
- 下方筛选栏支持：
  - **所有目录**：忽略当前目录，跨所有目录查询。
  - **限制数量**：随机抽取指定数量的媒体（适合快速浏览）。
  - **时间范围**：按修改日期区间筛选。
  - **标签查询**：弹出标签选择窗口，返回含指定标签的所有媒体。
- 点击任意缩略图，可在独立预览窗口中查看大图或播放视频，并支持左右键切换。

### 命令行模式

```bash
# 直接预览单个媒体文件
Z-Photo.exe "D:/path/to/photo.jpg"
Z-Photo.exe "D:/path/to/video.mp4"
> **💡 提示**：你也可以在图片或视频文件上**右键 → 打开方式 → 选择其他应用 → 浏览到 `Z-Photo.exe`**，即可快速预览。

# 显示帮助信息
Z-Photo.exe --help
```

---

## ⚡ 懒加载机制（性能核心）

Z-Photo 通过**可视区域感知**实现高效的缩略图管理，确保即使有 10 万+ 文件，内存占用也能保持极低。

该机制在主窗口（`z-photo.cpp`）中统一驱动，通过 `updateVisibleImages` 函数调度，与 `ImageGridWidget` 配合完成。

### 工作原理

1. **计算可视区域**：主窗口监听 `QScrollArea` 的滚动条，获取当前视口（viewport）在滚动内容中的位置矩形。
2. **扩展缓冲区**：在可视区域上下各扩展 **400 像素**，形成“加载缓冲区”。这样当用户滚动时，即将进入视野的图片会提前加载，消除白屏等待。
3. **遍历网格标签**：主窗口遍历 `ImageGridWidget` 中的 `QList<MediaLabel*>`，检查每个标签的 `geometry()` 是否与缓冲区矩形相交。
4. **动态加载/卸载**：
   - 相交 → 若未加载，则调用 `loadImage()` 加载缩略图；
   - 不相交 → 若已加载，则调用 `unloadImage()` 释放图片内存（`QPixmap` 置空）。
5. **防抖优化**：
   - 滚动事件使用 300ms 单次定时器，避免快速滚动时频繁计算。
   - 窗口尺寸变化（resize/分割器拖动）使用 100ms 定时器，并强制重绘已加载的图片以适应新尺寸。

### 关键代码片段

```cpp
// 主窗口中的 updateVisibleImages lambda
auto updateVisibleImages = [&scrollArea, &mediaList](bool forceReload) {
    QRect viewRect = scrollArea->viewport()->rect();
    viewRect.translate(0, scrollArea->verticalScrollBar()->value());
    viewRect.adjust(0, -400, 0, 400);

    for (MediaLabel* lab : mediaList) {
        if (lab->geometry().intersects(viewRect)) {
            if (!lab->isLoaded()) lab->loadImage(mode);
            else if (forceReload) lab->update();
        } else {
            if (lab->isLoaded()) lab->unloadImage();
        }
    }
};

---

## 🌐 本地与网络双模式

### 本地模式

- 直接访问操作系统文件系统，媒体路径由 `data_root`（用户指定）与数据库中的相对路径拼接而成。
- 缩略图从本地磁盘加载，速度极快。
- 适合个人电脑、移动硬盘等场景。

### 网络模式

- 服务器读取配置文件中的根目录，为客户端提供媒体文件流。
- **IPv6 支持**：服务器监听 `::`（所有 IPv6 地址），客户端可通过 IPv6 直连，无需 NAT 穿透。
- **断点续传**：服务器正确实现了 HTTP `Range` 请求（`206 Partial Content`），支持视频拖拽播放。
- **视频优化**：推荐配合 FFmpeg 将视频转码为 H.264 + `faststart`（`moov` 原子移至文件头），以获得最佳网络播放体验。

### 网络模式配置

1. 在服务器端创建 `server.ini`：
   ```ini
   [Server]
   rootDir=D:/your/media/root/
   ```
2. 运行服务器：
   ```bash
   MediaHttpServer.exe
   ```
3. 客户端启动时选择“网络模式”，输入服务器 IPv6 地址和端口（默认 8080）。

---

## 📂 项目结构

```
Z-Photo/
├── AddTagWindow/       # 标签选择与管理窗口
├── AppConfig/          # 全局配置管理（单例）
├── DateTimeUtils/      # UTC/Local 时间戳转换工具
├── ImageGridWidget/    # 网格视图 + 与媒体管理器连接
├── ImagePlayer/        # 图片预览窗口（支持缩放/拖拽/全屏）
├── MediaLabel/         # 缩略图控件（支持图片/视频）
├── VideoPlayer/        # 视频播放器（进度/音量/全屏/循环）
├── ZLens/              # 统一媒体预览入口（自动切换图片/视频播放器）
├── limitRandom/        # Fisher-Yates 随机抽样算法
├── sqliteApi/          # SQLite 封装（事务、索引、多标签查询）
├── style/              # QSS 样式表文件
├── icon/               # 应用程序图标资源
├── resources.rc        # Windows 资源文件（定义程序图标）
├── style.qrc           # Qt 资源文件（编译时嵌入 QSS）
├── z-photo.cpp         # 主程序入口（GUI + 命令行双模式 + 懒加载逻辑）
└── CMakeLists.txt      # CMake 构建脚本
```

### 核心模块说明

| 模块 | 职责 |
| :--- | :--- |
| `ZLens` | 统一预览入口，根据 `media_type` 自动创建 `ImagePlayer` 或 `VideoPlayer`，处理窗口切换与信号转发 |
| `ImagePlayer` | 图片预览窗口，基于 `QGraphicsView` 实现缩放、拖拽、全屏、键盘快捷键（左右切换/T 添加标签） |
| `VideoPlayer` | 视频播放器，基于 `QMediaPlayer`，支持进度拖拽、音量调节、循环播放、1.5x 长按加速 |
| `ImageGridWidget` | 媒体缩略图的网格布局容器，提供 `mediaList` 和 `layout()` 供主窗口的懒加载逻辑驱动，以及响应用户点击并触发 ZLens 预览窗口|
| `sqliteApi` | SQLite 封装，支持 WAL 模式、事务、UPSERT、多标签联合查询与排序 |
```

---

## 🖥️ 媒体预览操作指南

Z-Photo 内置了功能完整的图片查看器和视频播放器，支持键盘快捷键与鼠标交互。

### 图片查看器

图片预览窗口基于 `QGraphicsView` 实现，支持平滑缩放、拖拽和全屏浏览。

| 操作 | 按键 / 操作 |
| :--- | :--- |
| 切换全屏 / 退出全屏 | `F11` |
| 放大图片 | 鼠标滚轮向上滚动 |
| 缩小图片 | 鼠标滚轮向下滚动 |
| 拖拽图片 | 鼠标左键按住并拖动 |
| 打开标签弹窗 | `T` |
| 下一张图片 | `D` 或 `→`（右方向键） |
| 上一张图片 | `A` 或 `←`（左方向键） |

---

### 视频播放器

视频播放器基于 `QMediaPlayer`，支持进度拖拽、倍速播放和完整的音量控制。

| 操作 | 按键 / 操作 |
| :--- | :--- |
| 切换全屏 / 退出全屏 | `F11` |
| 快进 5 秒 | `R` 或 `→`（右方向键） |
| 后退 5 秒 | `L` 或 `←`（左方向键） |
| 1.5 倍速播放（长按生效） | 长按 `R` 或 长按 `→`（右方向键） |
| 切换播放 / 暂停 | `Space`（空格键）或点击暂停按钮 |
| 静音 / 取消静音 | `M` |
| 增大音量 | `↑`（上方向键） |
| 减小音量 | `↓`（下方向键） |
| 循环 / 单次模式切换 | `E`（默认循环播放） |
| 打开标签弹窗 | `T` |
| 下一个视频 | `D` |
| 上一个视频 | `A` |

**进度条操作**：
- 拖拽进度条上方的滑块（圆形手柄）即可跳转播放位置。
- 提示：**请拖拽滑块（圆形手柄）进行进度跳转，暂不支持点击进度条直接跳转。**

---

### 统一操作

以下操作在图片查看器和视频播放器中**通用**：

| 操作 | 按键 |
| :--- | :--- |
| 打开标签弹窗 | `T` |
| 下一张媒体 | `D` |
| 上一张媒体 | `A` |

---

## 📦 构建本地媒体库

### 0. 初始化数据库

在使用索引工具之前，你需要先创建数据库结构。`initdb.exe` 会建立 `media`、`tag` 和 `media_tag_rel` 三张表，并自动启用外键约束、WAL 模式和 20MB 缓存。

**用法**：
```cmd
initdb.exe [数据库文件名]
```

- 若未指定文件名，默认在当前目录生成 `metadata.db`。
- 若指定文件名（如 `initdb.exe mylib.db`），则创建 `mylib.db`。

**示例**：
```cmd
# 生成默认数据库
initdb.exe

# 生成自定义名称的数据库
initdb.exe D:\Z_project\media.db
```

执行后，控制台会输出 `Database schema created successfully.`，表示数据库已准备就绪。

---

Z-Photo 提供了两个配套工具，帮助你从零开始构建索引库并优化视频格式。

### 1. 视频预处理（可选但推荐）

对于网络播放，视频文件需要满足两个条件：**编码为 H.264** 且 **`moov` 原子位于文件头部**（`faststart`）。Z-Photo 提供了一个批处理脚本 `video_format.bat`，可以批量处理视频文件。
对于本地播放，视频可以不满足**`moov` 原子位于文件头部**，但还是推荐进行预处理。

**用法**：
```cmd
video_trans_pro.bat <源目录>
```

例如：
```cmd
video_trans_pro.bat data\video_source\TwDown
```

**脚本行为**：
- 扫描源目录中的所有常见视频格式（`.mp4`, `.mov`, `.mkv`, `.avi`, `.flv`, `.webm`）。
- 若视频已是 H.264，则仅复制流并移动 `moov`（极快）。
- 若非 H.264，优先尝试 NVIDIA 硬件加速转码（`h264_nvenc`），失败则回退到 CPU 软编（`libx264`）。
- 转换后的文件输出到 `data\video\<子目录>`，保留原文件名。
- 源文件备份为 `原文件名_src.扩展名`，避免重复转换。

**⚠️ 注意**：脚本硬编码了目标根目录为 `data\video`。如果你希望修改，请编辑脚本中的 `TARGET_ROOT` 变量。

---

### 2. 元数据索引与缩略图生成

`Indexer.exe` 是负责扫描媒体文件、提取元数据、生成缩略图并写入 SQLite 数据库的核心工具。

**基本用法**：
```cmd
Indexer.exe <扫描目录> <根目录> [数据库路径] [--incremental]
```

| 参数 | 说明 |
| :--- | :--- |
| `<扫描目录>` | 要扫描的目录（支持递归），例如 `data/image` 或 `data/video`。 |
| `<根目录>` | 媒体库的根路径，用于计算相对路径，例如 `D:/Z_project/`。 |
| `[数据库路径]` | 可选，默认为 `metadata.db`。 |
| `[--incremental]` | 可选，启用增量模式（见下文）。 |

**示例**：
```cmd
# 全量扫描图片目录
Indexer.exe D:\Z_project\data\image D:\Z_project\ D:\Z_project\media.db

# 增量扫描视频目录
Indexer.exe D:\Z_project\data\video D:\Z_project\ D:\Z_project\media.db --incremental
```

**处理内容**：
- 识别图片（`.jpg`, `.png`, `.webp` 等）和视频（`.mp4`, `.mov`, `.mkv` 等）。
- 为图片生成 `_ithu` 缩略图（保持原扩展名）。
- 为视频生成 `_vthu.jpeg` 缩略图（使用 FFmpeg 抽取第 1 秒帧）。
- 将文件路径、名称、所在目录、修改时间（UTC 秒数）、缩略图路径、媒体类型写入 `media` 表。
- 若文件已存在，自动更新（`ON CONFLICT DO UPDATE`），保留标签关联。

> **⚠️ 注意**：尽管 `Indexer.exe` 具备递归扫描能力，但为了更清晰地掌握数据处理进度并简化故障排查流程，**建议对各个目录分别执行索引操作**，而非一次性扫描整个媒体根目录。

---

### 3. 增量模式（`--incremental`）

增量模式通过比较文件修改时间（`mtime`）与数据库中对应目录的最大 `mtime`，**仅处理比基准时间更晚的文件**，大幅节省重复扫描的时间。

**适用场景**：
- 你向现有目录中添加了新文件。
- 你替换了某个文件（修改时间会更新，可被识别）。

**⚠️ 适用范围说明**：

| 文件类型 | 是否受增量逻辑影响 | 说明 |
| :--- | :--- | :--- |
| **视频** | ✅ 正常工作 | 预处理（转码）发生在索引之前，转码后的文件修改时间通常为当前时间，因此会被增量模式正确处理。 |
| **图片（直接下载）** | ✅ 正常工作 | 下载的图片保留其原始修改时间，若该时间晚于数据库中的基准，则会被索引。 |
| **图片（从压缩包解压）** | ⚠️ 可能被漏掉 | 解压操作往往会将文件的修改时间设置为压缩包内的原始时间（通常较早）。如果该时间早于数据库中该目录的最大 `mtime`，增量模式会认为“文件没有更新”而跳过索引。 |

**解决建议**：
- 对于从压缩包解压的图片，建议**首次使用全量扫描**（不加 `--incremental`），后续新增文件使用增量模式。
- 或者解压后手动用工具（如 `touch`）更新文件修改时间至当前时间，再执行增量扫描。

---

## 🧩 典型工作流

1. **初始化数据库** → 运行 `initdb.exe` 创建 `media.db`。
2. **下载/收集媒体** → 放入 `data/video_source` 或 `data/image`（按需）。
3. **预处理视频** → 运行 `video_trans_pro.bat` 将视频转为 H.264 + faststart，自动放入 `data/video`。
4. **全量索引（首次）** → 运行 `Indexer.exe` 扫描 `data/image` 和 `data/video`，生成缩略图和数据库记录。
5. **增量索引（后续）** → 使用 `--incremental` 定期扫描，只处理新增或修改的文件。

这样，你的媒体库始终保持最新的索引，且视频已针对网络播放优化，缩略图随用随生成，内存和存储开销都保持在合理范围内。

---

## 🌐 网络模式系统

Z-Photo 不仅支持本地媒体库管理，还提供了一套**轻量级的网络模式**，让你可以通过 IPv4/IPv6 远程访问媒体文件，实现类似个人云盘的功能。

### 架构概览

网络模式采用 **C/S（客户端/服务器）架构**：

- **服务端**：`MediaHttpServer.exe` 负责提供媒体文件流。
- **客户端**：Z-Photo 主程序在启动时选择“网络模式”，通过 HTTP 协议从服务端拉取媒体文件。

```
┌─────────────┐     HTTP / IPv6     ┌─────────────┐
│   Z-Photo   │ ◄────────────────── │   Server    │
│  (Client)   │     Range 请求      │ (MediaHttp  │
│             │ ──────────────────► │   Server)   │
└─────────────┘    请求分片/缩略图   └─────────────┘
```

---

### 1. 服务端部署

#### 1.1 配置文件

服务端从 `server.ini` 读取根目录配置，该文件应与服务端可执行文件放在同一目录。

**`server.ini` 示例**：
```ini
; 媒体文件根目录配置
[Server]
rootDir=D:/Z_project/
```

- `rootDir`：媒体库的根目录，服务端会将请求路径拼接在此目录下。
- 注释使用 `;` 或 `#` 开头，并**必须独占一行**（`QSettings` 解析规则）。

#### 1.2 启动服务端

```cmd
MediaHttpServer.exe
```

启动成功后，控制台会输出：
```
Using rootDir: D:/Z_project/
Server listen on 0.0.0.0:8080
```

服务端默认监听 **8080 端口**，支持 **IPv4 和 IPv6** 双栈（`QHostAddress::Any`）。

#### 1.3 路由规则

服务端提供两个路由：

| 路由 | 用途 | 示例 |
| :--- | :--- | :--- |
| `/images/` | 图片文件请求 | `GET /images/data/image/001.webp` → `D:/Z_project/data/image/001.webp` |
| `/videos/` | 视频文件请求 | `GET /videos/data/video/movie.mp4` → `D:/Z_project/data/video/movie.mp4` |

> **注意**：`relativePath` 是从请求 URL 中截取的相对路径。例如请求 `/images/data/image/001.webp`，服务端会拼接为 `rootDir + "data/image/001.webp"`。

#### 1.4 关键特性

- **断点续传**：正确实现 HTTP `Range` 请求，支持 `206 Partial Content`，视频播放器可以拖拽进度条。
- **流式发送**：每次发送 64KB 分块，避免大文件一次性加载到内存。
- **路径保护**：防止目录穿越攻击，只允许访问 `rootDir` 下的文件。

---

### 2. 客户端配置

在 Z-Photo 主程序启动时，选择 **“网络模式”**，并填写：

- **服务器 IP**：支持 IPv4（如 `192.168.1.100`）和 IPv6（如 `[240e:...]`）。
- **端口**：默认为 `8080`。
- **数据库路径**：需要从远端获取数据库文件（见下文）。

---

### 3. 获取远端数据库文件

在首次使用网络模式前，你需要将数据库文件从服务器端下载到本地。Z-Photo 提供了 `fetch_db.exe` 工具来完成此操作。

**用法**：
```cmd
fetch_db.exe
```

交互式输入：
```
==== 获取远端数据库文件 ====
远端服务器 IP（如 127.0.0.1 或 [240e:...]）: 192.168.1.100
端口（如 8080）: 8080
远端数据库路径（如 /images/media.db）: /images/media.db
```

工具会向服务端发送 GET 请求，将响应体保存为当前目录下的同名文件（如 `media.db`）。

**手动方式**（使用 curl）：
```cmd
curl -o media.db http://192.168.1.100:8080/images/media.db
```

---

### 4. 完整工作流（网络模式）

1. **服务端准备**：
   - 在服务器上放置媒体文件（如 `D:/Z_project/data/`）。
   - 创建 `server.ini`，指定 `rootDir`。
   - 启动 `MediaHttpServer.exe`。

2. **客户端准备**：
   - 运行 `fetch_db.exe`，从服务端下载 `media.db`。
   - 启动 Z-Photo，选择“网络模式”，填入服务器 IP 和端口。

3. **浏览媒体**：
   - 客户端将从服务端请求缩略图和媒体文件，体验与本地模式基本一致。
   - 视频播放支持拖拽进度条（得益于服务端的 `Range` 请求支持）。

---

### 5. 网络模式的优势

- **跨设备共享**：在家庭局域网或公网（IPv6）中，多台设备可共享同一个媒体库。
- **节省本地空间**：媒体文件集中存储在服务器上，客户端无需重复存储。
- **即开即用**：配合 `fetch_db.exe`，客户端只需下载轻量级数据库即可开始浏览。
- **安全可控**：服务端仅提供文件流，不涉及用户认证（可通过防火墙或 VPN 加固）。

---

### 6. 高级配置（可选）

如果你希望修改服务端端口，可以修改 `main.cpp` 中的 `server.listen(QHostAddress::Any, 8080)`，或将其配置化（如也通过 `server.ini` 读取）。

---

## 📜 许可证

本项目采用 MIT 许可证，详情请见 [LICENSE](LICENSE) 文件。

---

## 🙏 致谢

- [Qt](https://www.qt.io/) – 跨平台 GUI 框架
- [SQLite](https://www.sqlite.org/) – 轻量级嵌入式数据库
- [FFmpeg](https://ffmpeg.org/) – 强大的多媒体处理工具
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) – 轻量级 C++ HTTP 客户端/服务端库（用于 `fetch_db.exe` 工具）
- [MinGW](https://www.mingw-w64.org/) – Windows 下的 GCC 编译工具链
---