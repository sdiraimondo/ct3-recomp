# Crazy Taxi 3 Decompilation & Recompilation for Linux

An unofficial effort to decompile and statically recompile **Crazy Taxi 3** (Win32 port) into native x86-64 Linux code, with a small Win32 runtime and Direct3D 9 rendering through [dxvk-native](https://github.com/doitsujin/dxvk).

> [!IMPORTANT]
> **This repository contains no game code or assets.** You need your own legally obtained copy of Crazy Taxi 3 for PC. Everything derived from the game (the translated C code, the compiled library and the game data) is generated or read locally from your own copy and must never be committed, uploaded or shared here.

## Status

**The game boots, shows its menus and can be driven in-game** on Linux, with Direct3D 9 rendering through Vulkan, sound and a gamepad. It is not finished: several known bugs remain (see the roadmap). The development log is in French: [DECISIONS.md](DECISIONS.md) (one entry per decision) and [docs/TODO.md](docs/TODO.md).

| Phase | Goal | Status |
|---|---|---|
| 1 | Translate every function of `CT3.exe` (Ghidra p-code export, then C) | ✅ Done |
| 2 | Generated code and runtime compile and link | ✅ Done |
| 3 | Boot test: the game allocates memory, opens its files and loads its data | ✅ Done |
| 4 | Direct3D 9 through dxvk-native, window through SDL2 | ✅ Done: menus and in-game rendering |
| 5 | Input: keyboard (DirectInput) and gamepad (SDL) | ✅ First pass: Xbox layout, PlayStation controllers map 1:1 |
| 6 | Sound: DirectSound mixed to SDL audio | ✅ First pass: sound effects and in-game music. Movies are silent |
| 7 | Known issues | ⚠️ See the roadmap |

## Implemented features

- **Startup in about 10 seconds** (against about 45 before): the game's image filter and several texture routines run as native code, checked bit for bit against the translated version.
- **Loading screen video**: `avi/loading.avi` plays full screen on a black background while the game starts, decoded with FFmpeg. The game window stays hidden until the video ends. Needs FFmpeg (optional).
- **Settings in `Config.ini`** (see below): skip the logo sequence, 16:9 or 4:3 aspect, field of view, map draw distance, interface texture fix.
- **Gamepad** through SDL, with a mapping read from `Config.ini`, and **sound** through SDL audio (sound effects and in-game music).

## Roadmap

1. **In-game slowdowns.** Frame-time spikes during driving and camera changes. Next: measure them with DXVK's shader compilation log, then decide whether to precompile shaders at startup.
2. **Startup time.** The first frame now appears about 10 seconds after launch. The remaining cost is mostly the DXT color block encoder, still translated.
3. **Intro cinematics.** Still buggy. Movies do not play (the DirectShow playback is a stub that ends immediately), and some character polygons are wrong in the LV and SA cinematics.
4. **Crashes.** A crash after about 20 to 30 minutes of play (memory fault in the game's object code, cause not yet found) and a crash when quitting the game.
5. **Settings.** Window resolution and borderless mode in `Config.ini`, then the settings editable from the game's Options menu.

## How it works

1. **Ghidra export.** A headless Ghidra run (`tools/ghidra/ExportFunctions.java`) exports the p-code of every function of `CT3.exe`, including the targets of bounded jump tables.
2. **Translation.** `tools/recomp/ct3rc.py` turns the p-code into C. The generated code is compiled into a static library, `build/g3/libct3gen.a`. x87 floating-point helpers live in `runtime/include/rt_fp.h`.
3. **Runtime.** `runtime/` provides the Win32 side: guest memory and heap, file access, the window (SDL2), DirectInput keyboard, DirectSound (mixed to SDL audio), the gamepad mapping read from `Config.ini`, and a fake DirectShow graph for the movies. Direct3D 9 calls are forwarded to dxvk-native, which renders them through Vulkan.
4. **Unknown addresses.** A call into code that was not translated is reported in the log with its return address, instead of silently continuing.

Everything in `build/` is derived from your copy of the game and is not versioned.

## Building and running

Requirements:
- Linux x86-64 with a Vulkan 1.3 driver (tested with Mesa RADV)
- CMake ≥ 3.20, Ninja, clang, Python 3
- JDK 21 and [Ghidra 12.1.4](https://github.com/NationalSecurityAgency/ghidra/releases) (used at build time for the export)
- SDL2 and dxvk-native, installed in a prefix (the build expects `~/.local/ct3-deps/prefix` by default). The dependency recipe is not scripted yet.
- Optional: FFmpeg (LGPL build), for the loading screen video. `tools/deps/build_ffmpeg.sh` builds it in the same prefix. Without it, the game starts without the loading video.
- Your own copy of `CT3.exe`

```sh
git clone https://github.com/sdiraimondo/ct3-recomp.git
cd ct3-recomp

cmake -S . -B build -G Ninja \
    -DCT3_EXE="/path/to/your/Crazy Taxi 3/CT3.exe" \
    -DCT3_DEPS_PREFIX="$HOME/.local/ct3-deps/prefix" \
    -DCT3_GHIDRA_DIR="$HOME/.local/share/ghidra_12.1.4_PUBLIC" \
    -DCT3_JAVA_HOME="/path/to/jdk-21"   # optional: the system JDK is used when omitted
cmake --build build --target ct3_run

./build/tests/run/ct3_run "/path/to/your/Crazy Taxi 3/CT3.exe"
```

The first build runs the whole translation of `CT3.exe` and takes several minutes.

Useful runtime settings:
- `Config.ini` also holds the game settings (read from the same place):
  - `[Patches] SkipIntro=1` skips the logo sequence (`0` keeps it).
  - `[Patches] UiTexcoord=1` fixes the distorted interface after a material with environment reflection.
  - `[Game] Aspect=16:9` or `4:3` selects the camera aspect. `[Game] Fov=` sets the horizontal field of view in degrees (60 in 4:3 by default, 80 in 16:9).
  - `[Distance] Map=1.0` multiplies the map draw distances (`2.0` is the value used by taxipatch).
  Absent settings leave the game's own values unchanged.
- `CT3_SPLASH=0` disables the loading screen video.
- `Config.ini` (in the working directory, or the path in `CT3_CONFIG`) holds the gamepad mapping and the thresholds. Default mapping: A = Reverse, B = Drive, Y = Space, Start = Enter, RT = accelerate (Up), LT = brake (Down), left stick = steer.
- Keyboard: Up accelerate, Down brake, Left and Right steer, X reverse, S drive, Space view or passengers, Enter start.
- `CT3_NOAUDIO=1` disables sound. `CT3_PRESENT_LOG=1` logs the time of each frame, to measure startup and slowdowns.

Tests (each one is a program, run from `build/tests`):
- `cpu/test_cpu` and `vec/test_vec` need no argument.
- `x87/test_x87` and `g2/test_g2` take the path to `CT3.exe`.
- `g1_d3d9_triangle/g1_d3d9_triangle` draws a triangle through dxvk-native. Use a virtual display (Xvfb) if you have no desktop session.

## Legal

- Crazy Taxi is a trademark of SEGA Corporation. This project is **not affiliated with, endorsed by or sponsored by SEGA**.
- The repository only contains original code, build scripts and documentation. It contains **no** copyrighted game code, recompiled output, assets or keys.
- Do **not** open issues or pull requests containing game files, recompiled output, or links to where to download the game. Such content will be removed.
- Please support the official release: buy the game.

## Credits

- [taxipatch](https://github.com/stashymane/taxipatch) by stashymane (GPL-3.0), the successor of CT3Tweaks. Its documented behaviour (settings and addresses) is the reference for the `[Patches]`, `[Game]` and `[Distance]` options.

- [CT3Tweaks](https://github.com/stashymane/ct3tweaks) by stashymane (Apache-2.0), the first tweak tool for Crazy Taxi 3. Its resolution, frame rate and field of view changes were used as a reference when analysing the executable.

No code from these projects is reused. Thanks to their author.

## License

No license has been chosen yet. Until one is added, the code is not licensed for reuse.
