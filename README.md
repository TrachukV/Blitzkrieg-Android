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

**Fixed.** The answer is at the end of this section; the record of getting there
is left standing because most of it is wrong, and the wrong parts are the useful
ones. Short version: `CTransition` paints it, the curtain that `FinishInterface`
lowers is infinite by design, and the screen that should lift it is reached by
popping -- a path that never calls `StartInterface`. What follows is what was
believed along the way.

Was broken: **the mission after the first renders black.** Finishing a mission
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

Then a measurement that splits the question instead of comparing samples.
Forcing the clear to a blue nothing in the game paints with:

    mission 1   #000607, #00060d   the blue shows through the dark
    mission 2   #000000 on 96 of 96 samples

The clear never reaches the screen in the second mission, and `Clear` is called
in both with the same flags -- so something paints the whole frame black over
it. That is a full-screen quad, and the likeliest one is the transition fade:
the game darkens the screen when a mission ends, and on the second mission it
never comes back up.

That is proven. What paints it is now named, if not yet convicted.

`CInterfaceScreenBase::OpenCurtains` runs when a screen starts, and the curtain
it opens is not a video: `CTransition::Start` ignores the file name entirely,
sets an alpha to fall from opaque to clear over a fixed duration, and returns
that duration. So the transition is added to the scene whatever the state of
the Bink replacement, and it begins **fully opaque**. A screen is black until
the fade advances.

A fade that never advances stays opaque, and that is a black screen over a frame
that drew perfectly well underneath. It explains every measurement in this chase
rather than one of them.

The clock was the obvious suspect and it checks out: `timeGetTime` here is
`steady_clock` in milliseconds truncated to 32 bits -- monotonic, counting from
boot, the same shape and the same width as the Win32 original, and it cannot
return the zero that `CTransition::Update` uses as its "not started" sentinel.

And measured, the curtain says the rest itself:

    curtain: alpha 255.0 (0.0 -> 255.0), 51694 ms of 500, alive 1

Three things at once. The alpha runs 0 to 255, so this is the *closing* curtain,
not the opening one. Fifty-one seconds have passed against a five-hundred
millisecond duration, so it finished long ago and sits at full black. And it
still reports itself alive, because `bInfinite` makes `Update` return true
whatever the clock says -- so the scene never drops it.

The closing curtain of the finished mission is left in the scene, opaque,
for ever. The first mission prints nothing at all, having no curtain to begin
with.

What paints it is a `CTransition`: a full-screen quad whose alpha the trace
caught at 255 of 255, fifty-one seconds into a five-hundred millisecond fade,
still reporting itself alive because it was started infinite.

Where that transition comes from took three readings, two of them wrong, and the
correction of the second was wrong as well.

Tracing the creation site and `CScene::Clear` together printed nothing across a
whole run, with the screen black and the transition demonstrably updating -- so
I withdrew the creation site. Then searching the entire tree rather than two
directories: `Common/InterfaceScreenBase.cpp:164` is the *only* place a
transition is created anywhere in the engine.

The trace was not the problem, though: a trace in that same file printed
perfectly well during bring-up, so `DebugTrace` from `Common` does reach the
log and the silence was real. `PlayOverInterface` genuinely does not run.

Then the tool itself was checked, which is what should have come first. Entry
traces in all four places at once, counted over one run:

    CTransition::Update                  1377
    CTransition::Start                      0
    CTransition::operator&                  0
    CInterfaceScreenBase::PlayOverInterface 0

The tracing works -- `Update` fires more than a thousand times. And `Start`
never runs at all.

`CTransition` turned out to have no constructor at all: `timeStart`,
`fAlphaStart`, `fAlphaEnd`, `fAlpha` and `bInfinite` are set only by `Start`,
which never runs. So an unstarted one carries whatever was in the memory it got,
and the "alpha 255, infinite, fifty-one seconds into a five-hundred millisecond
fade" was that garbage being read as fields. It now has a constructor, because
uninitialised members are a defect whatever else is true.

It did not fix the black screen. Which retires the whole line: seeing a
`CTransition` update and concluding it was what covered the screen was an
inference, not a measurement -- nothing ever showed it drawing. With its alpha
now zero and its lifetime finite, the frame is still black on all 96 samples.

Seven readings of this bug, seven withdrawn. The pattern never varied: a fact
from a measurement, a few steps of reading code, and the conclusion carried out
with the weight of the fact. What is actually known is small and has not moved
in a while:

- the frame is black while 180 draws happen and the states, transforms and
  textures all match a mission that renders
