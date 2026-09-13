# Google AI Studio Viewer

A native desktop app (Qt6/C++20) for browsing and searching your [Google AI Studio](https://aistudio.google.com/) conversation history.

AI Studio saves your conversations as JSON files in a `Google AI Studio` folder in your Google Drive, but there's no good way to browse or search that history — the web UI's own history panel is slow to scroll and hard to search across many conversations. This app signs in with your Google account, syncs those files down, caches them locally, and gives you a fast, native three-pane viewer with full-text search, image/attachment previews, and dark-mode support.

Builds and runs on **Linux, macOS, and Windows**.

## Features

- **Browse conversations** in a three-pane layout: conversation list → prompt list → rendered response, with markdown rendering for model output
- **Full-text search** across all synced conversations, backed by a local SQLite cache — search your entire history offline, not just the current file
- **Background sync** of your Drive folder with progress reporting, retry-on-failure, and automatic token refresh mid-sync
- **Attachments and inline images** are downloaded and previewed in place (images, text/code, or a binary fallback)
- **Model "thoughts"** can be toggled on/off separately from the final response
- **Read-only Google OAuth (PKCE)** — the app only ever requests `drive.readonly` access, and tokens are stored in your OS's native credential store (Windows Credential Manager, macOS Keychain, or Linux Secret Service/KWallet) via [QtKeychain](https://github.com/frankosterfeld/qtkeychain), never in plain text
- **Optional local-LLM backend** — you can switch the toolbar from "Google AI Studio" to "Local Server", which talks to any OpenAI-compatible chat endpoint (e.g. LM Studio, Ollama, llama.cpp server) at `http://localhost:8000` by default, so the same viewer can also be a lightweight local chat client

## Requirements

- CMake 3.27+
- A C++20 compiler (MSVC, Clang, or GCC)
- Qt6 (Core, Gui, Widgets, Network, Sql) — 6.5 or newer recommended
- Git (needed at configure time — QtKeychain is pulled automatically via `FetchContent`)

On Linux, QtKeychain builds against the Secret Service API, so you'll also want `libsecret-1-dev` (Debian/Ubuntu) or `libsecret-devel` (Fedora) installed, plus a running keyring service (GNOME Keyring or KWallet — most desktop environments already have one).

## Building

The build is the same CMake flow on all three platforms:

```bash
git clone <this-repo-url>
cd GoogleAIViewer
cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<kit>
cmake --build build --config Release
```

A few platform notes:

- **Linux**: install Qt6 via your distro's packages (`qt6-base-dev`, `qt6-tools-dev`, etc.) or the official Qt installer; `CMAKE_PREFIX_PATH` usually isn't needed if Qt6 is installed system-wide.
- **macOS**: `brew install qt` works well; point `CMAKE_PREFIX_PATH` at the Homebrew Qt6 prefix (e.g. `$(brew --prefix qt)`). The app builds as a proper `.app` bundle.
- **Windows**: install Qt6 via the [Qt online installer](https://www.qt.io/download-qt-installer) (pick the kit matching your compiler, e.g. MSVC 2022 64-bit) and point `CMAKE_PREFIX_PATH` at that kit's directory, e.g. `C:/Qt/6.7.0/msvc2019_64`.

The first configure will take a little longer than usual since CMake fetches and builds QtKeychain from source.

## Setting up Google API access

The app talks to Google Drive using an OAuth "Desktop app" client that you create yourself:

1. In the [Google Cloud Console](https://console.cloud.google.com/), create a project (or use an existing one) and enable the **Google Drive API**.
2. Under **APIs & Services → Credentials**, create an **OAuth client ID** of type **Desktop app**.
3. On first sign-in, the app will look for credentials at `Documents/AI/oauth_config.json`. If that file doesn't exist, it will prompt you for your Client ID and Client Secret directly and save them there for next time — so you don't need to hand-craft the file, just paste in the two values from the Cloud Console.
4. Click **Sign In**. A browser window opens for the Google consent screen; the app spins up a temporary local server on `127.0.0.1` to catch the OAuth redirect (PKCE + state-checked, nothing is exposed externally).
5. Once signed in, the app looks for a folder literally named **"Google AI Studio"** in your Drive — the same folder AI Studio creates when you save conversations there — lists what's in it, and starts syncing.

Everything downloaded is cached locally in a SQLite database under your OS's app-data directory (e.g. `viewer_cache.db`), so re-opening the app doesn't require re-downloading everything, and search works even offline.

## A note on how this was built

This app was vibe coded — most of it came out of conversations with an AI coding assistant rather than being typed line-by-line. But "vibe coded" doesn't mean "one-shot and done." Getting from a working prototype to something that handles PKCE and CSRF state correctly, refreshes tokens mid-sync without losing your place, stores secrets in the right OS vault on three different platforms, and doesn't garble message ordering when timestamps collide — that took a genuinely long time, a lot of iteration, and a fair amount of manually tracing through edge cases the model got wrong the first (or third) time.

Two frictions showed up repeatedly along the way, and honestly are part of why this project exists at all:

- **Searching past conversations is a bit clumsy.** You remember roughly what you asked about — "the thing where the OAuth callback server wasn't shutting down" — but re-finding that exact exchange in a long history, across dozens of saved chats, is slower than it should be.
- **Starting a new conversation often isn't a clean start.** Picking a fix back up in a fresh session usually takes several turns just re-explaining the file layout, prior decisions, and constraints before you can get back to the actual problem — context that a previous conversation already had, but that doesn't carry over.

This app doesn't solve the second problem, but it's a small attempt at the first one.

## Project structure

```
src/
├── api/     # Drive/OAuth-adjacent networking, conversation sync, local SQLite cache
├── app/     # Application bootstrap, logging, OAuth/token management
├── models/  # Conversation/message/attachment data model + parsing
└── ui/      # Main window and the three panes (conversation list, prompt list, response view)
```

## License

TBD — add a license before making the repository public if you intend others to use or contribute to it.
