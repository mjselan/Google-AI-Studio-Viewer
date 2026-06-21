@echo off
setlocal enabledelayedexpansion

set "vcvars=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "cmakeGen=Ninja"
set "ninja=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "qtPrefix=C:/Qt/6.11.1/msvc2022_64"
set "buildDir=build"
set "config=Debug"
set "target="

:parse
if "%~1"=="" goto :done
if /i "%~1"=="-Target" (set "target=%~2" & shift & shift & goto :parse)
if /i "%~1"=="-Config" (set "config=%~2" & shift & shift & goto :parse)
shift
goto :parse

:done
if not exist "%buildDir%\CMakeCache.txt" (
    set "cmakeConfigure=cmake -S . -B %buildDir% -G "%cmakeGen%" -DCMAKE_MAKE_PROGRAM="%ninja%" -DCMAKE_PREFIX_PATH="%qtPrefix%" -DCMAKE_BUILD_TYPE=%config%"
) else (
    set "cmakeConfigure=rem configure skipped"
)

set "cmakeBuild=cmake --build %buildDir%"
if defined target set "cmakeBuild=cmake --build %buildDir% --target %target%"

if not defined VSINSTALLDIR (
    call "%vcvars%" || exit /b 1
)

%cmakeConfigure%
if errorlevel 1 exit /b 1
%cmakeBuild%
if errorlevel 1 exit /b 1
