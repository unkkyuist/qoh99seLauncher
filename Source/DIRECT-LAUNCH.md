# 0.2.6 direct launch compatibility

Supported canonical SHA-256 identities are the two values listed in README.md.
Identification restores only exact known patch byte sequences in a temporary byte
buffer, then hashes the entire executable. Unknown content is never patched.

Both supported fixed-base PE32 images have identical startup instructions at file
offset 0x719a2 (VA 0x4719a2): load the active profile pointer from 0x613de0 and
preserve the existing zero write at profile+0x50. After LocalConfig/System loading,
an eight-byte jump replaces these instructions and returns to 0x4719aa.
The twenty-byte stub in zero-filled .text raw padding at 0x98900 executes the
original instructions and writes DWORD 1 to profile+0x6c. The .text virtual size
is extended to 0x97914 so the stub is inside the executable section. File size,
section raw offsets and game data are unchanged. Actual presentation remains
controlled by cnc-ddraw in the same game directory.

Esc's jump-table entry at 0x70e60 changes from VA 0x470e0f (DestroyWindow path)
to VA 0x470b0f (message-handler epilogue). Both target signatures are checked.
Unchecking Esc and saving restores the original entry. A canonical original EXE
is kept under LauncherBackup/direct-launch/<canonical hash>/<EXE filename>.
Restoring it removes both patches. Keep ddraw.dll installed while using the
patched EXE; restoring only the DLL changes the display behavior.

The netplay button passes exactly `net`, matching the supplied shortcut. It does
not change firewall rules or network configuration.

Evidence boundary: instruction bytes and original file identities were read from
both local EXEs; the launcher builds with MSVC. Actual gameplay, post-Config
direct-launch rendering, Esc key behavior and two-PC netplay are not yet verified.
