# Installers

Offline installers built with the
[Qt Installer Framework](https://doc.qt.io/qtinstallerframework/) (IFW 4.x).

Component id (shared by all platforms): `com.googleaiviewer.app`
Version single source: `project(VERSION ...)` in `CMakeLists.txt`.
`GOOGLEAIVIEWER_VERSION` in the environment overrides it.

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
set QT_DIR=C:\Qt\6.11.1\msvc2022_64
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

## macOS / Linux

Not yet added. Follow the same shape when the time comes:
`Installer/<OS>/{config/config.xml, packages/<same-component-id>/{meta,data/}}`
plus `macdeploy.sh` / `linuxdeploy.sh` and `make-installer.sh`.
