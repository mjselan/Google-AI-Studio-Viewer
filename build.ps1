param(
    [string]$Target = "",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug"
)

$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
$cmakeGen = "Ninja"
$ninja = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$qtPrefix = "C:/Qt/6.9.3/msvc2022_64"
$buildDir = "build"

if (-not (Test-Path "$buildDir/CMakeCache.txt")) {
    $cmakeConfigure = "cmake -S . -B $buildDir -G `"$cmakeGen`" -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_PREFIX_PATH=`"$qtPrefix`" -DCMAKE_BUILD_TYPE=$Config"
} else {
    $cmakeConfigure = "@rem configure skipped"
}

$cmakeBuild = "cmake --build $buildDir"
if ($Target -ne "") {
    $cmakeBuild = "cmake --build $buildDir --target $Target"
}

$cmds = @(
    "`"$vcvars`" > nul",
    $cmakeConfigure,
    $cmakeBuild
)

cmd /c ($cmds -join " && ")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
