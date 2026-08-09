# Google AI Studio Viewer

A desktop application (Qt/C++) for browsing and reading [Google AI Studio](https://aistudio.google.com/) conversation exports stored in Google Drive. It authenticates with your Google account, lists conversation files from Drive, and renders each conversation as a readable list of prompt/response pairs — including inline images, file attachments, and model "thoughts."

## Features

- **Google Drive integration** — signs in via OAuth 2.0, locates your AI Studio conversations folder, and lists/searches conversation files.
- **Conversation viewer** — parses AI Studio's `chunkedPrompt` JSON format into chronological messages and pairs each user prompt with its model response(s).
- **Rich response rendering** — renders markdown-formatted model output, inline images (base64), and downloaded Drive attachments (images, text, JSON, etc.) directly in the response pane.
- **Full-text search** — debounced search across conversations stored in your Drive folder.
- **Local cache** — conversations are cached in a local SQLite database so previously viewed conversations load instantly and can be revisited offline.
- **Thoughts toggle** — optionally show or hide the model's intermediate "thought" chunks.
- **Optional local Copilot-style API client** — a small client for talking to an OpenAI-compatible local server (e.g. `http://localhost:8000`) for model listing/completions, separate from the Drive-based viewer flow.

## Architecture

The codebase is organized under `src/` by responsibility:

```
src/
├── api/            # Network & data access layer
│   ├── ConversationModel   # Parses raw Drive JSON into Conversation/Message objects, builds prompt/response pairs
│   ├── CopilotApi          # Client for a local OpenAI-compatible completions server
│   ├── DriveApi            # Google Drive v3 REST client (list, search, download conversations & attachments)
│   └── LocalCache          # SQLite-backed local cache of downloaded conversations
├── app/            # Application-level plumbing
│   ├── Application         # QApplication subclass, holds QSettings
│   ├── AuthManager          # Google OAuth 2.0 (loopback flow, token storage/refresh)
│   └── Logger               # File + console logging singleton
├── models/         # Plain data structures
│   ├── Conversation         # A conversation: run settings, messages, and derived prompt/response pairs
│   ├── FileAttachment       # Downloaded attachment metadata/content
│   └── Message               # A single chunk (user/model turn), parsed from AI Studio JSON
├── ui/             # Qt Widgets UI
│   ├── ConversationListPane  # Left-hand list of conversations from Drive
│   ├── FilePreviewWidget     # Preview pane for attachments
│   ├── ImageViewerDialog     # Full-size image viewer
│   ├── MainWindow            # Top-level window, wires together auth/API/UI
│   ├── PromptListPane         # List of prompts within the open conversation
│   ├── ResponsePane           # Renders a prompt/response pair (markdown, images, attachments)
│   └── SearchDialog            # Debounced search dialog over Drive conversations
└── main.cpp        # Entry point
```

### How a conversation loads

1. `AuthManager` obtains/refreshes a Google OAuth access token (loopback HTTP server for the auth-code redirect).
2. `DriveApi` locates the AI Studio conversations folder and lists/searches files in it.
3. Conversation JSON is downloaded from Drive (or read from `LocalCache` if already cached).
4. `ConversationModel::parseConversation` turns the raw `chunkedPrompt.chunks` array into a chronologically sorted list of `Message`s, then `buildPairs` groups them into `Conversation::PromptResponsePair`s (one user prompt → one or more model response chunks).
5. `MainWindow`/`ConversationListPane`/`PromptListPane`/`ResponsePane` present the parsed conversation, with `ResponsePane` rendering markdown, inline images, and on-demand attachment downloads via `DriveApi`.

## Requirements

- Qt (Widgets, Network, Sql modules) — Qt 5.15+ or Qt 6.x
- A C++17-capable compiler
- SQLite driver for Qt (`QSQLITE`), typically bundled with Qt
- A Google Cloud OAuth 2.0 **Desktop app** client (Client ID + Secret) with the `drive.readonly` scope enabled

> Note: this dump does not include the project's build file (`CMakeLists.txt` / `.pro`). Add one that includes the `Widgets`, `Network`, and `Sql` Qt modules and compiles all sources under `src/`.

## Configuration

The app expects an OAuth client config file (Google's standard "installed app" credentials JSON), loaded via `AuthManager::loadConfig()`:

```json
{
  "installed": {
    "client_id": "YOUR_CLIENT_ID.apps.googleusercontent.com",
    "client_secret": "YOUR_CLIENT_SECRET"
  }
}
```

By default the app requests the `https://www.googleapis.com/auth/drive.readonly` scope and performs the OAuth loopback flow on `127.0.0.1` (a random local port), matching a "Desktop app" OAuth client type in Google Cloud Console.

Tokens (access + refresh) are persisted via `QSettings` under the `GoogleAIViewer` organization/application name, and refreshed automatically on expiry or on `401` responses from Drive.

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

(Adjust to your actual build system if a qmake `.pro` file is used instead.)

## Running

1. Place your OAuth client credentials JSON somewhere accessible and point `AuthManager::loadConfig()` at it (or wire up `setCredentials()` with the client ID/secret directly).
2. Launch the built binary. On first run, a browser window opens for Google sign-in; after granting access, the app receives the auth code via a local callback and stores tokens for future sessions.
3. Once authenticated, the app locates your AI Studio conversations folder in Drive, lists conversations, and lets you open, search, and read them.

Logs are written to `GoogleAIViewer.log` next to the application binary.

## License

Add your license of choice here.
