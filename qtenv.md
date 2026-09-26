# Qt6 Build Environment

## Environment Variables
```
QT_DIR=C:\Qt\6.11.1\msvc2022_64
PATH=%QT_DIR%\bin;%PATH%
```

## Compiler
Visual Studio 2026 — MSVC
Installed at: `C:\Program Files\Microsoft Visual Studio\18\Community`

Initialize x64 environment:
```
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```

## CMake
Version: 3.30.5
Path: `C:\Qt\Tools\CMake_64\bin\cmake.exe`

## Ninja
Path: `C:\Qt\Tools\Ninja\ninja.exe`

## Build Commands
```bat
:: Configure
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=%QT_DIR%

:: Build
cmake --build build
```

## Project Settings
- C++20 (`-std:c++20`)
- Qt6 modules: Core, Gui, Widgets, Network, Concurrent
- CMake: AUTOMOC, AUTOUIC, AUTORCC enabled
