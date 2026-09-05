[CmdletBinding()]
param([string]$Version = '0.2.2')

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Version must be a numeric major.minor.patch value.' }
$projectDir = [IO.Path]::GetFullPath($PSScriptRoot)
$parentDir = Split-Path $projectDir
$utf8 = New-Object Text.UTF8Encoding($false)

function Assert-Child([string]$Path, [string]$Root) {
    $full = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the intended folder: $full"
    }
    return $full
}

function Require-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required package input is missing: $Path" }
    return (Resolve-Path -LiteralPath $Path).ProviderPath
}

function Assert-PackagePath([string]$Relative) {
    if ($Relative.Contains('\') -or $Relative.StartsWith('/') -or $Relative -match '(^|/)\.\.?(/|$)|:') {
        throw "Invalid archive path: $Relative"
    }
    $leaf = ($Relative -split '/')[-1]
    if ($leaf -match '^(qoh99\.exe|Config\.exe|QOHcnf\.(key|ke)|opening\..*)$' -or
        $leaf -match '\.(avi|mp4|mkv|webm|mov|wmv|mpg|mpeg|wav|mp3|ogg|chr|img|bin)$') {
        throw "Original game or personal asset is forbidden in the package: $Relative"
    }
    if ($leaf -match '\.exe$' -and $Relative -cne 'QOH-Launcher.exe') { throw "Unexpected EXE: $Relative" }
    if ($leaf -match '\.dll$' -and $Relative -cne 'ddraw.dll') { throw "Unexpected DLL: $Relative" }
}

$inputs = [ordered]@{}
function Add-Input([string]$Relative, [string]$Path) {
    Assert-PackagePath $Relative
    if ($inputs.Contains($Relative)) { throw "Duplicate package path: $Relative" }
    $inputs[$Relative] = Require-File $Path
}

Add-Input 'QOH-Launcher.exe' (Join-Path $projectDir 'build\Release\QOH-Launcher.exe')
Add-Input 'ddraw.ini' (Join-Path $projectDir 'distribution\ddraw.ini')
$ddrawPath = Join-Path $parentDir 'qoh-borderless-patch\vendor\ddraw.dll'
if (-not (Test-Path -LiteralPath $ddrawPath -PathType Leaf)) { $ddrawPath = Join-Path $parentDir 'ddraw.dll' }
Add-Input 'ddraw.dll' $ddrawPath
if ((Get-FileHash -LiteralPath $inputs['ddraw.dll'] -Algorithm SHA256).Hash.ToLowerInvariant() -ne
    '85e0f7d530dfda134793a57cb3e76b0287dcc96892ee57162dd68f47283b03a9') {
    throw 'cnc-ddraw DLL does not match the documented official 7.1.0.0 build.'
}
foreach ($name in @('README.md', 'INSTALL.md', 'LICENSE', 'THIRD-PARTY-NOTICES.md')) {
    Add-Input $name (Join-Path $projectDir $name)
}

$shaderDir = Join-Path $projectDir 'build\LauncherShaders'
if (-not (Test-Path -LiteralPath $shaderDir -PathType Container)) { $shaderDir = Join-Path $parentDir 'LauncherShaders' }
$shaderPaths = @('nearest-neighbor.glsl', 'interpolation/bilinear.glsl',
    'xbr/xbr-lv2-noblend.glsl', 'xbrz/xbrz-freescale-multipass.glsl',
    'xbrz/xbrz-freescale-multipass.glsl.pass1', 'crt/crt-lottes-fast-no-warp-bilinear.glsl')
foreach ($base in @(0, 1, 2, 4)) {
    foreach ($extra in @(1, 2)) {
        $suffix = if ($base -eq 1 -or $base -eq 4) { '-bilinear' } else { '' }
        $recipe = "recipes/base$base-extra$extra$suffix.glsl"
        $shaderPaths += $recipe, "$recipe.pass1"
    }
}
foreach ($name in $shaderPaths) { Add-Input "LauncherShaders/$name" (Join-Path $shaderDir $name) }

$licenseDir = Join-Path $projectDir 'licenses'
if (-not (Test-Path -LiteralPath $licenseDir -PathType Container)) { $licenseDir = Join-Path $parentDir 'licenses' }
$licenseNames = @('LICENSE-cnc-ddraw.txt', 'LICENSE-xBR-MIT.txt', 'LICENSE-xBRZ-NOTICES.txt',
    'GPL-3.0.txt', 'LICENSE-CRT-Unlicense.txt', 'LICENSE-FSR-MIT.txt', 'shader-package-readme.txt')
foreach ($name in $licenseNames) { Add-Input "licenses/$name" (Join-Path $licenseDir $name) }

# Explicit inputs prevent accidental inclusion of the original game, saved keys,
# screenshots, build intermediates, or the icon's source/provenance manifest.
$sourceNames = @('main.cpp', 'native_keys.h', 'native_keys_test.cpp', 'filter_plan.h',
    'filter_plan_test.cpp', 'launcher.manifest', 'launcher.rc', 'CMakeLists.txt',
    'build.ps1', 'prepare-shaders.ps1', 'package.ps1', 'README.md', 'INSTALL.md', 'LICENSE', 'THIRD-PARTY-NOTICES.md')
foreach ($name in $sourceNames) { Add-Input "Source/$name" (Join-Path $projectDir $name) }
Add-Input 'Source/distribution/ddraw.ini' (Join-Path $projectDir 'distribution\ddraw.ini')
if (Test-Path -LiteralPath (Join-Path $projectDir 'make-icon.py') -PathType Leaf) {
    Add-Input 'Source/make-icon.py' (Join-Path $projectDir 'make-icon.py')
}
foreach ($name in @('icon.png', 'app.ico')) {
    $asset = Join-Path $projectDir "assets\$name"
    if (Test-Path -LiteralPath $asset -PathType Leaf) {
        Add-Input "assets/$name" $asset
        Add-Input "Source/assets/$name" $asset
    }
}

$releaseDir = Assert-Child (Join-Path $projectDir 'releases') $projectDir
$releaseZip = Assert-Child (Join-Path $releaseDir "QOH-Launcher-$Version.zip") $releaseDir
$manifestOut = "$releaseZip.manifest.json"
$checksumOut = "$releaseZip.sha256"
foreach ($path in @($releaseZip, $manifestOut, $checksumOut)) {
    if (Test-Path -LiteralPath $path) { throw "Release already exists; preserve it and select a new version: $path" }
}
$runName = 'package-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$stageDir = Assert-Child (Join-Path $projectDir "build\$runName") (Join-Path $projectDir 'build')
$contentDir = Assert-Child (Join-Path $stageDir 'content') $stageDir
[IO.Directory]::CreateDirectory($contentDir) | Out-Null

$entries = @()
foreach ($relative in ($inputs.Keys | Sort-Object)) {
    $source = $inputs[$relative]
    $hashBefore = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    $target = Assert-Child (Join-Path $contentDir $relative) $contentDir
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    [IO.File]::Copy($source, $target, $false)
    $hashCopied = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hashCopied -cne $hashBefore) { throw "File changed during staging: $relative" }
    $entries += [ordered]@{ path = $relative; bytes = (Get-Item -LiteralPath $target).Length; sha256 = $hashCopied }
}
$manifest = [ordered]@{
    package = 'QOH99 Launcher'; version = $Version; createdUtc = [DateTime]::UtcNow.ToString('o')
    files = $entries
    manifestScope = 'All payload files. MANIFEST.json and SHA256SUMS.txt are excluded from the self-referential payload list; the complete ZIP has an external SHA256.'
    excluded = @('Original game EXE', 'Config.exe', 'QOHcnf.key', 'video/audio/character assets', 'personal configuration', 'icon provenance manifest')
}
$manifestJson = $manifest | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText((Join-Path $contentDir 'MANIFEST.json'), $manifestJson, $utf8)
$checksumLines = @($entries | ForEach-Object { "$($_.sha256)  $($_.path)" })
[IO.File]::WriteAllText((Join-Path $contentDir 'SHA256SUMS.txt'), ($checksumLines -join "`n") + "`n", $utf8)

