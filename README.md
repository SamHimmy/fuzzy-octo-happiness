# Song Unlocker — Geode Mod

Removes the Newgrounds song whitelist in Geometry Dash, letting you attach **any** NG track to your levels — not just ones from RobTop-approved artists.

---

## What it does

| Without mod | With mod |
|---|---|
| Only songs from whitelisted artists can be used | All Newgrounds songs are usable |
| Warning banner shown on non-whitelisted tracks | No warning banners |
| Non-whitelisted songs blocked at level save/upload | Songs save and upload normally |

---

## Requirements

| Requirement | Version |
|---|---|
| Geometry Dash | 2.2074 |
| Geode | ≥ 4.0.0 |

---

## Building from source

### 1. Install Geode SDK

Follow the official guide: https://docs.geode-sdk.org/getting-started/

Make sure the `GEODE_SDK` environment variable points to your SDK folder.

### 2. Clone this repo

```bash
git clone https://github.com/yourname/song-unlocker
cd song-unlocker
```

### 3. Configure with CMake

**Windows**
```bash
cmake -B build -A x64
cmake --build build --config Release
```

**macOS**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Android** (cross-compile from any host)
```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-23 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 4. Install

The built `.geode` file is placed in `build/yourname.song-unlocker.geode`.

Copy it to your Geode mods folder:
- **Windows:** `%LOCALAPPDATA%\GeometryDash\geode\mods\`
- **macOS:** `~/Library/Application Support/GeometryDash/geode/mods/`
- **Android:** `/sdcard/Android/data/com.robtopx.geometryjump/geode/mods/`

Or drag-and-drop it onto the in-game Geode mods screen.

---

## How it works

The mod hooks three layers of GD's whitelist system:

1. **`MusicDownloadManager::isVerifiedSong`** — the primary API-level gate; overridden to always return `true`.
2. **`CustomSongWidget`** — patches the song picker UI so the warning label never appears and the "Use" button is always enabled.
3. **`LevelEditorLayer` / `EditLevelLayer`** — pre-marks the attached song as verified before the editor opens, preventing validation errors on save/upload.

---

## License

MIT — see `LICENSE` for details.
