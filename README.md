# Blitzkrieg — Android port

A native Android port of **Blitzkrieg** (Nival Interactive, 2003), the real-time
strategy game whose singleplayer source Nival released in 2025.

The approach is to compile the original C++ for arm64 and replace only the
platform layer, rather than rewriting the game. The goal is the game as it was —
its own menus, campaign, missions and art — with touch controls in place of
mouse and keyboard.

This repository is a fork of [nival/Blitzkrieg](https://github.com/nival/Blitzkrieg);
the original project README is kept as [README_Original.md](README_Original.md).

## State: the whole engine builds, links and runs. It needs the game's data.

Every module of Blitzkrieg compiles for arm64 and links into one Android
library. The APK installs, opens a surface, starts the engine, brings up sound,
holds 60 fps, and routes touch gestures into the engine's own input. It stops
where it runs out of the game's data files, which this repository does not
carry.

Measured on an arm64 emulator, not asserted:

```
11 modules linked in
audio started: 44100 Hz, 2 channels, 32 voices
1 archives in .../files/data
60.0 fps, worst frame 20.0 ms
touch reaches the engine: cursor at 900, 640
gesture: hold -> right click at 844, 700
assert: pStream != 0 -- Can't open stream "consts.xml" (StructureSaver.h:200)
```

The last line is where it stands. `consts.xml` lives inside the game's `.pak`
archives; the engine reads it while building the camera, and without it there is
nothing further to run. Everything above that line is the port working.

The machinery that reads those archives is verified, separately from having
them. Putting a `.pak` on the device containing nothing but a well-formed
`consts.xml` -- a test fixture, not game content -- takes the engine past
`CCamera::Init` and on to the next file it wants, `cursor\1.xml`. That exercises
the whole chain in one go: the zip reader, the MSXML replacement written for
this port, and the data table on top of it. Startup from here is a walk through
the game's own data, and each step now names the file it is missing rather than
faulting.

| Module | Units | Module | Units |
| --- | --- | --- | --- |
| `AILogic` | 169 | `Net` | 15 |
| `GameTT` | 65 | `zlib` | 15 |
| `Scene` | 50 | `Common` | 15 |
| `Main` | 39 | `Misc` | 12 |
| `UI` | 33 | `Formats` | 12 |
| `RandomMapGen` | 29 | `Image` | 11 |
| `GameSpy` | 25 | `SFX` | 9 |
| `StreamIO` | 21 | `Anim` | 8 |
| `LuaLib` | 18 | `Input` | 7 |
| `GFX` | 17 | `libpng` | 17 |
| | | **Total** | **575** |

`Game` is absent on purpose: `WinMain`, a message loop and a keyboard hook are
the Windows shell, and `android_main` is what replaces them.

## Running it

The game's data belongs to whoever owns a copy of Blitzkrieg. Copy the `Data`
directory out of an installation:

```bash
adb push /path/to/Blitzkrieg/Data/. /sdcard/Android/data/com.nival.blitzkrieg/files/data/
```

The port looks for `*.pak` there, says so in the log if it finds none, and stays
running rather than crashing so the message can be read.

I have not run the game itself. Without a legal copy of the data I cannot, and I
will not describe menus or missions I have not seen working. What is written
above is what a device actually printed.

## Touch

Blitzkrieg wants a second mouse button, a wheel, arrow keys and a drag that
means "select". A touchscreen has none of them:

| Gesture | Stands in for |
| --- | --- |
| one finger, tap | left click |
| one finger, drag | button held down: the selection box |
| one finger, held still | right click: the order gesture |
| two fingers, drag | scroll the camera |
| two fingers, pinch | zoom |

These feed the buffered mouse and keyboard the engine's own bindings read, so
whatever the player has bound in the options still applies.

A second finger cancels whatever the first was doing: two fingers are never a
click, or every camera movement would drop a stray order on the battlefield. The
map moves with the fingers, so dragging left brings the land on the right into
view. Thresholds are in millimetres converted by the display's real density,
because twenty pixels of drift is a held finger on a dense phone and a drag on a
coarse tablet.

The recogniser is a unit with no Android in it, because two-finger gestures
cannot be injected on a stock emulator image: `/dev/input` refuses a non-root
writer. `android/tests/touch_gesture_test.cpp` drives the same unit the APK
ships.

## Building

```bash
android/build_apk.sh                        # an installable APK, SDK and NDK only
android/compile_check.sh Misc StreamIO      # what builds, per module
android/tests/run_tests.sh                  # the tests that guard the rewritten parts
```

Gradle works too and is the normal way in; `build_apk.sh` exists because it
needs no network, no plugin download and no wrapper jar. Both produce the same
package.

A note on measuring this tree, because it cost real time: the sources are
ISO-8859-1 and carry high bytes, which makes `grep` treat 744 of the 2,055 files
as binary and print *nothing* -- no match, no warning, and `-c` gives no output
rather than zero. `LC_ALL=C` does not fix it; `-a` does. Any count taken without
both is unsound.

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
- **Direct3D 8** — the engine drives it as a fixed-function 2D blitter: five
  texture operations across at most two stages, vertices already in screen
  space, no programmable shaders at all. So the replacement is one small GLES 3
  program with the stage configuration in uniforms, resolving arguments the way
  Direct3D does. The alpha test, which has no fixed-function equivalent, is a
  `discard`.
- **FMOD** — an AAudio device and a mixer: forty-two entry points, measured out
  of the call sites. Samples arrive as memory and are WAV, so they are parsed
  directly; music is a file and goes through the NDK's media codecs on its own
  thread. The mixer never allocates, opens a file or blocks, because an
  underrun is an audible click.
- **DirectInput** — a buffered device with the engine's own scan codes, which
  is what saved key bindings hold, filled in by the touch gestures.
- **BugSlay** — only the five entry points the game calls. The rest is Windows
  crash handling that Android's own tombstones do better. Its asserts are not
  diagnostics: their third argument is the engine's error handling, so they are
  enabled, and a missing file now says which file rather than faulting.

Twenty DLLs became one library, and that is a difference in behaviour, not in
packaging. Each DLL carried its own copy of the process-wide singleton
pointers; the loader guaranteed a module's statics were built before anything
that used them, and inside one binary nothing does. Both places that relied on
it are ordered explicitly now.

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
