@echo off
rem ============================================================================
rem make-installer.bat - Build the offline Windows installer with IFW.
rem
rem Produces: Installer\Windows\GoogleAIViewer-<version>-offline.exe
rem Run windeploy.bat first to stage the payload.
rem
rem Environment overrides:
rem   QT_DIR                   Qt kit prefix, default C:\Qt\6.11.1\msvc2022_64 (local dev).
rem                            CI sets QT_DIR from QT_ROOT_DIR (Qt 6.10.1).
rem   GOOGLEAIVIEWER_VERSION   override version, default parsed from CMakeLists
rem   IFW_BIN                  explicit binarycreator.exe path or its bin dir
rem ============================================================================
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%..\.."
set "PKG_DIR=%SCRIPT_DIR%packages\com.googleaiviewer.app"
set "DATA_DIR=%PKG_DIR%\data"
set "CONFIG=%SCRIPT_DIR%config\config.xml"

if defined GOOGLEAIVIEWER_VERSION (
    set "APP_VERSION=%GOOGLEAIVIEWER_VERSION%"
) else (
    call :ParseVersion
)
echo Google AI Studio Viewer version: !APP_VERSION!

if not defined QT_DIR (
    if defined QT_ROOT_DIR set "QT_DIR=!QT_ROOT_DIR!"
)
if not defined QT_DIR set "QT_DIR=C:\Qt\6.11.1\msvc2022_64"

if not exist "!DATA_DIR!\GoogleAIViewer.exe" (
    echo ERROR: payload not staged, "!DATA_DIR!\GoogleAIViewer.exe" missing
    echo HINT: run windeploy.bat first via call windeploy.bat
    exit /b 1
)
if not exist "!CONFIG!" (
    echo ERROR: installer config missing: "!CONFIG!"
    exit /b 1
)

call :FindBinaryCreator
if not defined BINARYCREATOR (
    echo ERROR: binarycreator.exe not found
    echo HINT: install Qt Installer Framework, or set IFW_BIN to its path
    exit /b 1
)
echo Using: !BINARYCREATOR!

set "OUT_EXE=%SCRIPT_DIR%GoogleAIViewer-!APP_VERSION!-offline.exe"
if exist "!OUT_EXE!" del "!OUT_EXE!"

"!BINARYCREATOR!" --offline-only -c "!CONFIG!" -p "!SCRIPT_DIR!packages" "!OUT_EXE!"
if errorlevel 1 (
    echo ERROR: binarycreator failed
    exit /b 1
)

echo.
echo Installer ready:
echo   !OUT_EXE!
exit /b 0

rem ============================================================================
rem Subroutines
rem ============================================================================

:ParseVersion
set "APP_VERSION=1.0.0"
for /f "tokens=3 delims=) " %%V in ('findstr /R /C:"project(.*VERSION" "%ROOT%\CMakeLists.txt" 2^>nul') do set "APP_VERSION=%%V"
goto :eof

:FindBinaryCreator
set "BINARYCREATOR="
if defined IFW_BIN call :CheckIfwBin "%IFW_BIN%"
if defined BINARYCREATOR goto :eof
if defined QT_DIR call :CheckIfwBin "!QT_DIR!\..\..\Tools\QtInstallerFramework"
if defined BINARYCREATOR goto :eof
if exist "C:\Qt\Tools\QtInstallerFramework" call :CheckIfwBin "C:\Qt\Tools\QtInstallerFramework"
if defined BINARYCREATOR goto :eof
for /f "delims=" %%B in ('where binarycreator.exe 2^>nul') do (
    if not defined BINARYCREATOR set "BINARYCREATOR=%%B"
)
goto :eof

rem %1 = IFW install root, a version dir, or a bin dir holding binarycreator
:CheckIfwBin
if exist "%~1\binarycreator.exe" (
    set "BINARYCREATOR=%~1\binarycreator.exe"
    goto :eof
)
if exist "%~1\bin\binarycreator.exe" (
    set "BINARYCREATOR=%~1\bin\binarycreator.exe"
    goto :eof
)
if exist "%~1\4.11\bin\binarycreator.exe" (
    set "BINARYCREATOR=%~1\4.11\bin\binarycreator.exe"
    goto :eof
)
if exist "%~1" call :NewestIfw "%~1"
goto :eof

:NewestIfw
set "BEST="
set "BEST_VER=0"
for /d %%D in ("%~1\4.*") do (
    set "CAND=%%~nxD"
    call :IsVersion "!CAND!"
    if "!IS_VER!"=="1" (
        if exist "%%D\bin\binarycreator.exe" (
            call :CmpVer "!CAND!" "!BEST_VER!"
            if "!CMP!"=="1" (
                set "BEST=%%D\bin\binarycreator.exe"
                set "BEST_VER=!CAND!"
            )
        )
    )
)
if defined BEST set "BINARYCREATOR=!BEST!"
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
set "B1=0"
set "B2=0"
set "B3=0"
for /f "tokens=1-3 delims=." %%A in ("%~1") do (
    set "A1=%%A"
    set "A2=%%B"
    set "A3=%%C"
)
for /f "tokens=1-3 delims=." %%A in ("%~2") do (
    set "B1=%%A"
    set "B2=%%B"
    set "B3=%%C"
)
if "!A1!"=="" set "A1=0"
if "!A2!"=="" set "A2=0"
if "!A3!"=="" set "A3=0"
if "!B1!"=="" set "B1=0"
if "!B2!"=="" set "B2=0"
if "!B3!"=="" set "B3=0"
for %%N in (1 2 3) do (
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
