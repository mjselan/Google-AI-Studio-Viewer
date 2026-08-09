cmake_minimum_required(VERSION 3.27)
project(GoogleAIViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/Logger.cpp
    src/app/AuthManager.cpp
    src/api/DriveApi.cpp
    src/api/CopilotApi.cpp
    src/api/ConversationModel.cpp
    src/api/LocalCache.cpp
    src/models/Conversation.cpp
    src/models/Message.cpp
    src/models/FileAttachment.cpp
    src/ui/MainWindow.cpp
    src/ui/ConversationListPane.cpp
    src/ui/PromptListPane.cpp
    src/ui/ResponsePane.cpp
    src/ui/FilePreviewWidget.cpp
    src/ui/ImageViewerDialog.cpp
)

set(HEADERS
    src/app/Application.h
    src/app/Logger.h
    src/app/AuthManager.h
    src/api/DriveApi.h
    src/api/CopilotApi.h
    src/api/ConversationModel.h
    src/api/LocalCache.h
    src/models/Conversation.h
    src/models/Message.h
    src/models/FileAttachment.h
    src/ui/MainWindow.h
    src/ui/ConversationListPane.h
    src/ui/PromptListPane.h
    src/ui/ResponsePane.h
    src/ui/FilePreviewWidget.h
    src/ui/ImageViewerDialog.h
)

qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    ${SOURCES}
    ${HEADERS}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "GoogleAIViewer"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.googleai.viewer")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

qt_finalize_executable(${PROJECT_NAME})