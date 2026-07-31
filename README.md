# Blitzkrieg — Android port

A native Android port of **Blitzkrieg** (Nival Interactive, 2003), the real-time
strategy game whose singleplayer source Nival released in 2025.

The approach is to compile the original C++ for arm64 and replace only the
platform layer, rather than rewriting the game. The goal is the game as it was —
its own menus, campaign, missions and art — with touch controls in place of
mouse and keyboard.

This repository is a fork of [nival/Blitzkrieg](https://github.com/nival/Blitzkrieg);
the original project README is kept as [README_Original.md](README_Original.md).

## State: it builds, installs and runs. It is not the game yet.

There is now an APK that installs on a device, opens a surface, holds 60 fps
and routes touch into the engine's input. What it does not do is draw the game,
because several engine modules are still unported and there is nothing yet to
call.

Measured on an arm64 emulator, not asserted:

```
surface 2700x1280, renderer Android Emulator OpenGL ES Translator
60.0 fps, worst frame 19.3 ms
touch reaches the engine: cursor at 900, 640
```

The last line is the one that matters. A tap was injected at 900,640 and the
position read back through `GetCursorPos` -- the call the engine itself makes --
so a finger arrives where the engine looks for a mouse. That is the whole design
of the touch layer: the engine keeps its own input model and a finger fills it
in, so its bindings, double-click and drag handling need no changes.

Fifteen modules build for arm64 with nothing failing:

| Module | Units | Module | Units |
| --- | --- | --- | --- |
| `Scene` | 50 / 50 | `Common` | 15 / 15 |
| `UI` | 33 / 33 | `Net` | 15 / 15 |
| `RandomMapGen` | 29 / 29 | `zlib` | 15 / 15 |
| `GameSpy` | 25 / 25 | `Misc` | 12 / 12 |
| `StreamIO` | 21 / 21 | `Formats` | 12 / 12 |
| `LuaLib` | 18 / 18 | `Image` | 11 / 11 |
| `GFX` | 17 / 17 | `Anim` | 8 / 8 |
| `Input` | 7 / 7 | **Total** | **288** |

Not started, and this is the bulk of what is left:

| Module | Files | Lines |
| --- | --- | --- |
| `AILogic` | 170 | 89,209 |
| `GameTT` | 65 | 27,741 |
| `Main` | 39 | 23,887 |
| `SFX` (FMOD → Oboe) | 9 | 2,119 |
| `Game` | 6 | 1,766 |

Direct3D 8 and DirectInput are written -- a sprite and quad renderer on OpenGL
ES 3, and a buffered input device that touch fills in -- but until `Main` is
ported nothing asks them to draw, which is why the surface is still empty.

A note on measuring this tree, because it cost real time: the sources are
ISO-8859-1 and carry high bytes, which makes macOS `grep` treat 744 of the
2,055 files as binary and print *nothing* -- no match, no warning, and `-c`
gives no output rather than zero. `LC_ALL=C` does not fix it; `-a` does. Any
count here taken without both is unsound.

```bash
android/build_apk.sh                        # an installable APK, SDK and NDK only
android/compile_check.sh Misc StreamIO      # what builds, per module
android/tests/run_tests.sh                  # the tests that guard the rewritten parts
```

Gradle works too and is the normal way in; `build_apk.sh` exists because it
needs no network, no plugin download and no wrapper jar. Both produce the same
package. The game's own data is not in this repository and never will be -- it
belongs to whoever owns a copy of the game.

## What was replaced rather than ported

Some of the original's dependencies do not exist off Windows and could not be
answered with a stub, because the game genuinely depends on what they do:

- **MSXML** — the game's data is 7336 XML files, so this is a real parser,
  serialiser and DOM with COM-style reference counting.
- **The S3TC texture codec** — DXT1 through DXT5, compressing and expanding,
  with endpoints fitted to the principal axis of each block. Covered by a
  round-trip test.
- **The registry** — a tree of keys and values over a file, since the game keeps
  its installation path and options there.
- **Win32 platform** — threads, events, critical sections, the file APIs with
  case-folding directory scans, code-page conversion with real CP1251 and
  CP1252 tables, sockets over POSIX, process creation over fork and exec, and
  the floating-point control word over arm64's FPCR.

Everything changed in the original sources is guarded on `_MSC_VER` or
`_M_IX86`, so the Windows build still compiles as it did.

Notes on the decisions, and the measurements behind them, are in
[android/README.md](android/README.md).

## Licence

The game and its source are Nival Interactive's, released under a licence that
permits study and modification but **prohibits commercial use** — see
[LICENSE.md](LICENSE.md). This port is non-commercial and carries the same
terms. It is not affiliated with or endorsed by Nival.

Blitzkrieg is still sold on
[Steam](https://store.steampowered.com/app/313480/Blitzkrieg_Anthology/) and
[GOG](https://www.gog.com/en/game/blitzkrieg_anthology).

## One file this repository cannot carry

`Versions/Current/Data/Movies/intro.bik` is stored in the upstream repository
through Git LFS, and that repository's LFS budget is exhausted — the object
cannot be fetched from it by anyone, including this fork. Rather than publish a
pointer to a file that will not download and a clone that fails, the port drops
it.

It is the introduction movie and nothing depends on it. To restore it, copy the
file from a retail installation of the game into that path.
