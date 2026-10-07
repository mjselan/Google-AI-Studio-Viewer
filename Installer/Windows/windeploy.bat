@echo off
rem ============================================================================
rem windeploy.bat - Build Release and stage the Windows installer payload.
rem
rem Stages: Installer\Windows\packages\com.googleaiviewer.app\data\
rem
rem Environment overrides:
rem   QT_DIR                   Qt kit prefix, default C:\Qt\6.11.1\msvc2022_64 (local dev).
rem                            CI sets QT_DIR from QT_ROOT_DIR (Qt 6.10.1).
rem                            If QT_DIR is unset, QT_ROOT_DIR is used, then 6.11.1/6.10.1 fallbacks.
rem   GOOGLEAIVIEWER_VERSION   override version, default parsed from CMakeLists
rem   GOOGLEAIVIEWER_SKIP_BUILD=1  skip cmake build, stage an existing exe
rem   GOOGLEAIVIEWER_EXE       explicit exe to stage in skip mode
rem   VC_REDIST_ROOT           explicit MSVC redist root for CRT bundling
rem ============================================================================
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
rem Normalize once so no literal ..\.. leaks into search or display paths
pushd "%SCRIPT_DIR%..\.." >nul
set "ROOT=%CD%"
popd >nul
set "PKG_DIR=%ROOT%\Installer\Windows\packages\com.googleaiviewer.app"
set "DATA_DIR=%PKG_DIR%\data"
set "BUILD_DIR=%ROOT%\build\windows-release"
set "APP_EXE_NAME=GoogleAIViewer.exe"

rem --- Program Files roots, resolved before any parenthesized block ---------
set "PF=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"

rem --- Version: env override wins, else parse project VERSION ---------------
if defined GOOGLEAIVIEWER_VERSION (
    set "APP_VERSION=%GOOGLEAIVIEWER_VERSION%"
) else (
    call :ParseVersion
)
echo Google AI Studio Viewer version: !APP_VERSION!

rem --- Qt location: QT_DIR wins, else QT_ROOT_DIR (jurplel/install-qt-action), else local defaults
rem --- Local dev uses Qt 6.11, CI pins Qt 6.10 -> accept both.
if not defined QT_DIR (
    if defined QT_ROOT_DIR set "QT_DIR=!QT_ROOT_DIR!"
)
if not defined QT_DIR set "QT_DIR=C:\Qt\6.11.1\msvc2022_64"
if not exist "!QT_DIR!\bin\windeployqt6.exe" (
    if not exist "!QT_DIR!\bin\windeployqt.exe" (
        call :ResolveQtFallback
    )
)
if not exist "!QT_DIR!\bin\windeployqt6.exe" (
    if not exist "!QT_DIR!\bin\windeployqt.exe" (
        echo ERROR: no windeployqt found under "!QT_DIR!\bin"
        echo HINT: set QT_DIR or QT_ROOT_DIR to your Qt kit, for example C:\Qt\6.11.1\msvc2022_64
        echo HINT: CI example: QT_DIR^=D:\a\Google-AI-Studio-Viewer\Qt\6.10.1\msvc2022_64
        exit /b 1
    )
)

rem --- Build, unless skipped --------------------------------------------------
if /I "!GOOGLEAIVIEWER_SKIP_BUILD!"=="1" (
    echo SKIP_BUILD requested, skipping cmake build
    goto :Stage
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmake not on PATH
    exit /b 1
)
where ninja >nul 2>&1
if errorlevel 1 (
    if exist "!QT_DIR!\..\..\Tools\Ninja\ninja.exe" (
        set "PATH=!QT_DIR!\..\..\Tools\Ninja;!PATH!"
    )
)

if not defined VSCMD_VER (
    echo Initializing MSVC environment ...
    call :InitVcvars
    if not defined VSCMD_VER (
        echo ERROR: could not initialize MSVC, no vcvars64.bat found
        exit /b 1
    )
)

