# Multi-Platform Qt Installers + GitHub Workflows — Agent Playbook

Reusable recipe for giving any Qt 6 Widgets app offline installers on
Windows / macOS / Linux plus CI and tag-triggered GitHub Releases.
Distilled from a working setup (Qt 6.10/6.11, CMake + Ninja, Qt Installer
Framework 4.11). Adapt names, component id, and Qt version; keep the
structure and the gotchas.

## 1. Layout

```
Installer/
  README.md                    # per-platform commands
  Windows/
    config/config.xml          # IFW installer configuration
    packages/<component-id>/
      meta/package.xml         # component metadata (version, license)
      meta/installscript.qs    # shortcuts etc.
      meta/*.txt               # license text shown by the wizard
      data/                    # STAGED payload, gitignored
    windeploy.bat              # build Release + stage data/
    make-installer.bat         # binarycreator -> *-offline.exe
  MacOS/
    config/config.xml
    packages/<component-id>/{meta,data/}   # data/ gitignored
    macdeploy.sh               # build Release + .app + macdeployqt
    make-installer.sh          # hdiutil DMG (default) or IFW DMG (--ifw)
  Linux/
    config/config.xml
    packages/<component-id>/{meta,data/}   # data/ gitignored
    linuxdeploy.sh             # build Release + bundle Qt via ldd/patchelf
    make-installer.sh          # binarycreator -> *-offline.run
    markdowneditor.desktop/.png/markdowneditor.sh  # metadata + launcher
.github/workflows/
  ci.yml       # per-push: build + test (+ installer artifact where cheap)
  release.yml  # per-tag (v*): build installers, attach to the Release
```

Rules:
- One shared component id (e.g. `com.example.myapp`) everywhere, but a
  **separate `packages/.../data` per platform** so staging never clobbers
  another OS payload.
- Gitignore: `Installer/*/packages/*/data/`, `Installer/Windows/*.exe`,
  `Installer/MacOS/*.dmg`, `Installer/Linux/*.run`.
- Version single source: `project(VERSION x.y.z)` in CMakeLists.txt.
  Installer scripts parse it; `MARKDOWNEDITOR_VERSION` env overrides it
  (release jobs set it from the tag). Keep `config.xml` /
  `package.xml` `<Version>` in sync on bump day, plus CHANGELOG.

## 2. Windows deploy (`windeploy.bat` pattern)

1. Build the Release preset (via an env wrapper like `devcmd.bat`, or
   directly when `VSCMD_VER` is already set, e.g. under
   `ilammy/msvc-dev-cmd`). Support `MARKDOWNEDITOR_SKIP_BUILD=1`.
2. Clean `data/`, copy the exe.
3. Run windeployqt: try `windeployqt6.exe`, fall back to `windeployqt.exe`;
   `--no-translations --no-system-d3d-compiler --no-opengl-sw`; delete the
   dropped `vc_redist` bootstrapper.
4. Bundle the MSVC CRT (`vcruntime140.dll`, `vcruntime140_1.dll`,
   `msvcp140.dll`) — no redist installer needed. Discover, don't hardcode:
   `VC_REDIST_ROOT` override → `VCToolsRedistDir` (active vcvars) →
   `vswhere -latest ...` → well-known VS 17/18 roots × editions.
   Within a version dir prefer `VC145` over `VC143`; compare version
   numbers **numerically** (`14.44` beats `14.9`; reverse-alpha sort lies).
5. Optional plugin stacks (spell check etc.) copy from `build/prefix`,
   skipped gracefully when absent.

`make-installer.bat` pattern: resolve `binarycreator.exe` (explicit
`IFW_BIN` file-or-dir → sibling `Tools/QtInstallerFramework` of `QT_DIR`
→ `C:\Qt\...` → `where`), prefer 4.11 else numerically newest 4.x;
`--offline-only` to `MarkdownEditor-<version>-offline.exe`.

## 3. macOS / Linux deploy (script patterns)

macOS (`macdeploy.sh`): `cmake --preset <preset>` + build, `ditto` the
`.app` into `data/`, `macdeployqt` (`-always-overwrite`, optional
`-codesign=`). Universal builds via a `CMAKE_OSX_ARCHITECTURES=arm64;x86_64`
preset. `make-installer.sh`: native drag-and-drop DMG with `hdiutil`
(default; needs no IFW) or `--ifw` wizard DMG. Validate with `otool -L`
(no build-tree paths), `plutil -lint Info.plist`, `codesign --verify`.

Linux (`linuxdeploy.sh`): copy binary as `*.bin` + launcher wrapper;
collect `libQt6*/libicu*/libKF6*` via `ldd` over the binary **and every
staged plugin**, repeat to closure; copy the needed `plugins/` subsets;
write `qt.conf` (`Prefix=.`, `Libraries=lib`, `Plugins=plugins`); `patchelf
--set-rpath '$ORIGIN/lib'` (libs: `$ORIGIN`, plugins:
`$ORIGIN/../../lib`); stage icon + `.desktop`. Never bundle system libs
(xcb, GL, fontconfig) — the wrapper preflights them with an apt hint.
`make-installer.sh`: `binarycreator --offline-only` to `.run`; smoke-test
the staged app headless (`QT_QPA_PLATFORM=offscreen timeout 10s`).

## 4. CI (`ci.yml` pattern)

One job per OS. Common shape per job: checkout → toolchain → Qt via
`jurplel/install-qt-action@v4` (`cache: true`) → export `QT_DIR` from
`qmake -query QT_INSTALL_PREFIX` (the presets read `$env{QT_DIR}`) →
configure debug → build → `ctest` (headless: `QT_QPA_PLATFORM=offscreen`)
→ optional installer build + `actions/upload-artifact` (90 days; artifacts
are temporary, Releases are permanent).

