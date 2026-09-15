@echo off
setlocal
set "EXE_ARG=C:\Github\Google-AI-Studio-Viewer\out\build\release\GoogleAIViewer.exe"
set QT_DIR=C:\Qt\6.11.1\msvc2022_64
set BUILD_DIR=%~dp0out\build
set WINDEPLOYQT=%QT_DIR%\bin\windeployqt6.exe

if not exist "%BUILD_DIR%" (
    echo Build directory not found: %BUILD_DIR%
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
    )
)

if not exist "%TARGET_EXE%" (
    echo Executable not found: %TARGET_EXE%
    exit /b 1
)

for %%I in ("%TARGET_EXE%") do set "TARGET_DIR=%%~dpI"

:: 1. Copy qt6keychain.dll into the release directory
if not exist "%TARGET_DIR%qt6keychain.dll" (
    echo Locating qt6keychain.dll in build dependencies...
    for /r "%BUILD_DIR%" %%f in (qt6keychain.dll) do (
        if exist "%%f" (
            echo Copying %%f to %TARGET_DIR%
            copy /y "%%f" "%TARGET_DIR%" >nul
        )
    )
)

:: 2. Trick windeployqt by temporarily satisfying its lookup in %QT_DIR%\bin
copy /y "%TARGET_DIR%qt6keychain.dll" "%QT_DIR%\bin\qt6keychain.dll" >nul

:: 3. Run windeployqt
echo Running windeployqt6 on %TARGET_EXE% ...
"%WINDEPLOYQT%" --no-translations "%TARGET_EXE%"
set DEPLOY_RESULT=%errorlevel%

:: 4. Remove the temporary copy from the Qt SDK folder
del "%QT_DIR%\bin\qt6keychain.dll" >nul 2>&1

if %DEPLOY_RESULT% neq 0 (
    echo windeployqt6 failed with error %DEPLOY_RESULT%.
    exit /b 1
)

echo Done. All dependencies deployed successfully!