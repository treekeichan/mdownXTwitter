param(
    [string]$QtRoot = 'C:\Qt\6.11.0\mingw_64',
    [string]$CompilerRoot = 'C:\Qt\Tools\mingw1310_64',
    [switch]$Package,
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
$cmake = 'C:\Qt\Tools\CMake_64\bin\cmake.exe'
$ninja = 'C:\Qt\Tools\Ninja\ninja.exe'
$env:PATH = "$CompilerRoot\bin;$QtRoot\bin;" + $env:PATH
& $cmake -S $projectRoot -B $buildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$CompilerRoot/bin/g++.exe" "-DCMAKE_PREFIX_PATH=$QtRoot" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $cmake --build $buildDir --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
if (!$SkipTests) {
    & 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        foreach ($log in 'engine-tests.txt', 'ui-tests.txt') {
            $path = Join-Path $buildDir $log
            if (Test-Path -LiteralPath $path) { Get-Content -LiteralPath $path }
        }
        throw 'Tests failed'
    }
}
if ($Package) {
    $distDir = Join-Path $projectRoot 'dist'
    New-Item -ItemType Directory -Path $distDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDir 'mdowXTwitter.exe') -Destination $distDir -Force
    & "$QtRoot\bin\windeployqt.exe" --release --compiler-runtime --no-translations --no-opengl-sw --no-system-d3d-compiler (Join-Path $distDir 'mdowXTwitter.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
    $toolsDir = Join-Path $projectRoot 'tools'
    if (Test-Path -LiteralPath $toolsDir) {
        $distTools = Join-Path $distDir 'tools'
        New-Item -ItemType Directory -Path $distTools -Force | Out-Null
        Get-ChildItem -LiteralPath $toolsDir -File | Where-Object { $_.Extension -in '.exe', '.txt' } | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $distTools -Force
        }
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $distDir -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY.md') -Destination $distDir -Force
    Write-Host "Portable application: $distDir\mdowXTwitter.exe"
}
