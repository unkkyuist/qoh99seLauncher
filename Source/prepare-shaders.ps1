[CmdletBinding()]
param([string]$SourcePath)

$ErrorActionPreference = 'Stop'
$projectDir = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not $SourcePath) {
    $SourcePath = Join-Path (Split-Path $projectDir) 'qoh-borderless-patch\vendor\Shaders'
    if (-not (Test-Path -LiteralPath $SourcePath -PathType Container)) {
        $SourcePath = Join-Path (Split-Path $projectDir) 'LauncherShaders'
    }
}
$sourceDir = (Resolve-Path -LiteralPath $SourcePath).ProviderPath
$outputDir = [IO.Path]::GetFullPath((Join-Path $projectDir 'build\LauncherShaders'))
$utf8 = New-Object Text.UTF8Encoding($false)

function Replace-Once([string]$Text, [string]$Before, [string]$After) {
    if ([regex]::Matches($Text, [regex]::Escape($Before)).Count -ne 1) {
        throw "Shader source changed; expected one occurrence of: $Before"
    }
    return $Text.Replace($Before, $After)
}

function Read-Shader([string]$RelativePath) {
    $path = Join-Path $sourceDir $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required shader is missing: $path"
    }
    return [IO.File]::ReadAllBytes($path)
}

# Collect and validate every input before replacing generated build files.
# Original shader bytes, including their embedded license notices, are preserved.
$files = @{}
$baseFiles = @{
    0 = 'nearest-neighbor.glsl'
    1 = 'interpolation\bilinear.glsl'
    2 = 'xbr\xbr-lv2-noblend.glsl'
    4 = 'crt\crt-lottes-fast-no-warp-bilinear.glsl'
}
$originals = @($baseFiles.Values) + @(
    'xbrz\xbrz-freescale-multipass.glsl',
    'xbrz\xbrz-freescale-multipass.glsl.pass1'
)
foreach ($relative in $originals) { $files[$relative] = Read-Shader $relative }

$postPasses = @{}
$hasRawEffects = (Test-Path -LiteralPath (Join-Path $sourceDir 'scanlines\scanline.glsl') -PathType Leaf) -and
    (Test-Path -LiteralPath (Join-Path $sourceDir 'sharpen\rca-sharpen.glsl') -PathType Leaf)
if ($hasRawEffects) {
    $scanline = $utf8.GetString((Read-Shader 'scanlines\scanline.glsl'))
    $scanline = Replace-Once $scanline `
        'omega = vec2(pi * OutputSize.x, 2.0 * pi * TextureSize.y);' `
        'omega = vec2(pi * OutputSize.x, pi * TextureSize.y);'
    $postPasses[1] = $utf8.GetBytes(
        "// QOH Launcher modification: output-row scanlines for cnc-ddraw second pass.`r`n" + $scanline)

    $sharpen = $utf8.GetString((Read-Shader 'sharpen\rca-sharpen.glsl'))
    $sharpen = Replace-Once $sharpen `
        'return COMPAT_TEXTURE(Source,p/OutputSize.xy);' `
        'return COMPAT_TEXTURE(Source, clamp(p, vec2(0.5), InputSize - vec2(0.5)) / TextureSize);'
    $sharpen = Replace-Once $sharpen `
        'vec2 fragCoord = vTexCoord.xy * OutputSize.xy;' `
        'vec2 fragCoord = vTexCoord.xy * TextureSize;'
    $postPasses[2] = $utf8.GetBytes(
        "// QOH Launcher modification: padded-texture texel sampling and input-edge clamping for cnc-ddraw second pass.`r`n" + $sharpen)
}

$recipePaths = @()
foreach ($base in @(0, 1, 2, 4)) {
    foreach ($extra in @(1, 2)) {
        # cnc-ddraw detects linear filtering by the literal substring bilinear.glsl.
        # Custom recipe names also keep effects enabled at native 1:1 size.
        $suffix = if ($base -eq 1 -or $base -eq 4) { '-bilinear' } else { '' }
        $recipe = "recipes\base$base-extra$extra$suffix.glsl"
        $pass1 = "$recipe.pass1"
        $files[$recipe] = $files[$baseFiles[$base]]
        if ($hasRawEffects) {
            $files[$pass1] = $postPasses[$extra]
        } else {
            # A portable source package can provide the already-prepared shaders.
            $bytes = Read-Shader $pass1
            $text = $utf8.GetString($bytes)
            if (-not $text.StartsWith('// QOH Launcher modification:')) {
                throw "Prepared second pass is missing its modification notice: $pass1"
            }
            $required = if ($extra -eq 1) {
                @('omega = vec2(pi * OutputSize.x, pi * TextureSize.y);')
            } else {
                @('vec2 fragCoord = vTexCoord.xy * TextureSize;',
                  'return COMPAT_TEXTURE(Source, clamp(p, vec2(0.5), InputSize - vec2(0.5)) / TextureSize);')
            }
            foreach ($token in $required) {
                if ([regex]::Matches($text, [regex]::Escape($token)).Count -ne 1) {
                    throw "Prepared second pass has an unexpected implementation: $pass1"
                }
            }
            $files[$pass1] = $bytes
        }
        $recipePaths += $recipe, $pass1
    }
}
if ($recipePaths.Count -ne 16 -or $files.Count -ne 22) {
    throw 'Shader preparation produced an unexpected file count.'
}

# Write only the explicit file set; never delete or replace shader directories.
foreach ($relative in ($files.Keys | Sort-Object)) {
    $target = Join-Path $outputDir $relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    [IO.File]::WriteAllBytes($target, $files[$relative])
    $actual = [IO.File]::ReadAllBytes($target)
    if ([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($files[$relative])) {
        throw "Shader verification failed: $target"
    }
}
Write-Output "Prepared and verified 6 original shaders and 16 recipe files: $outputDir"
