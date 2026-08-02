# Z-Photo

[![Qt](https://img.shields.io/badge/Qt-6.7.3-brightgreen)](https://www.qt.io/)
[![MinGW](https://img.shields.io/badge/MinGW-11.2.0-blue)](https://www.mingw-w64.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Z-Photo** is a cross-platform media manager built with Qt6 and SQLite. It supports unified management of images and videos, a tagging system, incremental indexing, and both local and IPv6 network access modes. Designed for personal media libraries, it maintains smooth browsing even with **100,000+** files.

---

## ✨ Key Features

- 🖼️ **Mixed Media** – Images and videos are displayed together in a single grid view, filterable on demand.
- 🏷️ **Tag System** – Add or remove tags on media files, with support for multi-tag combined queries.
- ⚡ **Lazy Loading** – Only loads thumbnails within the visible area (with a 400px buffer), keeping memory usage constant.
- 🌐 **Dual Mode** – Supports both local file system access and IPv6 streaming via a custom HTTP server.
- 🎬 **Video Optimization** – Works with FFmpeg to automatically transcode videos to H.264 + faststart for smooth streaming.
- ⌨️ **CLI Preview** – Use `z-photo.exe <file_path>` to quickly preview a single file, integrable with the system right‑click menu.

---

## 🛠️ Build Instructions

### Requirements

- Qt 6.7.3 (MinGW 11.2.0)
- CMake 3.16+
- MinGW 11.2.0 (or compatible)
- SQLite3 (already integrated in the source)

### Windows Build Steps

```bash
# Clone or enter the project directory
cd Z-Photo

# Create a build directory
mkdir build
cd build

# Generate MinGW Makefile
cmake -G "MinGW Makefiles" ..

# Build
mingw32-make

# Deploy Qt dependencies (place the generated Z-Photo.exe together with required DLLs)
# Replace the path below with your own Qt installation path
D:\Qt\6.7.3\mingw_64\bin\windeployqt.exe Z-Photo.exe
```

> **Note**: Replace `D:\Qt\6.7.3\mingw_64\` with your actual Qt installation path.

### Linux / macOS Build

```bash
mkdir build && cd build
cmake ..
make
```

---

## 📖 Usage

### GUI Mode

Run `Z-Photo.exe` directly; the startup dialog will ask you to choose:

- **Local Mode**: Specify the database file (`media.db`) and the media root directory (where `data/` is located).
- **Network Mode**: Enter the server IP and port (see the network mode section below).

Once in the main interface:

- The left panel shows all directories; click one to load its media into the grid on the right.
- The top toolbar lets you filter by type (All / Images / Videos) and sort by time or file name.
- The bottom filter bar provides:
  - **All Directories**: Ignore current selection and query across all folders.
  - **Limit Count**: Randomly sample a specified number of media items (handy for quick browsing).
  - **Time Range**: Filter by modification date range.
  - **Tag Query**: Opens a dialog to select tags and returns all media with those tags.
- Click any thumbnail to open a standalone preview window for viewing full-size images or playing videos, with support for left/right navigation.

### Command‑line Mode

```bash
# Directly preview a single media file
Z-Photo.exe "D:/path/to/photo.jpg"
Z-Photo.exe "D:/path/to/video.mp4"
> **💡 Tip**: You can also right‑click an image or video file → **Open with** → **Choose another app** → browse to `Z-Photo.exe` for quick preview.

# Show help
Z-Photo.exe --help
```

---

## ⚡ Lazy Loading (Performance Core)

Z-Photo employs **viewport‑aware** thumbnail management to keep memory usage low even with 100,000+ files.

This mechanism is driven centrally in the main window (`z-photo.cpp`) via the `updateVisibleImages` function, coordinating with `ImageGridWidget`.

### How It Works

1. **Calculate visible area**: The main window listens to the scrollbar of `QScrollArea` to obtain the current viewport rectangle within the scroll content.
2. **Expand buffer**: Adds a **400‑pixel** buffer above and below the visible area – this pre‑loads images that are about to enter the viewport, eliminating white‑screen waiting.
3. **Iterate grid labels**: The main window loops through the `QList<MediaLabel*>` in `ImageGridWidget` and checks if each label's `geometry()` intersects the buffered rectangle.
4. **Dynamic load/unload**:
   - If it intersects and not loaded → call `loadImage()` to display the thumbnail.
   - If it does not intersect and already loaded → call `unloadImage()` to free the image memory (`QPixmap` cleared).
5. **Debounce optimizations**:
   - Scroll events use a 300ms single‑shot timer to avoid excessive computations during fast scrolling.
   - Resize events (window resize / splitter drag) use a 100ms timer and force a repaint of already loaded images to adapt to new sizes.

### Key Code Snippet

```cpp
// Main window's updateVisibleImages lambda
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
```

---

## 🌐 Local and Network Dual Mode

### Local Mode

- Directly accesses the operating system file system – media paths are built by concatenating the user‑supplied `data_root` with the relative paths stored in the database.
- Thumbnails load from the local disk very fast.
- Ideal for personal computers, external hard drives, etc.

### Network Mode

- The server reads a configuration file to locate the root directory and provides media file streams to clients.
- **IPv6 support**: The server listens on `::` (all IPv6 addresses), allowing clients to connect directly via IPv6 without NAT traversal.
- **Resume support**: The server correctly implements HTTP `Range` requests (`206 Partial Content`), enabling video seeking.
- **Video optimization**: It is recommended to transcode videos to H.264 with `faststart` (move the `moov` atom to the beginning) for the best streaming experience.

### Network Mode Configuration

1. Create `server.ini` on the server side:
   ```ini
   [Server]
   rootDir=D:/your/media/root/
   ```
2. Run the server:
   ```bash
   MediaHttpServer.exe
   ```
3. On the client, launch Z‑Photo, choose “Network Mode”, and enter the server IPv6 address and port (default 8080).

---

## 📂 Project Structure

```
Z-Photo/
├── AddTagWindow/       # Tag selection & management window
├── AppConfig/          # Global configuration (singleton)
├── DateTimeUtils/      # UTC/Local timestamp conversion utilities
├── ImageGridWidget/    # Grid view and media manager connection
├── ImagePlayer/        # Image preview window (zoom/drag/fullscreen)
├── MediaLabel/         # Thumbnail widget (supports both images and videos)
├── VideoPlayer/        # Video player (progress/volume/fullscreen/loop)
├── ZLens/              # Unified media preview entry (auto‑switches between image/video players)
├── limitRandom/        # Fisher‑Yates random sampling algorithm
├── sqliteApi/          # SQLite wrapper (transactions, indices, multi‑tag queries)
├── style/              # QSS stylesheets
├── icon/               # Application icon resources
├── resources.rc        # Windows resource file (defines the program icon)
├── style.qrc           # Qt resource file (embeds QSS at compile time)
├── z-photo.cpp         # Main entry (GUI + CLI dual mode, lazy loading logic)
└── CMakeLists.txt      # CMake build script
```

### Core Modules Description

| Module | Responsibility |
| :--- | :--- |
| `ZLens` | Unified preview entry – creates `ImagePlayer` or `VideoPlayer` based on `media_type`, handles window switching and signal forwarding. |
| `ImagePlayer` | Image preview window based on `QGraphicsView` – supports zoom, drag, fullscreen, keyboard shortcuts (left/right navigation, T to add tags). |
| `VideoPlayer` | Video player based on `QMediaPlayer` – supports progress drag, volume control, loop playback, 1.5x long‑press acceleration. |
| `ImageGridWidget` | Container for media thumbnails – provides `mediaList` and `layout()` for the main window’s lazy loading logic, and triggers `ZLens` preview on user click. |
| `sqliteApi` | SQLite wrapper supporting WAL mode, transactions, UPSERT, multi‑tag combined queries and sorting. |
```

---

## 🖥️ Media Preview Quick Guide

Z‑Photo includes full‑featured image and video viewers with keyboard shortcuts and mouse interactions.

### Image Viewer

The image preview window is built with `QGraphicsView`, supporting smooth zoom, pan, and fullscreen.

| Action | Key / Operation |
| :--- | :--- |
| Toggle fullscreen | `F11` |
| Zoom in | Scroll wheel up |
| Zoom out | Scroll wheel down |
| Drag image | Left mouse button + drag |
| Open tag popup | `T` |
| Next image | `D` or `→` (Right arrow) |
| Previous image | `A` or `←` (Left arrow) |

---

### Video Player

The video player is based on `QMediaPlayer`, with progress drag, speed playback, and full volume control.

| Action | Key / Operation |
| :--- | :--- |
| Toggle fullscreen | `F11` |
| Forward 5 seconds | `R` or `→` (Right arrow) |
| Backward 5 seconds | `L` or `←` (Left arrow) |
| 1.5x speed (hold) | Hold `R` or hold `→` (Right arrow) |
| Play / Pause | `Space` or click the pause button |
| Mute / Unmute | `M` |
| Volume up | `↑` (Up arrow) |
| Volume down | `↓` (Down arrow) |
| Loop / Single‑play toggle | `E` (default loop) |
| Open tag popup | `T` |
| Next video | `D` |
| Previous video | `A` |

**Progress bar operation**:
- Drag the slider (round handle) on the progress bar to seek.
- Note: **Clicking directly on the progress bar is not supported** – please drag the handle.

---

### Common Operations

These actions work in **both** the image viewer and video player:

| Action | Key |
| :--- | :--- |
| Open tag popup | `T` |
| Next media | `D` |
| Previous media | `A` |

---

## 📦 Building a Local Media Library

### 0. Initialize the Database

Before using the indexing tools, you need to create the database schema. `initdb.exe` creates the `media`, `tag`, and `media_tag_rel` tables, and automatically enables foreign key constraints, WAL mode, and a 20MB cache.

**Usage**:
```cmd
initdb.exe [database_filename]
```

- If no filename is given, it creates `metadata.db` in the current directory.
- If a filename is provided (e.g., `initdb.exe mylib.db`), it creates `mylib.db`.

**Examples**:
```cmd
# Create default database
initdb.exe

# Create a custom‑named database
initdb.exe D:\Z_project\media.db
```

After execution, the console will show `Database schema created successfully.`, indicating the database is ready.

---

Z‑Photo provides two companion tools to help you build your index and optimize video formats from scratch.

### 1. Video Pre‑processing (Optional but Recommended)

For network playback, videos need to meet two conditions: **H.264 encoding** and **`moov` atom at the beginning** (`faststart`). Z‑Photo includes a batch script `video_format.bat` to handle this.
For local playback, `faststart` is not strictly required, but still recommended.

**Usage**:
```cmd
video_trans_pro.bat <source_directory>
```

Example:
```cmd
video_trans_pro.bat data\video_source\TwDown
```

**Script behavior**:
- Scans all common video formats (`.mp4`, `.mov`, `.mkv`, `.avi`, `.flv`, `.webm`) in the source directory.
- If the video is already H.264, it only copies the stream and moves `moov` (very fast).
- Otherwise, it attempts NVIDIA hardware transcoding (`h264_nvenc`) and falls back to CPU software encoding (`libx264`) on failure.
- Output files go to `data\video\<subdirectory>`, preserving the original filename.
- The source file is backed up as `original_filename_src.extension` to avoid duplicate processing.

**⚠️ Note**: The script hard‑codes the target root directory as `data\video`. To change it, edit the `TARGET_ROOT` variable in the script.

---

### 2. Metadata Indexing & Thumbnail Generation

`Indexer.exe` is the core tool that scans media files, extracts metadata, generates thumbnails, and writes them into the SQLite database.

**Basic usage**:
```cmd
Indexer.exe <scan_directory> <root_directory> [database_path] [--incremental]
```

| Parameter | Description |
| :--- | :--- |
| `<scan_directory>` | Directory to scan (recursive), e.g., `data/image` or `data/video`. |
| `<root_directory>` | The media library root path used to compute relative paths, e.g., `D:/Z_project/`. |
| `[database_path]` | Optional, defaults to `metadata.db`. |
| `[--incremental]` | Optional, enables incremental mode (see below). |

**Examples**:
```cmd
# Full scan of images
Indexer.exe D:\Z_project\data\image D:\Z_project\ D:\Z_project\media.db

# Incremental scan of videos
Indexer.exe D:\Z_project\data\video D:\Z_project\ D:\Z_project\media.db --incremental
```

**Processing includes**:
- Recognizes images (`.jpg`, `.png`, `.webp`, etc.) and videos (`.mp4`, `.mov`, `.mkv`, etc.).
- Generates `_ithu` thumbnails for images (keeping the original extension).
- Generates `_vthu.jpeg` thumbnails for videos (using FFmpeg to capture the 1‑second frame).
- Writes file path, name, directory, modification time (UTC seconds), thumbnail path, and media type into the `media` table.
- If a file already exists, it is updated automatically (`ON CONFLICT DO UPDATE`), preserving tag associations.

> **⚠️ Note**: Although `Indexer.exe` supports recursive scanning, **it is recommended to process each directory separately** to better track progress and simplify troubleshooting.

---

### 3. Incremental Mode (`--incremental`)

Incremental mode compares each file’s modification time (`mtime`) with the maximum `mtime` stored for that directory in the database, and **processes only files newer than that baseline**, greatly reducing redundant scanning.

**Use cases**:
- You added new files to an existing directory.
- You replaced a file (its modification time updates, so it will be detected).

**⚠️ Scope notes**:

| File type | Affected by incremental logic? | Explanation |
| :--- | :--- | :--- |
| **Videos** | ✅ Works normally | Pre‑processing (transcoding) happens before indexing; transcoded files usually get the current timestamp, so they are picked up correctly. |
| **Images (directly downloaded)** | ✅ Works normally | Downloaded images keep their original modification time; if that is later than the database baseline, they get indexed. |
| **Images (extracted from archives)** | ⚠️ May be missed | Extraction often sets the file time to the original archive time (usually older). If that is earlier than the directory’s max `mtime`, incremental mode will skip them. |

**Solutions**:
- For extracted archives, do a **full scan** (without `--incremental`) the first time, then use incremental for later additions.
- Or use a tool like `touch` to update file modification times to the current time before running incremental scans.

---

## 🧩 Typical Workflow

1. **Initialize database** → run `initdb.exe` to create `media.db`.
2. **Download/collect media** → place them into `data/video_source` or `data/image` as appropriate.
3. **Pre‑process videos** → run `video_trans_pro.bat` to convert them to H.264 + faststart, automatically placing them into `data/video`.
4. **Full index (first time)** → run `Indexer.exe` on `data/image` and `data/video` to generate thumbnails and database records.
5. **Incremental index (later)** → use `--incremental` for regular scans, processing only new or modified files.

This way your library always stays up‑to‑date, videos are network‑optimized, thumbnails are generated on‑demand, and memory/storage overhead remains manageable.

---

## 🌐 Network Mode System

Z‑Photo also offers a lightweight network mode that lets you access your media library remotely over IPv4/IPv6, like a personal cloud drive.

### Architecture Overview

The network mode uses a **client‑server (C/S) architecture**:

- **Server**: `MediaHttpServer.exe` serves media file streams.
- **Client**: Z‑Photo main program, when started in “Network Mode”, pulls media via HTTP.

```
┌─────────────┐     HTTP / IPv6     ┌─────────────┐
│   Z-Photo   │ ◄────────────────── │   Server    │
│  (Client)   │     Range requests  │ (MediaHttp  │
│             │ ──────────────────► │   Server)   │
└─────────────┘     request chunks  └─────────────┘
```

---

### 1. Server Deployment

#### 1.1 Configuration File

The server reads its root directory from `server.ini`, which should be placed in the same directory as the server executable.

**Example `server.ini`**:
```ini
; Media root directory configuration
[Server]
rootDir=D:/Z_project/
```

- `rootDir`: The media library root; the server appends the request path to this.
- Comments start with `;` or `#` and **must be on their own line** (as per `QSettings` parsing rules).

#### 1.2 Starting the Server

```cmd
MediaHttpServer.exe
```

After successful startup, the console outputs:
```
Using rootDir: D:/Z_project/
Server listen on 0.0.0.0:8080
```

The server listens on **port 8080** by default and supports both **IPv4 and IPv6** (`QHostAddress::Any`).

#### 1.3 Routing Rules

The server provides two routes:

| Route | Purpose | Example |
| :--- | :--- | :--- |
| `/images/` | Image file requests | `GET /images/data/image/001.webp` → `D:/Z_project/data/image/001.webp` |
| `/videos/` | Video file requests | `GET /videos/data/video/movie.mp4` → `D:/Z_project/data/video/movie.mp4` |

> **Note**: `relativePath` is extracted from the request URL. For example, a request to `/images/data/image/001.webp` becomes `rootDir + "data/image/001.webp"`.

#### 1.4 Key Features

- **Resume support**: Correctly implements HTTP `Range` requests (`206 Partial Content`), allowing video seeking.
- **Streaming**: Sends data in 64KB chunks to avoid loading large files entirely into memory.
- **Path protection**: Prevents directory traversal attacks – only files under `rootDir` are accessible.

---

### 2. Client Configuration

When launching Z‑Photo, choose **“Network Mode”** and fill in:

- **Server IP**: Supports IPv4 (e.g., `192.168.1.100`) and IPv6 (e.g., `[240e:...]`).
- **Port**: Default `8080`.
- **Database path**: Needs to be obtained from the remote side (see below).

---

### 3. Fetching the Remote Database File

Before using network mode for the first time, you need to download the database file from the server to your local machine. Z‑Photo provides `fetch_db.exe` for this.

**Usage**:
```cmd
fetch_db.exe
```

Interactive input:
```
==== Fetch remote database file ====
Remote server IP (e.g., 127.0.0.1 or [240e:...]): 192.168.1.100
Port (e.g., 8080): 8080
Remote database path (e.g., /images/media.db): /images/media.db
```

The tool sends a GET request to the server and saves the response body as a local file with the same name (e.g., `media.db`).

**Manual way** (using curl):
```cmd
curl -o media.db http://192.168.1.100:8080/images/media.db
```

---

### 4. Complete Workflow (Network Mode)

1. **Server side**:
   - Place media files on the server (e.g., `D:/Z_project/data/`).
   - Create `server.ini` with the correct `rootDir`.
   - Start `MediaHttpServer.exe`.

2. **Client side**:
   - Run `fetch_db.exe` to download `media.db` from the server.
   - Launch Z‑Photo, choose “Network Mode”, and enter the server IP and port.

3. **Browse media**:
   - The client will request thumbnails and media files from the server, offering an experience almost identical to local mode.
   - Video seeking works smoothly thanks to the server’s `Range` support.

---

### 5. Advantages of Network Mode

- **Cross‑device sharing**: Multiple devices on your LAN (or over IPv6) can share the same media library.
- **Saves local storage**: Media files reside centrally on the server; clients don’t need to store duplicates.
- **Ready to use**: With `fetch_db.exe`, clients only need a lightweight database to start browsing.
- **Controlled access**: The server only serves file streams without authentication (can be secured via firewall or VPN).

---

### 6. Advanced Configuration (Optional)

If you wish to change the server port, modify `server.listen(QHostAddress::Any, 8080)` in `main.cpp`, or make it configurable (e.g., also read from `server.ini`).

---

## 📜 License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgements

- [Qt](https://www.qt.io/) – Cross‑platform GUI framework
- [SQLite](https://www.sqlite.org/) – Lightweight embedded database
- [FFmpeg](https://ffmpeg.org/) – Powerful multimedia processing tool
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) – Lightweight C++ HTTP client/server library (used by `fetch_db.exe`)
- [MinGW](https://www.mingw-w64.org/) – GCC toolchain for Windows
```