if not exist "!BUILD_DIR!\CMakeCache.txt" (
    echo Configuring Release build ...
    cmake -S "!ROOT!" -B "!BUILD_DIR!" -G Ninja "-DCMAKE_PREFIX_PATH=!QT_DIR!" -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 (
        echo ERROR: cmake configure failed
        exit /b 1
    )
) else (
    echo Reusing existing configure in "!BUILD_DIR!"
)

echo Building Release ...
cmake --build "!BUILD_DIR!" --config Release
if errorlevel 1 (
    echo ERROR: build failed
    exit /b 1
)

:Stage
rem --- Locate the exe ----------------------------------------------------------
set "SRC_EXE=!BUILD_DIR!\!APP_EXE_NAME!"
if defined GOOGLEAIVIEWER_EXE set "SRC_EXE=%GOOGLEAIVIEWER_EXE%"
if not exist "!SRC_EXE!" set "SRC_EXE=%ROOT%\out\build\release\!APP_EXE_NAME!"
if not exist "!SRC_EXE!" set "SRC_EXE=%ROOT%\build\!APP_EXE_NAME!"
if not exist "!SRC_EXE!" (
    echo ERROR: executable not found, expected one of:
    echo   !BUILD_DIR!\!APP_EXE_NAME!
    echo   %ROOT%\out\build\release\!APP_EXE_NAME!
    echo HINT: build first, or set GOOGLEAIVIEWER_EXE to the exe path
    exit /b 1
)
echo Staging from: !SRC_EXE!
for %%I in ("!SRC_EXE!") do set "SRC_DIR=%%~dpI"

rem --- Clean data dir and copy exe --------------------------------------------
if exist "!DATA_DIR!" rmdir /s /q "!DATA_DIR!"
mkdir "!DATA_DIR!"
copy /y "!SRC_EXE!" "!DATA_DIR!\" >nul
if errorlevel 1 (
    echo ERROR: failed to copy exe to data dir
    exit /b 1
)

rem --- Locate qt6keychain.dll in the build tree --------------------------------
set "KEYCHAIN_DLL="
if exist "!BUILD_DIR!" call :FindKeychain "!BUILD_DIR!"
if not defined KEYCHAIN_DLL (
    if exist "!SRC_DIR!" call :FindKeychain "!SRC_DIR!"
)
if not defined KEYCHAIN_DLL (
    if exist "%ROOT%\out" call :FindKeychain "%ROOT%\out"
)
if not defined KEYCHAIN_DLL (
    echo ERROR: qt6keychain.dll not found, cannot ship a working app
    exit /b 1
)
echo Found keychain dll: !KEYCHAIN_DLL!
copy /y "!KEYCHAIN_DLL!" "!DATA_DIR!\" >nul
if errorlevel 1 (
    echo ERROR: failed to copy keychain dll to data dir
    exit /b 1
)
if not exist "!DATA_DIR!\qt6keychain.dll" (
    echo ERROR: keychain dll missing from data dir after copy
    exit /b 1
)

rem --- Run windeployqt ----------------------------------------------------------
set "STAGED_EXE=!DATA_DIR!\!APP_EXE_NAME!"
set "WINDEPLOYQT=!QT_DIR!\bin\windeployqt6.exe"
if not exist "!WINDEPLOYQT!" set "WINDEPLOYQT=!QT_DIR!\bin\windeployqt.exe"

rem Temporarily satisfy windeployqt lookup next to the Qt binaries
set "QT_BIN_TEMP=0"
if not exist "!QT_DIR!\bin\qt6keychain.dll" (
    copy /y "!KEYCHAIN_DLL!" "!QT_DIR!\bin\qt6keychain.dll" >nul
    set "QT_BIN_TEMP=1"
)
echo Running windeployqt, this may take a minute ...
"!WINDEPLOYQT!" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "!STAGED_EXE!"
set "DEPLOY_RESULT=!ERRORLEVEL!"
if "!QT_BIN_TEMP!"=="1" del "!QT_DIR!\bin\qt6keychain.dll" >nul 2>&1
if not "!DEPLOY_RESULT!"=="0" (
    echo ERROR: windeployqt failed with code !DEPLOY_RESULT!
    exit /b 1
)
rem Drop the vc_redist bootstrapper, we bundle the CRT dlls instead
del "!DATA_DIR!\vc_redist*.exe" >nul 2>&1

