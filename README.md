# Blitzkrieg — Android port

A native Android port of **Blitzkrieg** (Nival Interactive, 2003), the real-time
strategy game whose singleplayer source Nival released in 2025.

The approach is to compile the original C++ for arm64 and replace only the
platform layer, rather than rewriting the game. The goal is the game as it was —
its own menus, campaign, missions and art — with touch controls in place of
mouse and keyboard.

This repository is a fork of [nival/Blitzkrieg](https://github.com/nival/Blitzkrieg);
the original project README is kept as [README_Original.md](README_Original.md).

## State: it plays

Every module of Blitzkrieg compiles for arm64 and links into one Android
library. The APK installs, opens a surface, starts the engine, brings up sound,
and runs the game: the main menu, the campaign screen, the chapter and mission
briefings, and a mission on the battlefield -- terrain, units, minimap, command
panel -- reached entirely by touch, with no keyboard and no mouse.

Measured on an arm64 emulator, not asserted:

```
11 modules linked in
audio started: 44100 Hz, 2 channels, 32 voices
game data: 34 entries in /storage/emulated/0/Android/media/com.nival.blitzkrieg/data
60.0 fps, worst 17.4 ms                                    <- menus
59.9 fps | step avg 15.1 | swap 1.6 | GL 14.3 ms, 180 draws <- in a mission
```

The mission figure was 30 fps until the renderer stopped repeating itself. The
frame was 30 ms of talking to GL for 180 draws and under a millisecond of the
game thinking: every draw re-issued the whole pipeline whether or not anything
had changed, including a string lookup for a uniform location. Sending only
what actually changed halved it, and the picture is unchanged pixel for pixel.

This is an emulator, whose GL is translated in software on the host. I have not
measured a real device and will not pretend to have.

Reached by touch and verified on screen: the main menu, New Game, Campaigns,
the Allied campaign map, the chapter and mission briefings, a mission on the
battlefield, and Options down to the video page. In the mission, a one-finger
drag selects the units it encloses and a hold orders them; both were watched
happening rather than inferred.

Three bugs found here are worth naming because none is Android's fault:

- The engine truncates a locked texture's pointer to 32 bits --
  `reinterpret_cast<void*>( DWORD(lockinfo.pData) + i*nPitch )` in `CTextureLock`
  and three places like it. Harmless on Win32, fatal on arm64, and it faulted the
  moment a mission built its minimap. Now byte-pointer arithmetic, which compiles
  to the same thing on Win32.
- `config.cfg` and `defconf.cfg` are not in `Data`; they sit beside it and carry
  the control bindings. Without them the menus light up under the cursor and
  refuse every click -- input arrives, binds to nothing, and nothing says so.
- Reporting one display mode -- the surface's own size, which is the only true
  answer -- crashed Options. The screen lists the modes and then looks up the
  *current* setting in that list, and the game's default is `1024x768x32`; a
  list that cannot hold the value being searched for yields index -1, and the
  read lands 24 bytes below zero. The adapter now reports the standard 4:3
  ladder a card of that era did.

The debug frame dump is off unless asked for -- `adb shell setprop
debug.blitzkrieg.frames 1` -- because a debug harness should not run in a
shipped game. I expected it to explain the missions' frame times and measured
instead: with nothing written at all, the profile is unchanged. What costs
those frames is still unmeasured.

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

Build and install first -- the data goes into a directory that belongs to the
installed package:

```bash
android/build_apk.sh Release && adb install -r android/build/Release/Blitzkrieg.apk
```

The game's data belongs to whoever owns a copy of Blitzkrieg. Copy the `Data`
directory and the two config files out of an installation:

```bash
adb push /path/to/Blitzkrieg/Data/. /sdcard/Android/media/com.nival.blitzkrieg/data/
```

```bash
adb push /path/to/Blitzkrieg/config.cfg /path/to/Blitzkrieg/defconf.cfg /sdcard/Android/media/com.nival.blitzkrieg/
```

`Android/media`, not `Android/data`, and that is not a preference. Android gives
an app a passthrough mount of its own `Android/data/<pkg>`, so the real uid on
disk decides: `adb push` writes as shell, and the app -- neither the owner nor in
the group -- gets `EACCES` on a directory whose contents `adb shell ls` prints
happily. The modes are synthesised by FUSE, so `chmod` does not move them, and
without root neither does `chown`. `Android/media/<pkg>` goes through
MediaProvider instead, which grants an app its own package directory whatever
uid wrote the bytes, and needs no permission at all.

The port looks in both, plus `/sdcard/Blitzkrieg`, and says which one it took and
why it passed over the others.

`config.cfg` and `defconf.cfg` are not optional. They carry the control
bindings, and without them every menu lights up under the finger and answers
nothing -- the input arrives, binds to nothing, and the engine does not say so.

## What is verified, and what is not

Three campaigns start and play: Allied (winter Norway), German (summer), Soviet
(deep winter) -- different terrain, seasons and unit sets, each holding 60 fps.
Menus, campaign map, chapter and mission briefings, Options down to the video
page, and a mission on the battlefield, all reached by touch.

The Tutorials list works too, and fixing it fixed a whole class. `fnmatch` reads
`*.*` literally -- something, a dot, something -- while Windows has treated it as
"everything" since MS-DOS. The engine enumerates a directory with `*.*` and
recurses into whatever comes back marked as a directory, and directories rarely
have dots, so the walk never descended. Every list built by scanning a tree came
up empty. One line, and six tutorials appeared.

Known broken: **the mission after the first renders black.** Finishing a mission
reaches the statistics screen, which is correct and complete -- the table, both
sides, the timings -- and leaving it advances the campaign. The next mission
then loads, runs its script and holds 60 fps at 181 draws a frame, and the
screen is black: 96 of 96 sampled pixels, read out of the framebuffer itself
rather than through a screenshot.

Measured four times. What is ruled out, so nobody walks these again:

- **The transforms.** Naming each draw by its place in the frame -- not sampling
  every N-th one, which compares different passes and is what made the first two
  readings wrong -- draws 0, 20, 60, 100 and 140 are identical to the last digit
  in both missions, world, view and projection alike.
- **The GL state cache.** Invalidating it on texture death was a real latent bug
  and is fixed; it changed nothing here.
- **Texture name reuse** after `glDeleteTextures`, for the same reason.
- **The clear.** Both missions clear black with the same flags before drawing.

Two more measurements after those: at the same fixed positions, the depth test,
the blend, the alpha test, the cull mode and the bound texture -- its size, its
byte count, its GL name, its uploaded flag -- are identical in both missions
as well.

So everything this trace can see matches, and one frame draws while the other
is black. The honest reading is that the trace is not yet looking at the pass
that differs: the sampled textures are 256x512, 32x16, 128x128, which are
interface and font sizes rather than terrain. Fixed draw positions compare like
with like only if the frame has the same shape in both runs, and 180 draws
against 181 says it does not, quite.

The trace is off unless asked for and stays in the build, because it is where
the next attempt starts:

```bash
adb shell setprop debug.blitzkrieg.draws 1
```

Two of the four readings were published here as findings before being checked
against a like-for-like comparison. They are withdrawn.

Found by adding a way to end a mission as a win on request --
`adb shell setprop debug.blitzkrieg.winmission 1` -- because walking the road
after a mission is a test of the port and playing well enough to earn it is not.

Not verified, and I will not claim otherwise: no real device -- every figure
here is from an arm64 emulator; no campaign played to the end, only its opening
missions; and two-finger gestures are covered by the recogniser's own tests but
not injected through Android. That last one is not for want of trying: the
emulator exposes a device per finger, `shell` is in the `input` group and the
nodes are `0660 root:input`, but `sendevent` is refused by SELinux, and the
`google_apis` image that would allow `adb root` does not boot on this host.

## Touch

Blitzkrieg wants a second mouse button, a wheel, arrow keys and a drag that
means "select". A touchscreen has none of them:

| Gesture | Stands in for |
| --- | --- |
| one finger, tap on the ground | the order: the units go there |
| one finger, tap on a unit or a button | left click: select, or press |
| one finger, drag | **the map moves with the finger** |
| one finger, held still | right click: the order gesture |
| two fingers, drag | the selection box |
| two fingers, pinch | zoom |
| three fingers | the screen keyboard, on and off |

The first two are the way round every map on the device and every strategy game
written for one has them: the commonest action gets the commonest gesture. The
selection box, which a player reaches for far less often, took the second
finger. This is the opposite of where the port started, and the swap is the
single thing that stopped it feeling like a desktop game being poked at.

One consequence worth naming: the left button no longer goes down when a finger
lands. It waits for the lift. Pressing on touch was right while a drag meant a
box; now that a drag moves the map, it would have meant every pan began by
clicking whatever it started on.

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
