$ErrorActionPreference = "Stop"

$ndk = if ($env:ANDROID_NDK_HOME) {
    $env:ANDROID_NDK_HOME
} else {
    "C:\Users\29633\AppData\Local\Android\Sdk\ndk\29.0.14206865"
}
$sdkcmake = if ($env:ANDROID_CMAKE_DIR) {
    $env:ANDROID_CMAKE_DIR
} else {
    "C:\Users\29633\AppData\Local\Android\Sdk\cmake\4.1.2"
}
$cmake  = Join-Path $sdkcmake "bin\cmake.exe"
$ninja  = Join-Path $sdkcmake "bin\ninja.exe"
$toolchain = Join-Path $ndk "build\cmake\android.toolchain.cmake"

if (-not (Test-Path $cmake))     { Write-Error "cmake not found: $cmake" }
if (-not (Test-Path $ninja))     { Write-Error "ninja not found: $ninja" }
if (-not (Test-Path $toolchain)) { Write-Error "toolchain not found: $toolchain" }

$src   = $PSScriptRoot
$build = Join-Path $src "build"

& $cmake -S $src -B $build -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    -DANDROID_ABI=arm64-v8a `
    -DANDROID_PLATFORM=android-24 `
    -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $build -j 8
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Built: $(Join-Path $build 'libStructureLimitRemover.so')"
