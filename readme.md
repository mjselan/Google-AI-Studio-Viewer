# Google AI Studio Viewer

A native, high-performance desktop application (Qt6 / C++20) for browsing, searching, and organizing your [Google AI Studio](https://aistudio.google.com/) conversation history.

Google AI Studio saves prompts and chat sessions as JSON files inside a `Google AI Studio` folder in your Google Drive. However, the web UI lacks fast full-text search across past chats and scrolling through deep histories can be cumbersome. This app connects securely to your Drive via read-only OAuth, syncs your conversations to a local SQLite cache, and provides an instant three-pane native viewer with search, code highlighting, attachment previews, and dark mode support.

Runs natively on **Windows, macOS, and Linux**.

---

## Features

- **Three-Pane Desktop Layout:** Browse conversations on the left, inspect turn-by-turn user prompts in the middle, and read markdown-rendered model responses on the right.
- **Offline Full-Text Search:** Instantly search across conversation titles, user prompts, system instructions, and model responses backed by a local SQLite index.
- **Attachment Search & Parent Linking:** When "Search all attachments" is enabled, searches match text, code, and PDFs stored in Drive and automatically resolve them back to their parent conversation.
- **Inline Attachments & Code Previews:** Uploaded code snippets, text files, and images are previewed directly in the prompt area.
- **"Show Attachments" Toggle:** Easily collapse large code files (like project dumps or logs) into a compact badge to keep the conversation view clean.
- **Model "Thoughts" Inspection:** Toggle chain-of-thought reasoning blocks on/off, rendered with high-contrast styling in both Light and Dark themes.
- **Rate-Limited Background Sync:** Automatically keeps your local database in sync with Google Drive using request pacing, limited concurrency, and exponential backoff with jitter to stay well within Google API quotas.
- **Hardened OAuth 2.0 (PKCE):** 
  - Strictly requests `drive.readonly` scope.
  - Implements RFC 7636 (PKCE with SHA-256) and validates CSRF `state` nonces against an ephemeral loopback callback server (`127.0.0.1`).
  - Stores access and refresh tokens in your OS's native secure vault via [QtKeychain](https://github.com/frankosterfeld/qtkeychain) (**Windows Credential Manager**, **macOS Keychain**, or **Linux Secret Service / KWallet**).
- **Optional Local LLM Backend:** Switch the toolbar from "Google AI Studio" to "Local Server" to chat with any OpenAI-compatible server (e.g., Ollama, LM Studio, llama.cpp server, or vLLM) running on `http://localhost:8000`.

---

## Requirements

- **CMake:** 3.27 or newer
- **C++ Compiler:** C++20 compliant (MSVC 2022, GCC 11+, or Clang 13+)
- **Qt6:** 6.5 or newer (Modules: `Core`, `Gui`, `Widgets`, `Network`, `Sql`)
- **Git:** Required at CMake configure time to fetch `qtkeychain`

### Platform-Specific Packages
- **Linux:** Requires Secret Service development libraries and a running keyring daemon:
  - **Debian / Ubuntu:** `sudo apt install qt6-base-dev libsecret-1-dev`
  - **Fedora:** `sudo dnf install qt6-qtbase-devel libsecret-devel`
  - **Arch Linux:** `sudo pacman -S qt6-base libsecret`
- **macOS:** Install Qt6 via Homebrew: `brew install qt`
- **Windows:** Install Qt6 via the [Qt Online Installer](https://www.qt.io/download-qt-installer) (select the MSVC 2022 64-bit kit).

---

## Building from Source

The build workflow is identical across all platforms:

```bash
# 1. Clone the repository
git clone https://github.com/your-username/Google-AI-Studio-Viewer.git
cd Google-AI-Studio-Viewer

# 2. Configure with CMake (specify your Qt6 directory if not in PATH)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 3. Build the project
cmake --build build --config Release
```
Platform Configuration Notes
macOS: Point CMake to Homebrew's Qt prefix:
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
The build generates a self-contained .app bundle.
Windows (Visual Studio / Qt Creator): Ensure you point CMAKE_PREFIX_PATH to your compiler kit, for example:
cmake -B build -S . -DCMAKE_PREFIX_PATH=C:/Qt/6.7.0/msvc2022_64

Setting up Google API Access
The application accesses your Google AI Studio conversations using a Google Drive OAuth "Desktop app" client that you create in your own Google Cloud Console. Setting this up takes about 3 minutes:
Step 1: Create a Google Cloud Project & Enable Drive API
Open the Google Cloud Console.
In the top project dropdown, click New Project, name it AI Studio Viewer, and click Create.
In the left navigation menu, go to APIs & Services → Library.
Search for Google Drive API, select it, and click Enable.
Step 2: Configure the OAuth Consent Screen
(Google requires this before generating desktop credentials)
In the left navigation menu, go to APIs & Services → OAuth consent screen.
Select External as the User Type and click Create.
Fill in the required basic information: 
App name: Google AI Studio Viewer
User support email: Select your email address.
Developer contact email: Enter your email address.
Click Save and Continue.
Scopes: Click Add or Remove Scopes, check https://www.googleapis.com/auth/drive.readonly, click Update, and then Save and Continue.
Test Users (Crucial Step): 
Because your app is in "Testing" status, Google will reject sign-ins from any account not listed here with Error 403: access_denied.
Click + Add Users, enter the Google email address you use for Google AI Studio, and click Save and Continue.
Step 3: Create Desktop Client Credentials
In the left navigation menu, go to APIs & Services → Credentials.
Click + Create Credentials → OAuth client ID.
In the Application type dropdown, choose Desktop app.
Set the name to Google AI Studio Viewer Client and click Create.
A dialog will appear displaying your Client ID and Client Secret.
Step 4: First Sign-In
Launch the application and click Sign In with Google.
Paste the Client ID and Client Secret into the setup prompt. (The application saves these with owner-only read/write permissions to ~/Documents/AI/oauth_config.json so you only have to enter them once).
A browser window will open displaying Google's authorization screen.
Bypass the "Unverified App" Screen: Because you generated these credentials yourself in testing mode, Google will warn you that "Google hasn't verified this app". Click Advanced, then click Go to Google AI Studio Viewer (unsafe), and approve the read-only Drive permissions.
The browser will redirect to a temporary loopback port (127.0.0.1) managed by the app, complete the PKCE handshake, and sign you in.
The app will locate the "Google AI Studio" folder in your Drive, cache conversations into SQLite, and begin background synchronization.

File & Data Storage Locations
The application strictly separates configuration, database caching, and OS security credentials:
Item
Windows
Linux
macOS
Client Secrets
%USERPROFILE%\Documents\AI\oauth_config.json
~/Documents/AI/oauth_config.json
~/Documents/AI/oauth_config.json
OAuth Tokens
Windows Credential Manager
Secret Service / KWallet
macOS Keychain
SQLite Cache
%APPDATA%\GoogleAIViewer\viewer_cache.db
~/.local/share/GoogleAIViewer/viewer_cache.db
~/Library/Application Support/GoogleAIViewer/viewer_cache.db
Application Log
%APPDATA%\GoogleAIViewer\GoogleAIViewer.log
~/.local/share/GoogleAIViewer/GoogleAIViewer.log
~/Library/Application Support/GoogleAIViewer/GoogleAIViewer.log


Project Architecture
src/
├── api/
│   ├── ConversationModel.cpp/h        # Turn pairing, chunk parser, stable sort
│   ├── conversationsyncmanager.cpp/h  # Rate-limited background sync queue & backoff
│   ├── CopilotApi.cpp/h               # Local OpenAI-compatible REST chat client
│   ├── DriveApi.cpp/h                 # Google Drive API v3 requests & attachment fetcher
│   └── LocalCache.cpp/h               # SQLite storage, text search, attachment lookups
├── app/
│   ├── Application.cpp/h              # QSettings and global app initialization
│   ├── AuthManager.cpp/h              # OAuth 2.0 PKCE, loopback server, QtKeychain
│   └── Logger.cpp/h                   # Thread-safe file and stderr logger
├── models/
│   ├── Conversation.cpp/h             # Conversation struct and prompt-response pairs
│   ├── FileAttachment.cpp/h           # File attachment metadata
│   └── Message.cpp/h                  # Flat message chunks and inline image extractor
├── ui/
│   ├── ConversationListPane.cpp/h     # Left sidebar conversation list and sorting
│   ├── FilePreviewWidget.cpp/h        # Standalone attachment preview widget
│   ├── ImageViewerDialog.cpp/h        # Modal zoomable image viewer
│   ├── MainWindow.cpp/h               # Main window, toolbar, coordinator
│   ├── PromptListPane.cpp/h           # Middle pane displaying prompt turns
│   └── ResponsePane.cpp/h             # Right pane with markdown, attachments & thoughts
└── main.cpp                           # Entry point and AppData logger setup

A Note on How This Was Built
This project was built iteratively using AI-assisted programming ("vibe coding"). But moving from a basic prototype to a robust production tool required solving real engineering hurdles along the way:
Taming Google Drive API Quotas: Handling rate limits and 429 backoff gracefully during bulk downloads.
Solving the "Unindexed File" Problem: Discovering that Google Drive does not index .prompt JSON files, which led to building a client-side SQLite search engine.
De-scrambling Prompt Chunks: Replacing non-deterministic std::sort with std::stable_sort to maintain the chronological order of simultaneous thoughts and model text.
Cross-Platform Credential Security: Moving away from plaintext configuration files to OS-level secure credential storage using QtKeychain across Windows, Linux, and macOS.
Native OS Theming: Resolving subtle desktop bugs, like Windows 11 light-selection highlights clashing with default text palettes.

License
MIT License. See LICENSE for details.