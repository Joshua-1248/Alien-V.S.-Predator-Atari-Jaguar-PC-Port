# Windows-native PC build (R284)

The Windows executable `AVP_Jaguar_PC.exe` is built from the same source as the
Linux build. The game logic, Jaguar GPU reconstruction, ROM check, input
bindings and renderer are shared by both builds. Only the host layer differs:

- Window and pixels: Win32 + GDI.
- Audio: WinMM waveOut, 20774 Hz stereo FullSynth.
- Controllers: XInput, up to four, with stick axes and axis-as-button bindings.
- ROM hashing: the system BCrypt SHA-256 provider (no OpenSSL).
- High-resolution raster workers: native Win32 threads (Linux uses pthreads).

## Status (R284)

- Cross-compiled here with MinGW-w64 (GCC 13, strict `-Wall -Wextra -Werror`).
- The executable is self-contained. libgcc, libstdc++, winpthread and zlib are
  linked statically, so it imports only system DLLs: kernel32, user32, gdi32,
  winmm, xinput9_1_0, bcrypt and msvcrt.
- Tested under Wine 9 with the retail ROM:
  - ROM SHA-256 verified.
  - Intro, title, character select and briefing screens ran.
  - Gameplay ran with keyboard input.
  - F5 quicksave and F9 quickload restored the level and position.
- **Not yet tested on a physical Windows PC.** Still unchecked there: audio
  device output, XInput hardware, and maximised/fullscreen behaviour.

## Using it

1. Put your own retail `AVP.jag` in the same folder as `AVP_Jaguar_PC.exe`,
   or run `AVP_Jaguar_PC.exe --rom "C:\path\to\AVP.jag"`.
2. Keep the folder somewhere writable, not Program Files. The game writes these
   files beside the executable:
   - `settings.cfg`
   - `avp_eeprom.bin` (the original Jaguar save slots)
   - the ROM-derived `cache\` folder
   - the log file

The ROM, ROM-derived assets and personal saves are never included in releases.

## Quicksave on Windows

F5/F9 session quicksave works on Windows. `cmake/avp_game_state_pe.ld` collects
the writable data and bss of `libavp_pc_core.a` into one page-aligned PE
section, `.avpgs`. This is the same rule the ELF script
`cmake/avp_game_state.ld` uses on Linux.

The durable quicksave file (`avp_quickstate.bin`, which survives restarting the
game) is still Linux-only. To restore a saved state, it relocates stale native
pointers using rules that depend on the Linux address-space layout. On Windows,
heap and image addresses do not follow that layout, so the same rules could
silently keep a stale pointer. The Windows build therefore does not write or
read that file. The original Jaguar EEPROM save slots work fully.

## Building

### Cross-compile from Linux (as done for releases)

```
sudo apt-get install mingw-w64 cmake wine    # wine is optional (runtime checks)
# zlib for MinGW: the static libz.a must be in /usr/x86_64-w64-mingw32/lib
bash tools/build_windows_cross.sh            # -> dist/windows/AVP_Jaguar_PC.exe
```

The script runs these checks:

1. It builds with the toolchain file `cmake/mingw-w64-x86_64.cmake`.
2. It rejects any non-system DLL import.
3. It rejects any PE section that is not aligned to SectionAlignment, because
   Windows refuses to load such an image.
4. When Wine is installed, it runs `--help` and the PE quickstate round-trip
   test `tests/test_quickstate_pe_r284.c`.
5. It writes a stripped executable and a README.

### Native build on Windows with MSYS2 UCRT64

Open the MSYS2 **UCRT64** shell and run:

```
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-zlib
bash tools/package_windows_ucrt64.sh build-win dist/windows
```

`.github/workflows/windows-release.yml` does the same on a GitHub Windows
runner, builds and tests Linux, and publishes a combined archive. That workflow
has not been run in this session.

## Windows-specific notes for maintainers

- `winnt.h` declares an enumerator named `ScreenOff`, which collides with the
  original game routine `void ScreenOff(void)`. Every `<windows.h>` include in
  shared code wraps it as follows:
  `#define ScreenOff avp_winnt_ScreenOff` / `#include <windows.h>` / `#undef ScreenOff`
- The historical oracle allocator libraries (`avp_unzip_*`, `avp_rom_*`) are
  built with `-fno-builtin` on Windows. Their `malloc(unsigned long)` prototype
  differs from the C library's on LLP64. They are not linked into the game.