rem --- Bundle the MSVC runtime, no redist installer needed ----------------------
call :StageCRT
if errorlevel 1 exit /b 1

echo.
echo Staged payload in:
echo   !DATA_DIR!
echo Version: !APP_VERSION!
echo Next step: call "%SCRIPT_DIR%make-installer.bat"
exit /b 0

rem ============================================================================
rem Subroutines
rem ============================================================================

:ResolveQtFallback
rem Try well-known local kit locations when the configured QT_DIR has no windeployqt.
rem QT_ROOT_DIR (CI) was already mapped above; this only covers local defaults.
for %%Q in (
    "C:\Qt\6.11.1\msvc2022_64"
    "C:\Qt\6.10.1\msvc2022_64"
) do (
    if exist "%%~Q\bin\windeployqt6.exe" (
        set "QT_DIR=%%~Q"
        echo Using Qt fallback: !QT_DIR!
        goto :eof
    )
    if exist "%%~Q\bin\windeployqt.exe" (
        set "QT_DIR=%%~Q"
        echo Using Qt fallback: !QT_DIR!
        goto :eof
    )
)
if defined USERPROFILE (
    for %%Q in (
        "%USERPROFILE%\Qt\6.11.1\msvc2022_64"
        "%USERPROFILE%\Qt\6.10.1\msvc2022_64"
    ) do (
        if exist "%%~Q\bin\windeployqt6.exe" (
            set "QT_DIR=%%~Q"
            echo Using Qt fallback: !QT_DIR!
            goto :eof
        )
        if exist "%%~Q\bin\windeployqt.exe" (
            set "QT_DIR=%%~Q"
            echo Using Qt fallback: !QT_DIR!
            goto :eof
        )
    )
)
goto :eof

:ParseVersion
set "APP_VERSION=1.0.0"
for /f "tokens=3 delims=) " %%V in ('findstr /R /C:"project(.*VERSION" "%ROOT%\CMakeLists.txt" 2^>nul') do set "APP_VERSION=%%V"
goto :eof

:InitVcvars
for %%M in (18 17) do (
    for %%E in (Community Professional Enterprise BuildTools) do (
        if not defined VSCMD_VER call :TryVcvars "!PF!\Microsoft Visual Studio\%%M\%%E"
        if not defined VSCMD_VER call :TryVcvars "!PF86!\Microsoft Visual Studio\%%M\%%E"
    )
)
goto :eof

:TryVcvars
if exist "%~1\VC\Auxiliary\Build\vcvars64.bat" (
    call "%~1\VC\Auxiliary\Build\vcvars64.bat" >nul
)
goto :eof

:FindKeychain
set "KEYCHAIN_DLL="
for /f "delims=" %%F in ('dir /s /b "%~1\qt6keychain.dll" 2^>nul') do (
    if not defined KEYCHAIN_DLL set "KEYCHAIN_DLL=%%F"
)
goto :eof

:StageCRT
call :FindCRTRoot
if not defined CRT_ROOT (
    echo WARNING: MSVC CRT not found, installer will require a vc_redist install
    echo HINT: set VC_REDIST_ROOT to your redist dir to bundle the CRT
    goto :eof
)
echo Bundling MSVC CRT from: !CRT_ROOT!
for %%D in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll) do (
    if exist "!CRT_ROOT!\%%D" (
        copy /y "!CRT_ROOT!\%%D" "!DATA_DIR!\" >nul
    ) else (
        echo WARNING: %%D missing in !CRT_ROOT!
    )
)
goto :eof