- `Clear` runs, and its colour does not survive to the readback
- nothing yet identified accounts for that

Watching it get covered -- reading one pixel back after every draw -- answered
in one run what seven readings of the code could not:

    centre pixel went BLACK  at draw 180:  0 0 0
    centre pixel went bright at draw 0:   72 62 25

The scene draws correctly. At draw 0 the middle of the screen is terrain
colour, and it stays that way through the frame. The **last draw of the frame**
blackens the whole thing, every frame, and the next frame starts bright again.

Which finally gives meaning to a number measured early and dismissed: the first
mission issues 180 draws a frame and the second 181. That extra draw is the one
that covers the screen.

And identified. The tail of the frame is the same in both missions -- draws 176
to 179, textured -- except that the second has one more, and it is the only one
in the frame with no texture at all:

    draw 180: fvf 0x1c2 stride 28 | blend on 5/6 | alphatest 1 ref 1 | tex0 none

That is the draw the pixel watcher caught blackening the screen. The format
decodes without ambiguity: position, diffuse, specular and one texture
coordinate pair, stride 28 -- a full-screen quad coloured by its vertices and
textured by nothing.

Which closes the loop with a fix made earlier the same day: a stage with no
texture answers `D3DTA_TEXTURE` with white, so `MODULATE` of white by the
diffuse is the diffuse -- black, with whatever alpha the vertex carries. The
screen is black, so the alpha arriving at the blend is opaque when it should be
clear.

And that value, read:

    untextured quad at draw 180: diffuse b=0 g=0 r=0 a=255

Black, and fully opaque, and present only in the second mission. With
SRCALPHA/INVSRCALPHA blending an alpha of 255 is a solid fill, and the alpha
test at reference 1 lets it through. So the frame is covered by an opaque black
full-screen quad -- measured, not inferred.

It is also **not** the `CTransition` given a constructor earlier the same day:
that one packs its alpha into the colour and would now be zero. Whatever draws
this is something else, and it is described exactly rather than guessed at --
the frame's only untextured draw, last in order, opaque black, absent from the
mission that renders.

Full-screen coloured rectangles come from `IGFX::DrawRects`, and its callers are
enumerable: a dozen UI elements, `Scene/SquadVisObj.cpp`, and the transition
that the alpha rules out. Picking one from that list by its name would be the
tenth theory in this section; the nine before it were all withdrawn.

And its geometry, read from the same place:

    first vertex 0.0,768.0 | 2 primitives

Two primitives is one quad, and its corner sits at the bottom-left of the
engine's own 1024x768 screen. A full-screen black sheet, laid down last, every
frame, in the second mission only.

`CTransition` is excluded twice over now. Its `Draw` packs the alpha into the
top byte, so the colour measured is exactly what it would produce at alpha 255 --
but `ALPHA_MIN` is 0 and `ALPHA_MAX` is 255, so the zero its constructor now
sets survives the clamp and it would draw nothing at all.

Which is where this stops, with the defect described completely and nothing
guessed:

| | |
| --- | --- |
| when | last draw of the frame, every frame |
| where | full-screen quad, first vertex 0,768 in engine coordinates |
| what | 2 primitives, `fvf 0x1c2`, stride 28, no texture |
| colour | black, alpha 255, opaque under SRCALPHA/INVSRCALPHA |
| whose | one of `IGFX::DrawRects`' callers, `CTransition` excluded |
| only | in the second mission of a run; the first has 180 draws, this has 181 |

A candidate, marked as one: `CSimpleWindow::Draw` (`UI/UIBasic.cpp`) is the
generic path by which a UI window draws its own rectangles, and a full-screen
window would produce exactly this shape. It normally draws them textured, so
this would be a window whose image is absent -- and with an unbound stage
answering white, `MODULATE` leaves the diffuse alone: opaque black.

That is a shape argument, not a measurement, and shape arguments are what the
ten withdrawn readings above were made of. Tracing `CSimpleWindow::DrawBackground`
for a full-screen rectangle printed nothing, so the quad does not come through
that function -- one candidate eliminated, and the trace taken back out.

This is where the investigation stopped the first time. Seven instrumentation
attempts in a row missed -- a cached switch, the same cache again, a limiter on
the wrong counter, a limiter on the wrong draw, a log on one draw path of two, a
compile error fixing that, and finally a trace in a function the draw does not
pass through. Each cost a full cycle and each produced a silence
indistinguishable from a finding.

### What it actually was

