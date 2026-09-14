@echo off
setlocal

set QT_DIR=C:\Qt\6.11.1\msvc2022_64
set BUILD_DIR=%~dp0build
set WINDEPLOYQT=%QT_DIR%\bin\windeployqt6.exe

rem Optional first argument: full path to the exe, a folder containing it, or Release / Debug
set "EXE_ARG=%~1"

if not exist "%BUILD_DIR%" (
    echo Build directory not found: %BUILD_DIR%
    echo Run build first.
    exit /b 1
)

if not exist "%WINDEPLOYQT%" (
    echo windeployqt6.exe not found at %WINDEPLOYQT%
    exit /b 1
)

set "TARGET_EXE="

if defined EXE_ARG (
    if exist "%EXE_ARG%\GoogleAIViewer.exe" (
        set "TARGET_EXE=%EXE_ARG%\GoogleAIViewer.exe"
    ) else if exist "%EXE_ARG%" (
        set "TARGET_EXE=%EXE_ARG%"
    ) else if /i "%EXE_ARG%"=="Release" (
        set "TARGET_EXE=%BUILD_DIR%\Desktop_Qt_6_11_1_MSVC2022_64bit_Release\GoogleAIViewer.exe"
    ) else if /i "%EXE_ARG%"=="Debug" (
        set "TARGET_EXE=%BUILD_DIR%\Desktop_Qt_6_11_1_MSVC2022_64bit_Debug\GoogleAIViewer.exe"
    ) else (
        echo Unknown argument or path not found: %EXE_ARG%
        echo Usage: %~nx0 [path-to-exe ^| Release ^| Debug]
        exit /b 1
    )
) else (
    rem Auto-detect: plain build dir first, then Qt Creator config dirs
    if exist "%BUILD_DIR%\GoogleAIViewer.exe" (
        set "TARGET_EXE=%BUILD_DIR%\GoogleAIViewer.exe"
    ) else if exist "%BUILD_DIR%\Desktop_Qt_6_11_1_MSVC2022_64bit_Release\GoogleAIViewer.exe" (
        set "TARGET_EXE=%BUILD_DIR%\Desktop_Qt_6_11_1_MSVC2022_64bit_Release\GoogleAIViewer.exe"
    ) else if exist "%BUILD_DIR%\Desktop_Qt_6_11_1_MSVC2022_64bit_Debug\GoogleAIViewer.exe" (
        set "TARGET_EXE=%BUILD_DIR%\Desktop_Qt_6_11_1_MSVC2022_64bit_Debug\GoogleAIViewer.exe"
    )
)

if not defined TARGET_EXE (
    echo GoogleAIViewer.exe not found under %BUILD_DIR%
    echo Build the project first, or pass the exe path: %~nx0 path\to\GoogleAIViewer.exe
    exit /b 1
)

if not exist "%TARGET_EXE%" (
    echo Executable not found: %TARGET_EXE%
    exit /b 1
)

echo Running windeployqt6 on %TARGET_EXE% ...
"%WINDEPLOYQT%" --no-translations "%TARGET_EXE%"
if errorlevel 1 (
    echo windeployqt6 failed with error %errorlevel%.
    exit /b 1
)

echo Done.