:FindCRTRoot
set "CRT_ROOT="
if defined VC_REDIST_ROOT call :CheckCRTRoot "%VC_REDIST_ROOT%"
if defined CRT_ROOT goto :eof
if defined VCToolsRedistDir call :CheckCRTRoot "%VCToolsRedistDir%"
if defined CRT_ROOT goto :eof
if exist "!PF86!\Microsoft Visual Studio\Installer\vswhere.exe" (
    call :TryVsWhere
    if defined CRT_ROOT goto :eof
)
for %%M in (18 17) do (
    for %%E in (Community Professional Enterprise BuildTools) do (
        if not defined CRT_ROOT call :TryVsRoot "!PF!\Microsoft Visual Studio\%%M\%%E"
        if not defined CRT_ROOT call :TryVsRoot "!PF86!\Microsoft Visual Studio\%%M\%%E"
    )
)
goto :eof

:TryVsWhere
set "VS_PATH="
for /f "usebackq delims=" %%P in (`"!PF86!\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%P"
if defined VS_PATH call :TryVsRoot "!VS_PATH!"
goto :eof

:TryVsRoot
if exist "%~1\VC\Redist\MSVC" call :NewestSubdir "%~1\VC\Redist\MSVC"
if defined NEWEST call :CheckCRTRoot "!NEWEST!"
set "NEWEST="
goto :eof

:CheckCRTRoot
if exist "%~1\vcruntime140.dll" (
    set "CRT_ROOT=%~1"
    goto :eof
)
if exist "%~1\x64\vcruntime140.dll" (
    set "CRT_ROOT=%~1\x64"
    goto :eof
)
if exist "%~1\x64\Microsoft.VC145.CRT\vcruntime140.dll" (
    set "CRT_ROOT=%~1\x64\Microsoft.VC145.CRT"
    goto :eof
)
if exist "%~1\x64\Microsoft.VC143.CRT\vcruntime140.dll" (
    set "CRT_ROOT=%~1\x64\Microsoft.VC143.CRT"
    goto :eof
)
goto :eof

:NewestSubdir
set "NEWEST="
set "NEWEST_VER=0"
for /d %%D in ("%~1\*") do (
    set "CAND=%%~nxD"
    call :IsVersion "!CAND!"
    if "!IS_VER!"=="1" (
        call :CmpVer "!CAND!" "!NEWEST_VER!"
        if "!CMP!"=="1" (
            set "NEWEST=%%D"
            set "NEWEST_VER=!CAND!"
        )
    )
)
goto :eof

:IsVersion
set "IS_VER=0"
if "%~1"=="" goto :eof
set "REST="
for /f "delims=0123456789. tokens=*" %%X in ("%~1") do set "REST=%%X"
if not defined REST set "IS_VER=1"
goto :eof

:CmpVer
set "CMP=0"
set "A1=0"
set "A2=0"
set "A3=0"
set "A4=0"
set "B1=0"
set "B2=0"
set "B3=0"
set "B4=0"
for /f "tokens=1-4 delims=." %%A in ("%~1") do (
    set "A1=%%A"
    set "A2=%%B"
    set "A3=%%C"
    set "A4=%%D"
)
for /f "tokens=1-4 delims=." %%A in ("%~2") do (
    set "B1=%%A"
    set "B2=%%B"
    set "B3=%%C"
    set "B4=%%D"
)
if "!A1!"=="" set "A1=0"
if "!A2!"=="" set "A2=0"
if "!A3!"=="" set "A3=0"
if "!A4!"=="" set "A4=0"
if "!B1!"=="" set "B1=0"
if "!B2!"=="" set "B2=0"
if "!B3!"=="" set "B3=0"
if "!B4!"=="" set "B4=0"
for %%N in (1 2 3 4) do (
    if "!A%%N!" GTR "!B%%N!" (
        set "CMP=1"
        goto :eof
    )
    if "!A%%N!" LSS "!B%%N!" (
        set "CMP=-1"
        goto :eof
    )
)
goto :eof
