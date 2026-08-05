Google AI Studio Viewer
A high-performance C++/Qt desktop application for browsing, caching, searching, and managing Google AI Studio conversations directly synced from Google Drive. It also supports interactive chat via local OpenAI-compatible server endpoints (e.g., Local Copilot/LLM servers).
       

🌟 Key Features
Google Drive Sync & OAuth 2.0 Integration: 
Native desktop OAuth 2.0 authorization with an embedded loopback HTTP server (QTcpServer) for redirect handling (no deprecated OOB flows).
Automatic token persistence and non-blocking background refresh.
Smart Parsing & Conversation Structuring: 
Reconstructs raw JSON exports from Google AI Studio into structured, chronological user prompt–model response pairs.
Handles complex multi-turn conversations, system instructions, token count metrics, and thought process signatures.
Inline Media & Attachment Rendering: 
Previews text files, source code (<pre> formatted), and embedded/inline images directly in the response pane.
Interactive image viewer modal with zoom, fit-to-screen, 1:1 scale, and export capabilities.
Offline Caching (SQLite): 
Local persistence layer (QSqlDatabase) caches downloaded conversations to enable instant startup and offline reading.
Full-Text Drive Search: 
Integrated search dialog with debounced input querying Google Drive files across conversations.
Dual Backend Support: 
Switch seamlessly between remote Google AI Studio archives and local OpenAI-compatible endpoints (/v1/chat/completions).
Rich Text & Markdown Rendering: 
Pre-processes Markdown (strikethroughs, checklist items, code blocks) into clean, styled HTML for rendering inside Qt text documents.

🛠️ Architecture & Technical Highlights
This project was built from the ground up prioritizing modern C++ practices, clean architecture, and responsive desktop UI design.
Project Structure
GoogleAIViewer/
├── src/
│   ├── api/              # Network & Storage Services
│   │   ├── ConversationModel.cpp/h  # JSON parser & message pairing logic
│   │   ├── CopilotApi.cpp/h         # Local LLM endpoint client
│   │   ├── DriveApi.cpp/h           # Google Drive v3 REST client
│   │   └── LocalCache.cpp/h         # SQLite database management layer
│   ├── app/              # Core Application & Infrastructure
│   │   ├── Application.cpp/h        # QApp subclass & global settings
│   │   ├── AuthManager.cpp/h        # OAuth 2.0 client & HTTP loopback server
│   │   └── Logger.cpp/h             # Thread-safe file & console logger
│   ├── models/           # Data Structures
│   │   ├── Conversation.h           # Represents a full conversation thread
│   │   ├── Message.cpp/h            # Individual user/model prompt structure
│   │   └── FileAttachment.h         # Attachment payload representation
│   ├── ui/               # Qt Graphical Interface
│   │   ├── ConversationListPane.cpp/h # Conversation sidebar list
│   │   ├── PromptListPane.cpp/h       # Prompt selection panel
│   │   ├── ResponsePane.cpp/h         # HTML/Markdown chat viewer
│   │   ├── FilePreviewWidget.cpp/h    # Inline widget for previewing files
│   │   ├── ImageViewerDialog.cpp/h    # Fullscreen image inspection modal
│   │   ├── SearchDialog.cpp/h         # Drive search dialog
│   │   └── MainWindow.cpp/h           # Main UI container & orchestration
│   └── main.cpp
├── CMakeLists.txt        # Build configuration
└── README.md
Engineering Highlights (For Code Reviewers)
Asynchronous I/O & Non-Blocking UI: All network transactions (QNetworkAccessManager) and database queries operate asynchronously to ensure the UI thread remains 60 FPS responsive.
Memory Management & RAII: Strict object ownership hierarchy utilizing Qt's parent-child system, std::unique_ptr, and RAII memory cleanup.
Explicit Signal/Slot Connection Lifecycle: Network callbacks use explicit QMetaObject::Connection tracking to prevent dangling lambdas or memory leaks across view refresh cycles.
Timezone-Robust Data Sorting: Message parsing and UI list ordering utilize UTC epoch millisecond metrics (toMSecsSinceEpoch()) to avoid timezone offsets causing misordered conversation histories.
Security & OAuth Compliance: Implements a dedicated local HTTP server (http://127.0.0.1:<port>/callback) for Google OAuth 2.0 PKCE/Loopback authorization. Query string parameters are escaped to avoid injection issues.

📋 Prerequisites
C++ Compiler: C++17 support required (MSVC 2019+, GCC 9+, Clang 10+).
Qt Framework: Qt 5.15+ or Qt 6.x (Modules required: Core, Gui, Widgets, Network, Sql).
CMake: Version 3.16 or higher.

⚙️ Building the Application
Using CMake
Clone the repository:
git clone https://github.com/your-username/google-ai-studio-viewer.git
cd google-ai-studio-viewer
Configure and build:
mkdir build && cd build
cmake ..
cmake --build . --config Release
Run the executable:
./GoogleAIViewer

🔑 Google Drive API Setup
To sync your Google AI Studio conversations directly from Google Drive:
Go to the Google Cloud Console.
Create a project and enable the Google Drive API.
Configure the OAuth consent screen (add test users or publish the app).
Create credentials for a Desktop Application and download the client_id and client_secret.
Place your OAuth configuration at Documents/AI/oauth_config.json in the following format:
{
  "installed": {
    "client_id": "YOUR_CLIENT_ID.apps.googleusercontent.com",
    "client_secret": "YOUR_CLIENT_SECRET"
  }
}
Alternatively, the application will prompt you for your Client ID and Client Secret on your first sign-in.

🖥️ Usage
Sign In: Click "Sign In with Google" in the toolbar. A browser window will open to authorize access to your Google Drive (drive.readonly scope).
Browse Conversations: Once authenticated, the app automatically finds your Google AI Studio folder in Drive and populates the conversation sidebar.
View & Search: Select a conversation to view its prompt history. Use the Search tool to search across all archived conversations.
Local Copilot Server Mode: Switch the backend dropdown to "Local Server (Copilot)" to chat directly with a local OpenAI-compatible endpoint (defaults to http://localhost:8000).

📄 License
Distributed under the MIT License. See LICENSE for more information.

👤 Author
Your Name
GitHub: @your-username
LinkedIn: linkedin.com/in/your-profile
Email: your.email@example.com