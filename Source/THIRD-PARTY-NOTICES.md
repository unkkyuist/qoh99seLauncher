# QOH99 Launcher: third-party notices

This inventory records the notices found in the exact bundled files. It does not
assign a license to the icon or the original QOH game. Launcher-authored source
uses the destination repository's existing MIT LICENSE, included with this release.
The original game, video, sound, character data, and personal configuration are
not included in the launcher package.

## cnc-ddraw 7.1.0.0

`ddraw.dll` is the unmodified 32-bit DLL from the official release:

- <https://github.com/FunkyFr3sh/cnc-ddraw/releases/tag/v7.1.0.0>
- <https://github.com/FunkyFr3sh/cnc-ddraw/releases/download/v7.1.0.0/cnc-ddraw.zip>
- Source and license: <https://github.com/FunkyFr3sh/cnc-ddraw/tree/v7.1.0.0>
- DLL SHA256: `85e0f7d530dfda134793a57cb3e76b0287dcc96892ee57162dd68f47283b03a9`

Copyright (c) 2022 github.com/FunkyFr3sh. MIT License; the complete copyright,
permission, and warranty notices are in `licenses/LICENSE-cnc-ddraw.txt`.
Preserve these notices when redistributing the DLL.

## Shader sources

The `LauncherShaders` files came from the shader bundle shipped with that
cnc-ddraw release. The upstream project is
<https://github.com/libretro/glsl-shaders>. The original bundle notice is retained
as `licenses/shader-package-readme.txt`. The `.glsl` and `.glsl.pass1` files are
editable source code, including their original embedded notices.

| Files | Notice found in supplied source |
| --- | --- |
| `xbr/xbr-lv2-noblend.glsl` | Hyllian, copyright 2011–2016; MIT. Full notice in `licenses/LICENSE-xBR-MIT.txt`. |
| `xbrz/xbrz-freescale-multipass.glsl` and `.glsl.pass1` | Hyllian MIT notice and Zenju GPL version 3 notice with a MAME-specific linking exception. Full embedded notices in `licenses/LICENSE-xBRZ-NOTICES.txt`; GPL text in `licenses/GPL-3.0.txt`. Both source passes are included unchanged. |
| `crt/crt-lottes-fast-no-warp-bilinear.glsl` | Timothy Lottes, adapted for RetroArch by hunterk; Unlicense/public-domain dedication and warranty disclaimer. Complete header in `licenses/LICENSE-CRT-Unlicense.txt`. |
| `nearest-neighbor.glsl`, `interpolation/bilinear.glsl` | No separate license or copyright header in the supplied files; the bundle identifies Libretro as its source. No new license is inferred here. |
| `recipes/*extra1*.glsl.pass1` | Adapted from the bundle's `scanlines/scanline.glsl`, which has no separate license or author header. Provenance and the change are recorded below. |
| `recipes/*extra2*.glsl.pass1` | Adapted from `sharpen/rca-sharpen.glsl`; its header identifies a Shadertoy port under MIT. AMD's upstream FSR MIT notice is also included as `licenses/LICENSE-FSR-MIT.txt`. |

The xBRZ files retain the GPL notice and MAME exception exactly as received.
Keep both source passes and the GPL text together when sharing them. The
MAME-specific exception is not represented as a general permission for QOH.
This inventory does not resolve the scope of copyleft for any future combined
distribution. No additional restrictions are placed on the third-party shaders
by this document. Absence of a per-file license header is recorded as an open
provenance limitation, not treated as a public-domain declaration.

Relevant upstream locations:

- xBR: <https://github.com/libretro/glsl-shaders/tree/master/xbr>
- xBRZ: <https://github.com/libretro/glsl-shaders/tree/master/xbrz/shaders/xbrz-freescale-multipass>
- CRT author example: <https://www.shadertoy.com/view/MtSfRK>
- Scanlines: <https://github.com/libretro/glsl-shaders/tree/master/scanlines>
- RCAS port identified in the supplied header: <https://www.shadertoy.com/view/stXSWB>
- AMD FSR: <https://github.com/GPUOpen-Effects/FidelityFX-FSR>
- AMD license: <https://raw.githubusercontent.com/GPUOpen-Effects/FidelityFX-FSR/master/license.txt>
- GPL version 3: <https://www.gnu.org/licenses/gpl-3.0.html>
- Bundled verbatim GPL text retrieved from: <https://raw.githubusercontent.com/libretro/RetroArch/master/COPYING>

## Launcher shader adaptations, 2026-09-05

`Source/prepare-shaders.ps1` produces the recipe files. Their base `.glsl` files
are byte-for-byte copies of the relevant base shaders, with the same notices.
Each modified second pass begins with `QOH Launcher modification:`.

- Scanlines: changed the vertical frequency expression from
  `2.0 * pi * TextureSize.y` to `pi * TextureSize.y` for the second pass.
- RCAS: changed fragment-coordinate calculation to use `TextureSize` and clamps
  samples to the input edges before normalizing by `TextureSize`. This handles
  the padded intermediate textures used by cnc-ddraw.
- xBRZ's two passes are unchanged. No extra pass is appended to xBRZ.

The prepared source is supplied in `LauncherShaders`, and the preparation
script is included in `Source`. Starting with 0.2.4, the portable launcher also
embeds these source files, the unmodified cnc-ddraw DLL, and their license
notices as file resources. It extracts them beside the game before use; the
shader source is not compiled or linked into the launcher code. The complete
source package remains available alongside the EXE in the same release.

## Launcher source, icon, and Windows runtime

`Source` is included for inspection and rebuilding. Launcher-authored source is
distributed under the existing MIT LICENSE from `unkkyuist/qoh99seLauncher`,
with its original copyright notice preserved. Distribution credit: tikiland.
That license does not grant rights to the user-provided icon or original game.
Third-party components retain their separate licenses above.

The release launcher uses the statically linked Microsoft C++ runtime. Its
inspected import table contains only Windows DLLs: `COMCTL32.dll`, `bcrypt.dll`,
`SHELL32.dll`, `KERNEL32.dll`, `USER32.dll`, and `GDI32.dll`. No Microsoft runtime
redistributable installer or separate runtime DLL is bundled. This import audit
is not a claim that every Windows or graphics-driver configuration was tested.