- Ubuntu (`ubuntu-24.04`, oldest supported = glibc baseline): apt-install
  `ninja-build` (+ `patchelf` if bundling).
- Windows (`windows-latest`): `ilammy/msvc-dev-cmd@v1` (`arch: x64`;
  Node-20 deprecation warning is benign — the runner force-upgrades it;
  a manual vswhere+GITHUB_ENV replacement was tried and did NOT carry the
  env reliably). IFW via `pip install aqtinstall` +
  `aqt install-tool windows desktop tools_ifw` (cache the dir). Run `.bat`
  steps with `shell: cmd`. Validate `cmake`, `ninja`, `where cl`, `cl`.
- macOS (`macos-14`): universal preset via env, upload the DMG.

Tests under `cmd` need no extra env; QtTest suites should force
`QT_QPA_PLATFORM=offscreen` themselves (CTest property or in-code).

## 5. Releases (`release.yml` pattern)

```yaml
on:
  push:
    tags: ['v*']
permissions:
  contents: write
jobs:
  release-windows / release-linux / release-macos:
    # same install steps as CI, then the deploy scripts, then:
      - uses: softprops/action-gh-release@v2
        with:
          generate_release_notes: true
          fail_on_unmatched_files: true
          files: Installer/Windows/MarkdownEditor-*-offline.exe  # etc.
```

Each job derives `MARKDOWNEDITOR_VERSION` from the tag (`${GITHUB_REF_NAME#v}`,
PowerShell: `$env:GITHUB_REF_NAME -replace '^v',''`) so filenames match
the release even if CMakeLists lags. Tag runs use the workflow file
**pinned at the tag** — fixing `main` never re-runs a tag; re-tag or bump.
Release process checklist: bump versions (CMakeLists, 3× config.xml, 3×
package.xml + dates, CHANGELOG) → commit → tag `vX.Y.Z` → push commit +
tag → watch the Release run → delete superseded incomplete releases.

## 6. Known pitfalls (error signature → fix)

- `python.exe failed with exit code 1` in Install-Qt on **Windows with Qt
  ≥ 6.11**: Qt moved Windows metadata to a per-arch split layout
  (`qt6_6111/qt6_6111_msvc2022_64`, no flat `Updates.xml`) which aqtinstall
  ≤ 3.3.0 cannot read (`Failed to locate XML data`). Upstream:
  `miurahr/aqtinstall#1007`. Fix: pin Windows to Qt 6.10.x
  (`win64_msvc2022_64` still flat) until upstream fixes it; Linux/macOS
  stay on 6.11 (flat layout, verified via `aqt ... --dry-run`).
- `. was unexpected at this time.` in a `.bat` step: a `).` sequence
  inside a parenthesized block (e.g. `echo ... (%VAR%).`). Reword; never
  emit `).` in blocks. (Balanced parens alone are fine.)
- Batch file invoked without `call` (including workflow `run:` lines):
  control transfers and the caller never resumes. Always `call *.bat`.
- `if %VAR%==...` with empty/special values: use `if defined` +
  `if /I "%VAR%"=="..."`; `set "X=..."` quoted syntax everywhere.
- Reverse-alphabetical ≠ newest for versions (4.8.1 > 4.12.0, 14.9 >
  14.44 alphabetically). Compare MAJOR/MINOR/PATCH numerically; skip
  non-numeric dirs (validate digits without `findstr` regex class
  combos — unreliable; strip digits via `call set` instead, guarding the
  empty-variable `5=` artifact).
- `no such option: --break-system-externally-allowed` (Ubuntu 24.04,
  PEP 668): the real flag is `--break-system-packages`.
- `install-qt-action` needs a new-enough Python (it manages setup-python
  itself; don't fight it) and `arch:` names changed with Qt 6.11 repo
  layout (`linux_gcc_64`, not `gcc_64`).
- Windows CI geometry tests (scroll sync, anchors, icon ink) need real
  fonts: the offscreen platform has none → set `QT_QPA_FONTDIR:
  'C:/Windows/Fonts'` on the test step (Linux/macOS resolve natively).
  Same for local runs.
- QTest stdout vanishes on failure/crash when piped: use
  `test.exe -o file,txt` and read the file; run with a timeout guard and
  kill leftovers. Headless modal dialogs (e.g. recovery prompts) hang
  CI: tests must start from clean state dirs (`QStandardPaths::
  setTestModeEnabled(true)` + pre-clean).

## 7. Local verification recipes

- Batch subroutines: extract label-to-EOF verbatim into a harness, drive
  with fake trees (adversarial versions: `14.9` vs `14.44`, `4.8.1` vs
  `4.12.0`, junk dirs), assert picks. Redspectively caught real bugs here.
- Stub external tools (`cmake.bat` echoing args + `exit /b 0`) to execute
  deploy scripts through every branch without full builds.
- `aqt install-qt <host> desktop <ver> <arch> --dry-run` validates repo
  resolution without downloading gigabytes; inspect `Updates.xml`
  directly when it fails (watch for version/extension collisions like
  `qt6_611` = 6.1.1 vs 6.11.x).
- Render-based checks (icon ink bounds, print-vs-screen lightness) via
  throwaway QtTest slots against `QImage`: assert, then delete the slot
  if font-metric brittleness makes it CI-unfit.
- New unit tests belong in the committed suite; throwaway probes must
  never be committed.
