# Google AI Studio Viewer

A native, high-performance desktop application (Qt6 / C++20) for browsing, searching, and organizing your [Google AI Studio](https://aistudio.google.com/) conversation history.

Google AI Studio saves prompts and chat sessions as JSON files inside a `Google AI Studio` folder in your Google Drive. However, the web UI lacks fast full-text search across past chats and scrolling through deep histories can be cumbersome. This app connects securely to your Drive via read-only OAuth, syncs your conversations to a local SQLite cache, and provides an instant three-pane native viewer with search, code highlighting, attachment previews, and dark mode support.

Runs natively on **Windows, macOS, and Linux**.

---

## Features

* **Three-Pane Desktop Layout:** Browse conversations on the left, inspect turn-by-turn user prompts in the middle, and read markdown-rendered model responses on the right.
* **Offline Full-Text Search:** Instantly search across conversation titles, user prompts, system instructions, and model responses backed by a local SQLite index.
* **Attachment Search & Parent Linking:** When "Search all attachments" is enabled, searches match text, code, and PDFs stored in Drive and automatically resolve them back to their parent conversation.
* **Inline Attachments & Code Previews:** Uploaded code snippets, text files, and images are previewed directly in the prompt area.
* **"Show Attachments" Toggle:** Easily collapse large code files (like project dumps or logs) into a compact badge to keep the conversation view clean.
* **Model "Thoughts" Inspection:** Toggle chain-of-thought reasoning blocks on/off, rendered with high-contrast styling in both Light and Dark themes.
* **Rate-Limited Background Sync:** Automatically keeps your local database in sync with Google Drive using request pacing, limited concurrency, and exponential backoff with jitter to stay well within Google API quotas.
* **Hardened OAuth 2.0 (PKCE):**
  * Strictly requests `drive.readonly` scope.
  * Implements RFC 7636 (PKCE with SHA-256) and validates CSRF `state` nonces against an ephemeral loopback callback server (`127.0.0.1`).
  * Stores access and refresh tokens in your OS's native secure vault via [QtKeychain](https://github.com/frankosterfeld/qtkeychain) (**Windows Credential Manager**, **macOS Keychain**, or **Linux Secret Service / KWallet**).
* **Optional Local LLM Backend:** Switch the toolbar from "Google AI Studio" to "Local Server" to chat with any OpenAI-compatible server (e.g., Ollama, LM Studio, llama.cpp server, or vLLM) running on `http://localhost:8000`.

---

## Requirements

* **CMake:** 3.27 or newer
* **C++ Compiler:** C++20 compliant (MSVC 2022, GCC 11+, or Clang 13+)
* **Qt6:** 6.5 or newer (Modules: `Core`, `Gui`, `Widgets`, `Network`, `Sql`)
* **Git:** Required at CMake configure time to fetch `qtkeychain`

### Platform-Specific Packages

* **Linux:** Requires Secret Service development libraries and a running keyring daemon:
  * **Debian / Ubuntu:**
    ```bash
    sudo apt install qt6-base-dev libsecret-1-dev
    ```
  * **Fedora:**
    ```bash
    sudo dnf install qt6-qtbase-devel libsecret-devel
    ```
  * **Arch Linux:**
    ```bash
    sudo pacman -S qt6-base libsecret
    ```
* **macOS:** Install Qt6 via Homebrew:
  ```bash
  brew install qt