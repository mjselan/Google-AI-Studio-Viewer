@echo off
setlocal

set QT_DIR=C:\Qt\6.11.1\msvc2022_64
set BUILD_DIR=%~dp0build
set WINDEPLOYQT=%QT_DIR%\bin\windeployqt6.exe

if not exist "%BUILD_DIR%" (
    echo Build directory not found. Run build first.
    exit /b 1
)

if not exist "%WINDEPLOYQT%" (
    echo windeployqt6.exe not found at %WINDEPLOYQT%
    exit /b 1
)

echo Running windeployqt6 on %BUILD_DIR%\GoogleAIViewer.exe ...
"%WINDEPLOYQT%" --no-translations "%BUILD_DIR%\GoogleAIViewer.exe"

echo Done.
