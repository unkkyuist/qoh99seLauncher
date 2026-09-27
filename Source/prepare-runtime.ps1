[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDir)
$ErrorActionPreference='Stop'
$projectDir=$PSScriptRoot
$parentDir=Split-Path $projectDir
& (Join-Path $projectDir 'prepare-shaders.ps1')
$dll=Join-Path $parentDir 'qoh-borderless-patch\vendor\ddraw.dll'
if(-not (Test-Path -LiteralPath $dll)) { $dll=Join-Path $parentDir 'ddraw.dll' }
$stream=[IO.File]::OpenRead($dll)
$sha=[Security.Cryptography.SHA256]::Create()
try { $dllHash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
finally { $sha.Dispose(); $stream.Dispose() }
if($dllHash -ne '85E0F7D530DFDA134793A57CB3E76B0287DCC96892EE57162DD68F47283B03A9') {
    throw 'Unexpected cnc-ddraw runtime. Expected official v7.1.0.0.'
}
$inputs=[ordered]@{'ddraw.dll'=$dll; 'ddraw.ini'=(Join-Path $projectDir 'distribution\ddraw.ini')}
$shaderDir=Join-Path $projectDir 'build\LauncherShaders'
$shaders=@('nearest-neighbor.glsl','interpolation/bilinear.glsl','xbr/xbr-lv2-noblend.glsl',
    'xbrz/xbrz-freescale-multipass.glsl','xbrz/xbrz-freescale-multipass.glsl.pass1','crt/crt-lottes-fast-no-warp-bilinear.glsl')
foreach($base in @(0,1,2,4)) { foreach($extra in @(1,2)) {
    $suffix=if($base -eq 1 -or $base -eq 4){'-bilinear'}else{''}
    $name="recipes/base$base-extra$extra$suffix.glsl"
    $shaders += $name,"$name.pass1"
} }
foreach($name in $shaders) { $inputs["LauncherShaders/$name"]=Join-Path $shaderDir $name }
$licenseDir=Join-Path $projectDir 'licenses'
if(-not (Test-Path -LiteralPath $licenseDir)) { $licenseDir=Join-Path $parentDir 'licenses' }
foreach($name in @('LICENSE-cnc-ddraw.txt','LICENSE-xBR-MIT.txt','LICENSE-xBRZ-NOTICES.txt',
    'GPL-3.0.txt','LICENSE-CRT-Unlicense.txt','LICENSE-FSR-MIT.txt','shader-package-readme.txt')) {
    $inputs["licenses/$name"]=Join-Path $licenseDir $name
}
$inputs['Launcher-LICENSE.txt']=Join-Path $projectDir 'LICENSE'
$inputs['Launcher-THIRD-PARTY-NOTICES.md']=Join-Path $projectDir 'THIRD-PARTY-NOTICES.md'
[IO.Directory]::CreateDirectory($OutputDir) | Out-Null
$rc=@('#include <windows.h>')
$header=@('#pragma once','struct PayloadFile { int id; const wchar_t* path; bool preserve; };','inline constexpr PayloadFile kPayload[] = {')
$id=201
foreach($name in $inputs.Keys) {
    $source=(Resolve-Path -LiteralPath $inputs[$name]).ProviderPath.Replace('\','/')
    $rc += "$id RCDATA `"$source`""
    $preserve=if($name -eq 'ddraw.ini'){'true'}else{'false'}
    $header += "    {$id, L`"$name`", $preserve},"
    $id++
}
$header += '};'
$utf8=New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllLines((Join-Path $OutputDir 'runtime_payload.rc'),$rc,$utf8)
[IO.File]::WriteAllLines((Join-Path $OutputDir 'runtime_payload.h'),$header,$utf8)
Write-Output "Embedded runtime prepared: $($inputs.Count) files"
