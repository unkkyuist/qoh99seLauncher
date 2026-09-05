param([switch]$Deploy)
$ErrorActionPreference = 'Stop'
$projectDir = $PSScriptRoot
cmake -S $projectDir -B "$projectDir\build" -G 'Visual Studio 18 2026' -A Win32
if ($LASTEXITCODE) { throw 'CMake configure failed' }
cmake --build "$projectDir\build" --config Release
if ($LASTEXITCODE) { throw 'C++ build failed' }
ctest --test-dir "$projectDir\build" -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed' }
& "$projectDir\prepare-shaders.ps1"
if ($Deploy) {
    $gameDir = Join-Path (Split-Path $projectDir) 'Queen of Heart 99 SE'
    if (-not (Test-Path -LiteralPath "$gameDir\qoh99.exe")) { throw 'Original game not found' }
    Copy-Item -LiteralPath "$projectDir\build\Release\QOH-Launcher.exe" -Destination "$gameDir\QOH-Launcher.exe"
    $shaderSource = Join-Path $projectDir 'build\LauncherShaders'
    New-Item -ItemType Directory -Force -Path "$gameDir\LauncherShaders" | Out-Null
    foreach ($entry in (Get-ChildItem -LiteralPath $shaderSource)) {
        Copy-Item -LiteralPath $entry.FullName -Destination "$gameDir\LauncherShaders" -Recurse -Force
    }
    Copy-Item -LiteralPath "$projectDir\README.md" -Destination "$gameDir\Launcher-README.md"
    Write-Output "Ready: $gameDir\QOH-Launcher.exe"
}
