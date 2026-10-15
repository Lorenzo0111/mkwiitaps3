# Mario Kart Wii for PS Vita

A native PlayStation Vita port of
[MiniBandit's MKW-3DS](https://github.com/MiniBandit/MKW-3DS), a 1:1
recreation of **Mario Kart Wii** for the Nintendo 3DS. It's a port of that
3DS recreation, not of the Wii game: no Wii code runs.

It isn't an emulator. The game runs natively on the Vita:
- **Physics:** kart physics come from [Kinoko](https://github.com/vabold/Kinoko), a recreation of Mario Kart Wii's physics.
- **Game code:** menus, races, items and the AI are MKW-3DS's native code.
- **3DS layer:** a small layer underneath that code provides the 3DS system and graphics functions it expects, implemented with the Vita's own APIs and [vitaGL](https://github.com/Rinnegatamante/vitaGL).

This repository contains **no Nintendo files**. You build the game files
yourself, on your own computer, from your own Mario Kart Wii disc.

> **PS3:** an untested PS3 (CFW/HEN) build is also available; see
> [docs/PS3_PORT.md](docs/PS3_PORT.md).

> **Status: playable, work in progress.** Menus, Grand Prix (including
> Mirror and the award ceremony), VS and Time Trials run on real hardware,
> with sound, in 16:9. Battle is playable and still being worked on. Races run
> at 45-60 FPS (or a locked 30, see [Settings](#settings)). The first time a
> new material appears the game pauses to compile its shader; those are
> cached, so later launches are smooth.

## What works

| | |
|---|---|
| Menus, Grand Prix (50/100/150cc and Mirror), VS, Time Trials | ✅ |
| Grand Prix award ceremony and final standings | ✅ |
| All 24 drivers and 36 vehicles | ✅ |
| Items, CPU racers, results and standings | ✅ |
| Music and sound effects | ✅ |
| Wii-style course map, race timer and incoming-item warning | ✅ |
| Best times saved | ✅ |
| Widescreen (16:9) | ✅ |
| Battle: team Balloon Battle and Coin Runners on all ten arenas | ⏳ Playable, in progress |
| Steady 60 FPS | ⏳ 45-60 FPS in races; performance work ongoing |
| Team VS, online | ❌ Not yet |

## What you need

**To play:**
- A PS Vita or PS TV with [HENkaku / Ensō](https://vita.hacks.guide) and VitaShell.
- `ur0:data/libshacccg.suprx`, the Vita's shader compiler. Extract it once with [ShaRKF00D](https://github.com/Rinnegatamante/ShaRKF00D).
- About **3 GB** free on `ux0:` for the game files.

**To build** (see [BUILDING.md](BUILDING.md)):
- **Mario Kart Wii (USA)**, game ID `RMCE01`, dumped as `.iso` or `.wbfs`. Convert RVZ or NKit dumps to ISO in Dolphin first. Other regions aren't supported yet.
- A Windows PC with about 15 GB free.
- [Python 3.11+](https://www.python.org/downloads/), [Node.js LTS](https://nodejs.org) and [Git](https://git-scm.com).
- [VitaSDK](https://vitasdk.org), only if you build the VPK yourself.

## Quick start

1. **Get the VPK:** download `mkw.vpk` from the [latest release](https://github.com/boomannotjeff/mkwiita/releases/latest).
2. **Build the game files** from your disc: double-click **`Build Game Files.bat`**, drag your disc image into the window and press Enter. The first build takes 30–60 minutes.
   Use the source code of the same release as your VPK (the release page has it as a zip), so the game files match the program.

   *Building the VPK yourself?* Use **`Build MKW Vita.bat`** instead: it needs VitaSDK and makes both the game files and `build\vita\mkw.vpk`.
3. Install the VPK with VitaShell.
4. Copy the **contents** of `build\vita\data\mkw\` to `ux0:data/mkw/` on the Vita, over USB or FTP in VitaShell.
5. Launch **Mario Kart Wii** from the LiveArea.

**Please don't share the game files** (`data/mkw`): they contain Nintendo's
game data. The VPK itself is only the program; it holds no game data.

## Controls

| Vita | Action |
|---|---|
| Left stick | Steer; up/down when using an item throws forward/back |
| Cross | Accelerate / confirm |
| Circle | Look behind (hold) / back in menus |
| Square | Brake, reverse |
| R | Hop and drift; trick off a ramp |
| Triangle | Trick (stick picks the direction) / bike wheelie |
| D-pad | Trick / wheelie (Classic Controller style) |
| L | Use item |
| START | Pause |
| SELECT | Speedometer on/off |

## Settings

Optional: create `ux0:data/mkw/quality.txt` with one `name=value` per line.
The values in use are written to `ux0:data/mkw/vita.log` at start-up.

| Setting | Default | |
|---|---|---|
| `framerate` | `60` | `30` locks races to a steady 30 FPS |
| `resolution` | `1.5` | Internal resolution, `1` to `2`. `2` is sharper (848x480) but slower; use it with `framerate=30` |
| `map` | `1` | `0` shows the simple racing-line map instead of the Wii-style map |
| `shadowVolumes` | `1` | `0` turns off kart and object shadows |
| `bloomBlurRounds` | `2` | Glow: `1` cheaper, `0` off |
| `particleBudget` | no limit | Fewer smoke and spark particles, e.g. `96` |

For example, the sharp look at a locked 30 FPS:

```
framerate=30
resolution=2
```

## Troubleshooting

- **Black screen after the vitaGL logo:** `libshacccg.suprx` is missing (see above), or the game files aren't in `ux0:data/mkw/`.
- **Short freezes on the first launch:** shaders compiling. They're saved to `ux0:data/mkw/shaders/` and loaded at start-up after that.
- **Something looks wrong after an update:** delete `ux0:data/mkw/shaders/` to rebuild the shader cache.
- **Logs:** `ux0:data/mkw/vita.log` (graphics, frame rate) and `ux0:data/mkw/runtime.log` (game). Attach both when reporting a problem.
- **Problems building assets:** Try to use python 3.14.5 and make sure you have Visual C++ 14.0 or later.

## How it works

The 3DS game talks to libctru (the 3DS system library) and citro3d (its GPU
library). `platform/vita` re-implements the parts it uses on the Vita:
- **System:** threads, locks, timing, input and files.
- **Graphics:** the 3DS GPU's texture combiner becomes generated GLSL shaders, its vertex shaders are ported to GLSL, and textures are converted from the 3DS tiled format.
- **Sound:** a software mixer stands in for the 3DS DSP.

Details: [docs/VITA_PORT.md](docs/VITA_PORT.md).

## Credits

- **dudewhat:** the original Mario Kart Wii recreation for the 3DS
- **MiniBandit:** [MKW-3DS](https://github.com/MiniBandit/MKW-3DS), the continuation this port is built on (including its Balloon Battle groundwork)
- [Kinoko](https://github.com/vabold/Kinoko) by vabold (MIT): Mario Kart Wii physics
- [vitaGL](https://github.com/Rinnegatamante/vitaGL) and [vitaShaRK](https://github.com/Rinnegatamante/vitaShaRK) by Rinnegatamante: OpenGL and runtime shaders on the Vita
- [VitaSDK](https://vitasdk.org): the Vita toolchain
- [citro3d](https://github.com/devkitPro/citro3d) (zlib) and [libctru](https://github.com/devkitPro/libctru) (zlib): 3DS headers, matrix maths and the texture loader
- [Azahar](https://github.com/azahar-emu/azahar): reference for the 3DS GPU's texture and combiner behaviour
- [noclip.website](https://github.com/magcius/noclip.website) (MIT) and [gclib](https://github.com/LagoLunatic/gclib) by LagoLunatic (MIT): disc and model tools
- [ogws](https://github.com/doldecomp/ogws) and [mkw](https://github.com/snailspeed3/mkw) decompilations (CC0): references

Mario Kart Wii is © Nintendo. This is an unofficial fan project with no
affiliation with Nintendo or Sony. See [NOTICE](NOTICE).
