# Installers

Offline installers built with the
[Qt Installer Framework](https://doc.qt.io/qtinstallerframework/) (IFW 4.x).

Component id (shared by all platforms): `com.googleaiviewer.app`
Version single source: `project(VERSION ...)` in `CMakeLists.txt`.
`GOOGLEAIVIEWER_VERSION` in the environment overrides it.
Keep `<Version>` in each `config/config.xml` and `packages/.../meta/package.xml`
in sync with `CMakeLists.txt` on bump day.

## Windows

Prerequisites: MSVC 2022 x64, CMake + Ninja, Qt 6 kit, IFW 4.x
(default locations are auto-detected; overrides below).

```cmd
cd Installer\Windows
call windeploy.bat
call make-installer.bat
```

Output: `Installer\Windows\GoogleAIViewer-<version>-offline.exe`

### Useful overrides

```cmd
set QT_DIR=C:\Qt\<QT_VERSION>\msvc2022_64
set GOOGLEAIVIEWER_VERSION=1.0.0
set GOOGLEAIVIEWER_SKIP_BUILD=1
set GOOGLEAIVIEWER_EXE=C:\path\to\GoogleAIViewer.exe
set VC_REDIST_ROOT=C:\path\to\VC\Redist\MSVC\14.44.35207
set IFW_BIN=C:\Qt\Tools\QtInstallerFramework\4.11\bin
```

`windeploy.bat` builds `build\windows-release` in Release, stages
`packages\com.googleaiviewer.app\data\` (exe + Qt libs via windeployqt +
`qt6keychain.dll` + MSVC CRT), and `make-installer.bat` wraps it into the
offline installer. The payload dir (`packages/*/data/`) and `*.exe`
outputs are gitignored.

## Linux (Ubuntu 24.04+)

Prerequisites: Qt 6.x `gcc_64` (version per `qt-versions.env`), IFW 4.x, `cmake`, `ninja-build`,
`patchelf` (default locations are auto-detected; overrides below).

```bash
sudo apt install ninja-build patchelf libsecret-1-dev
export QT_DIR=$HOME/Qt/<QT_VERSION>/gcc_64

# 1. Build Release + stage packages/com.googleaiviewer.app/data/
Installer/Linux/linuxdeploy.sh

# Reuse an existing build tree instead of rebuilding:
GOOGLEAIVIEWER_SKIP_BUILD=1 Installer/Linux/linuxdeploy.sh --build-dir build

# 2. Create the offline installer
Installer/Linux/make-installer.sh
```

Output: `Installer/Linux/GoogleAIViewer-<version>-offline.run`

Notes:

- Only `libQt6*` / `libicu*` / `libKF6*` plus `libqt6keychain*`
  (FetchContent-built, resolved from the build tree) are bundled — ldd
  closure over the binary, every staged plugin, and every staged lib.
  System libs (xcb, GL, fontconfig) are never bundled; the launcher prints
  an `apt install` hint when missing.
- `qt.conf` pins `Prefix=.`, `Libraries=lib`, `Plugins=plugins`; rpaths are
  `$ORIGIN/lib` (binary), `$ORIGIN` (libs), `$ORIGIN/../../lib` (plugins).
- `make-installer.sh` prefers IFW 4.11, else the numerically newest 4.x
  (reverse-alphabetical lies: 4.12.0 beats 4.8.1 only with numeric compare),
  and smoke-tests the staged payload headless
  (`QT_QPA_PLATFORM=offscreen`, 10 s timeout).

## macOS

Prerequisites: Qt 6.x `macos` or `clang_64` (version per `qt-versions.env`), CMake + Ninja, Xcode CLT
(default locations are auto-detected; overrides below).

```bash
export QT_DIR=$HOME/Qt/<QT_VERSION>/macos

# 1. Build Release + .app + macdeployqt + stage packages/com.googleaiviewer.app/data/
Installer/MacOS/macdeploy.sh

# Reuse an existing build tree instead of rebuilding:
GOOGLEAIVIEWER_SKIP_BUILD=1 Installer/MacOS/macdeploy.sh --build-dir build

# Universal binary (arm64 + x86_64):
Installer/MacOS/macdeploy.sh --universal

# 2. Create the DMG installer (drag-and-drop, default)
Installer/MacOS/make-installer.sh

# Or create an IFW wizard DMG (requires IFW 4.x):
Installer/MacOS/make-installer.sh --ifw
```

Output: `Installer/MacOS/GoogleAIViewer-<version>.dmg`

Notes:

- `macdeploy.sh` builds a Release `.app`, runs `macdeployqt` to inline Qt
  libraries, and stages the result into `packages/com.googleaiviewer.app/data/`.
  It validates with `otool -L`, `plutil -lint`, and `codesign --verify`.
- `make-installer.sh` defaults to a drag-and-drop DMG via `hdiutil` (no IFW
  needed). Use `--ifw` for an IFW wizard DMG (requires `binarycreator`).
- The `.app` is ad-hoc signed by default. Set `CODESIGN_IDENTITY` to sign with
  a real identity for distribution outside the App Store.
- The payload dir (`packages/*/data/`) and `*.dmg` outputs are gitignored.
