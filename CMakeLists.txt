cmake_minimum_required(VERSION 3.27)
project(GoogleAIViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/Logger.cpp
    src/app/AuthManager.cpp
    src/api/DriveApi.cpp
    src/api/CopilotApi.cpp
    src/api/ConversationModel.cpp
    src/models/Conversation.cpp
    src/models/Message.cpp
    src/models/FileAttachment.cpp
    src/ui/MainWindow.cpp
    src/ui/ConversationListPane.cpp
    src/ui/PromptListPane.cpp
    src/ui/ResponsePane.cpp
    src/ui/FilePreviewWidget.cpp
    src/ui/ImageViewerDialog.cpp
    src/ui/SearchDialog.cpp
)

set(HEADERS
    src/app/Application.h
    src/app/Logger.h
    src/app/AuthManager.h
    src/api/DriveApi.h
    src/api/CopilotApi.h
    src/api/ConversationModel.h
    src/models/Conversation.h
    src/models/Message.h
    src/models/FileAttachment.h
    src/ui/MainWindow.h
    src/ui/ConversationListPane.h
    src/ui/PromptListPane.h
    src/ui/ResponsePane.h
    src/ui/FilePreviewWidget.h
    src/ui/ImageViewerDialog.h
    src/ui/SearchDialog.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS}
    src/app/patch.diff)
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