Add-Type -AssemblyName System.IO.Compression.FileSystem
$temporaryZip = Assert-Child (Join-Path $stageDir "QOH-Launcher-$Version.zip") $stageDir
[IO.Compression.ZipFile]::CreateFromDirectory($contentDir, $temporaryZip, [IO.Compression.CompressionLevel]::Optimal, $false)
$expected = @{}
foreach ($entry in $entries) { $expected[$entry.path] = $entry.sha256 }
foreach ($name in @('MANIFEST.json', 'SHA256SUMS.txt')) {
    $expected[$name] = (Get-FileHash -LiteralPath (Join-Path $contentDir $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$archive = [IO.Compression.ZipFile]::OpenRead($temporaryZip)
$seen = @{}
try {
    foreach ($entry in $archive.Entries) {
        Assert-PackagePath $entry.FullName
        if ($seen.ContainsKey($entry.FullName) -or -not $expected.ContainsKey($entry.FullName)) {
            throw "Unexpected or duplicate ZIP entry: $($entry.FullName)"
        }
        $stream = $entry.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose(); $stream.Dispose() }
        if ($hash -cne $expected[$entry.FullName]) { throw "ZIP content checksum failed: $($entry.FullName)" }
        $seen[$entry.FullName] = $true
    }
    if ($seen.Count -ne $expected.Count) { throw 'ZIP is missing expected entries.' }
} finally { $archive.Dispose() }

# Fail if an input changed while this archive was being constructed.
foreach ($entry in $entries) {
    if ((Get-FileHash -LiteralPath $inputs[$entry.path] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry.sha256) {
        throw "Package input changed; rebuild the package from a stable source: $($entry.path)"
    }
}
[IO.Directory]::CreateDirectory($releaseDir) | Out-Null
[IO.File]::Copy($temporaryZip, $releaseZip, $false)
$zipHash = (Get-FileHash -LiteralPath $releaseZip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($zipHash -cne (Get-FileHash -LiteralPath $temporaryZip -Algorithm SHA256).Hash.ToLowerInvariant()) {
    throw 'Published ZIP differs from the verified staging ZIP.'
}
[IO.File]::WriteAllText($manifestOut, $manifestJson, $utf8)
[IO.File]::WriteAllText($checksumOut, "$zipHash  $([IO.Path]::GetFileName($releaseZip))`n", $utf8)
Write-Output "Package: $releaseZip"
Write-Output "Verified: $($entries.Count) payload files plus 2 manifest/checksum files; no original game or personal assets."
Write-Output "SHA256: $zipHash"
Write-Output "Staging retained: $stageDir"