Stop reasoning about which object it might be, and ask the code. `DrawRects` is
the one funnel every rectangle passes through, so a probe there recording the
**return address** names the caller with nothing inferred:

    CTransition::Draw            Scene/Transition.cpp:32
    CScene::Draw                 Scene/SceneDraw.cpp:694   (alwaysObjects)
    CInterfaceScreenBase::Step   Common/InterfaceScreenBase.cpp:244

Identical on all 10812 samples. So `CTransition` was the painter the whole time,
and the withdrawal above that cleared it was itself wrong -- it cleared the right
suspect on indirect evidence.

The rest followed from tracing the always-visible list and the command queue:

- `FinishInterface` lowers a curtain: `PlayOverInterface` with `PLAY_INFINITE`,
  so the transition fades to opaque black and **stays** there deliberately while
  the next screen loads.
- The next screen lifts it in `StartInterface` via `RemoveTransition`.
- The queue is healthy. The command is created, counts down once per frame
  (`650 -> 364 -> ... -> 18`) and executes. No `IsValid` failure, no
  `ResetStack`. Both explanations guessed before this trace were wrong.
- The command is `MAIN_COMMAND_POP` -- id 268501014, which is
  `MAIN_BASE_VALUE + 22`. Not `CAMPAIGN` and not `CHAPTER`, which live at
  `0x100e0085` and `0x100e0087` and were never involved.

`POP` does not build an interface, so it never runs
`CInterfaceCommandBase::Exec`, so `StartInterface` is never called, so nothing
lifts the curtain. `CMainLoop::PopInterface` hands the screen underneath the
focus and nothing more. Every later frame then draws correctly underneath an
opaque black sheet.

The fix follows the engine's own contract -- a screen that becomes current lifts
the curtain. Screens built by a command do it in `StartInterface`; a screen
reached by popping now does it in `PopInterface`. It removes only the curtain,
not the whole always-visible list the way `RemoveTransition` does, because on
that path the list also holds the mission's gamma fader.

Verified on the emulator on the exact sequence that used to fail: finish mission
one, dismiss statistics, and the next mission renders. Re-checked on a build with
the diagnostics compiled out.

One warning, learned twice in one sitting: every one of these switches has to be
**re-read**, not cached on the first draw of the run. A cached one cannot be
armed for the case you want -- and its silence then reads exactly like code that
never runs, which is how two of the withdrawn readings in this section happened.
Both switches re-read now.

Found by adding a way to end a mission as a win on request --
`adb shell setprop debug.blitzkrieg.winmission 1` -- because walking the road
after a mission is a test of the port and playing well enough to earn it is not.

Known broken: **Restart Mission aborts.** Reproduced on the German campaign's
first mission -- End Mission, then Restart Mission -- and this is the captured
stack, not a reconstruction:

    Abort message: 'Pure virtual function called!'
    __cxa_pure_virtual
      CUnitTurret::~CUnitTurret()+60
      CUnitTurret::Release(int, int)
      CMilitaryCar::~CMilitaryCar()
      CTank::Release(int, int)
      CListsSet<CObj<CAIUnit>>::~CListsSet()
      NGlobalObjects::Clear()
      CAILogic::Clear()

An earlier version of this file named `CAITransportUnit::Release` here. It does
not appear in the stack at all; the teardown runs from `CAILogic::Clear` through
the global unit list. That guess is retired.

What the stack and the class say together, with nothing added: `CUnitTurret`
declares no destructor, and of its members only `CPtr<CAIUnit> pOwner` is
non-trivial -- `nModelPart`, `dwGunCarriageParts`, `wHorConstraint`,
`wVerConstraint` and `bCanRotateTurret` are all PODs. So the entire body of the
generated destructor is releasing that one reference, and that is where it
aborts.

`pOwner` points back at the object being destroyed. `~CMilitaryCar` releases its
turret; the turret releases a counted reference to the car that owns it. Unit
owns turret, turret owns unit -- a cycle. Releasing it from inside the owner's
destructor reaches an object whose vtable has already dropped to `CTurret`,
where seven accessors are pure.

Not patched. The right repair is to make the turret's back-pointer non-owning,
but `pOwner` is serialized and read across AILogic, so changing what keeps a
unit alive needs testing far wider than the single path reproducible here.
Giving the pure virtuals safe defaults would silence the abort and leave the
cycle in place, which is worse than a known crash.

End Mission on its own tears down cleanly on the same build. The fault is
specific to restart, which queues `MISSION_COMMAND_MISSION` while the old
mission is still alive.

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
