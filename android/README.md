# Blitzkrieg 1 Android Port — Bootstrap

This directory is the start of a native Android port of Blitzkrieg (2003), built
the same way as the Blitzkrieg 2 port: compile the original sources for arm64
and replace only the platform layer, rather than rewriting the game.

Nothing here runs yet. What exists is the compatibility layer that lets the
original translation units reach the compiler at all, plus the measurements
that decided the approach.

## What the sources actually are

Measured on the checkout in this repository, not assumed:

| Module | Lines | Files including `windows.h` |
| --- | --- | --- |
| AILogic | 92 230 | 2 |
| Scene | 20 153 | 2 |
| UI | 16 063 | 2 |
| StreamIO | 10 332 | 2 |
| Misc | 9 549 | 2 |
| GFX | 8 927 | 2 |

The game modules without the editor and tools come to roughly 200 000 lines.
Win32 enters through each module's `StdAfx.h`, not through the code, which is
what makes a shim layer viable.

The renderer is **Direct3D 8 fixed-function used as a 2D blitter**. There are
no programmable shaders anywhere — `CreateVertexShader`, `CreatePixelShader`,
`vs_1_`, `ps_1_` and `D3DVS_` return zero hits across `GFX` and `Scene`, and
`CGraphicsEngine::SetVertexShader` merely forwards an FVF code. The ground is
not a heightfield mesh: `Scene/MeshBuilders.cpp` generates tile vertices
directly in screen pixels and emits them pre-transformed (`D3DFVF_XYZRHW`),
and `CTerrain::Draw` runs with depth test and write off. The camera is
orthographic; `CreatePerspectiveProjectionMatrixRH` exists in `Misc/Geometry.h`
and has no call site anywhere in the tree.

This is the opposite of the usual assumption in the helpful direction: the
world is screen-space 2D, so `GFX` — only ~9 000 lines — is reimplemented on
bgfx as a sprite/quad batcher rather than ported as a 3D renderer.

There is no Granny anywhere in the tree, so the animation-asset problem that
blocked 263 meshes in the Blitzkrieg 2 port does not exist here.

## Engine lineage

Blitzkrieg 1 is the same engine family one generation earlier. `NI_ASSERT`
appears in 168 files, `Singleton` in 144, and `AILogic` carries the same
`CAIUnit`, `CGroupLogic` and `Segment()` architecture the Blitzkrieg 2 port
links unmodified. Lua is present as `LuaLib`.

The object and serialization layer differs: `OBJECT_BASIC_METHODS` and
`IBinSaver`, which the Blitzkrieg 2 port leans on, are effectively absent here.

MFC is **not** a dependency despite 76 `AILogic` files mentioning `afx`. Every
mention is the `#ifdef __AFX__` branch in `StdAfx.h` or a Visual Studio include
guard; `CArchive`, `CObject`, `CString`, `CFile` and `DECLARE_SERIAL` have zero
uses. The game configuration takes the other branch: STLport plus `comutil.h`.

## The compatibility layer

`app/src/main/cpp/compat/` holds what the original translation units need that
the NDK does not provide:

| File | Why |
| --- | --- |
| `stl/_config.h`, `stl_user_config.h` | Neutralise STLport so libc++ is used instead |
| `hash_map`, `hash_set` | The engine spells these `std::hash_map` / `std::hash_set` — 14 and 3 call sites, none unqualified — so the aliases are injected into `std`, onto `std::unordered_*`, whose template parameters sit in the same order |
| `comutil.h` | `_variant_t` and `_bstr_t`, the only COM types the engine's property system uses |
| `windows.h`, `bk1_win32_types.h` | The Win32 scalar types, handles and macros `StdAfx.h` reaches for |
| `imagehlp.h` | BugSlayer's declarations, included from every module's `StdAfx.h` |
| `bk1_msvc_types.h` | Force-included ahead of every unit |

STLport is **vendored in this repository** at `Sources/sdk/stlport`, so
neutralising it in favour of libc++ is a choice rather than a necessity. The
choice is not free: the engine uses STLport extensions such as `std::construct`,
and libc++ brings its own `<cmath>` float overloads that collide with the
engine's (see below). Building against the vendored copy remains the fallback
if the libc++ path proves more expensive than it looks.

Two compiler flags carry more weight than the whole layer:

- `-fms-extensions` makes clang resolve the **2 137 include directives written
  with backslashes** across 668 files. Without it every one of them fails; with
  it none do. It also supplies `__int64` and the calling-convention keywords.
- `-Wno-reserved-user-defined-literal` accepts the MSVC-era string
  concatenation (`"a"macro"b"`) the engine uses in macros.

Reproduce the current state with:

```bash
NDK=~/Library/Android/sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin
COMPAT=android/app/src/main/cpp/compat
cd Sources/src && $NDK/aarch64-linux-android24-clang++ \
  -std=c++17 -fms-extensions -fdelayed-template-parsing \
  -Wno-reserved-user-defined-literal -fsyntax-only \
  -include ../../$COMPAT/bk1_msvc_types.h -I../../$COMPAT -I. -IMisc \
  Misc/StrProc.cpp
```

## Measuring this tree at all

The sources are **ISO-8859-1**, not UTF-8. `grep` in a UTF-8 locale classifies
them as binary and silently reports nothing — not an error, just no output. Every
count in this file is taken with `LC_ALL=C`; anything measured without it is
wrong in the direction of "there is no problem here."

## Where the build stands

| Module | Compiles | Total |
| --- | --- | --- |
| `Misc` | **12** | 12 |
| `StreamIO` | **8** | 23 |

```bash
android/compile_check.sh Misc StreamIO
```

The flags that matter, beyond the two already described:

- `-D_LIBCPP_ENABLE_CXX17_REMOVED_FEATURES` restores `std::ptr_fun`,
  `auto_ptr` and the rest of the C++17 removals the 2003 sources use.
- `-fdelayed-template-parsing` gives templates MSVC's lookup timing.

## The wchar_t decision

The engine keeps wide strings as `WORD*` (UTF-16, as on Windows) while also
building `std::wstring` over the platform's `wchar_t` — 251 uses. Under MSVC 6
those were the same 16-bit type. On Android `wchar_t` is 32 bits, so they are
not, and the two spellings have to be reconciled.

`-fshort-wchar` looks like the fix and is a trap. Compiling a `std::wstring`
under it emits four *undefined* references to
`std::__ndk1::basic_string<wchar_t,...>::__init`, `append`, `reserve` and the
destructor. Those names are mangled identically to the ones in the prebuilt
`libc++`, which was built with a 32-bit `wchar_t`. The link succeeds and the
string internals are then written by 16-bit code and read by 32-bit code. It
fails at runtime, silently, not at build time.

So `wchar_t` stays native and 32-bit, `WORD`/`BSTR`/`OLECHAR` stay UTF-16, and
the boundary between them is a real transcode in `comutil.h`. The compiler finds
every mixing site for us, which is the point of choosing this way round.

## What the compatibility layer grew

Beyond the original shims, all of it driven by what the compiler actually asked
for rather than by guessing at Win32:

| File | Provides |
| --- | --- |
| `bk1_win32_platform.h` | Threads, events, recursive critical sections, `Sleep`, `GetTickCount`, `QueryPerformance*`, `LoadLibrary` on `dlopen` |
| `bk1_win32_files.h/.cpp` | `FindFirstFile` on `opendir`+`fnmatch` with `FNM_CASEFOLD`, attributes, `FILETIME` and the FAT date conversions |
| `bk1_win32_fileio.h/.cpp` | The handle-based API: `CreateFile`, `ReadFile`, `SetFilePointer`, `GetFileInformationByHandle` and friends |
| `bk1_win32_strings.h/.cpp` | `GetACP`, `MultiByteToWideChar`, `WideCharToMultiByte` with real CP1251 and CP1252 tables, `OutputDebugString`, `_itoa` |
| `comutil.h` | `VARIANT` in the Windows layout, `BSTR` with its length prefix, `_bstr_t`, and the UTF-16/wchar_t transcode |
| `io.h`, `direct.h`, `comdef.h` | The MSVC headers the sources include by name |

Case folding in `FindFirstFile` is not a nicety: the data was authored on a
case-insensitive filesystem and Android's is not.

`GetACP` defaults to **1251** and `Bk1SetAnsiCodePage` overrides it. Which page
the shipped `Versions/Current` data actually uses has not been verified yet, and
getting it wrong garbles every non-ASCII string in the game.

## Source defects fixed

All kept compiling under MSVC by guarding on `_MSC_VER` / `_M_IX86` rather than
by deleting the original:

- **Inline assembly**, 50 statements in 6 files — the `Sign`, `Min`/`Max`,
  `select_*`, `MemSet*`, `Copy8/16/32Bytes`, `Float2Int`, `CheckForViewingFrustum`
  and `GetCPUID` bodies now have an arm64 path beside the x86 one. `Float2Int`
  became `lrintf`, not a cast: `fistp` rounds to nearest, and truncating would
  change gameplay arithmetic. `GetCPUID` returns 0 — it has no call site outside
  `Tools.h`.
- **`Misc/HPTimer.cpp`** counted RDTSC ticks and calibrated them against
  `QueryPerformanceCounter`. There is no user-space cycle counter on arm64, so
  the counter is `steady_clock` nanoseconds and the calibration loop is skipped.
- **`CVec4` and `SPlane`** each declared a union member twice (`w`, `d`). The
  duplicate is dropped; the layout is unchanged because both arms started at the
  same offset. `SPlane`'s four-float constructor also initialised across two
  union arms and now assigns in the body.
- **`Misc/Tools.h`** redefined `fabs`, `cos`, `sin`, `acos`, `asin` for `float`,
  which C++11 `<cmath>` already declares.
- **`Misc/Basic.h`**'s pointer macro called `Set`/`GetPtr` unqualified from a
  dependent base — 10 lines now say `this->`.
- **`StreamIO/DBIO.h`** passed member functions as `IDataTable::GetInt`; standard
  C++ wants `&IDataTable::GetInt`. 20 sites.
- **`typename`** before dependent types, in `SSHelper.h`, `DTHelper.h` and
  `VarSystemInternal.h`. Inserted from clang's own diagnostics rather than by
  pattern-matching, so only genuinely dependent names were touched.

## What blocks the next step

- **MSXML.** `StreamIO/DataTreeXML.h` opens with `#import "msxml.dll"`, which is
  MSVC generating COM wrappers from a type library at compile time. Four
  translation units depend on it. This one is a replacement, not a port: the XML
  data-tree reader has to be rewritten against a portable parser.
- **`mmsystem.h`** — one unit, the multimedia timer and `WAVEFORMATEX`.
- More anonymous-struct redeclarations of the `CVec4` kind, and the remaining
  `use of undeclared identifier` cases in `StreamIO`, which are Win32 calls the
  layer has not been asked for yet.
- `Misc/HashFuncs.h` casts pointers to `int` (`reinterpret_cast<int>( pData )`).
  It has not blocked a unit yet, but it will on arm64 and must widen.

## Data

`Versions/Current` carries the complete retail runtime: `game.exe`, the engine
DLLs, and 2.5 GB of `Data` with Scenarios, Maps, Units, Squads, Terrain, Music,
Movies and Textes. Unlike the Blitzkrieg 2 tree, no campaign descriptors appear
to be missing. FMOD and Bink are the proprietary audio and video layers, both of
which the Blitzkrieg 2 port already replaced with Oboe and a native decoder.

## Lineage, settled

The simulation is not merely similar to Blitzkrieg 2's — it is the same file,
edited. `AILogic/UnitsSegments.h` is byte-identical between the two trees, and
`CAIUnit::operator&` in both assigns the same arbitrary serialization slot
numbers to the same fields (5 = timeToDeath, 6 = player, 11 = fCamouflage,
15 = fHitPoints, 22 = pTankPit, 40 = bFreeEnemySearch), with slot 16 commented
out in Blitzkrieg 2 and later fields appended from 53 up. Arbitrary numbering
matched to arbitrary fields is not convergent design.

Below the simulation the two share nothing: `IDataStream` against
`CDataStream`, `IStorage` against `NVFS::IVFS`, `StreamIO/DataBase.h` against
`NDb::Get`.

## What the Blitzkrieg 2 Android port gives us

About a fifth of its 43 564 lines of C++ — roughly 9 000 — carries over.

Essentially unchanged: the whole audio stack (the software mixer, the Oboe
output, the RIFF/WAVE and MS-ADPCM decoder — and Blitzkrieg 1's shipped voice
files are MS-ADPCM too — the PCM ring and the Vorbis streamer), the platform
and path layers, and the case-insensitive path resolution in the VFS, which is
the single most valuable piece for a Windows game on a case-sensitive
filesystem.

With rework: the EGL/bgfx bootstrap and the 2D textured-quad batcher, the frame
pacer and lifecycle loop, the DXT decoder, and the video bridge — Blitzkrieg 1
ships Bink as well.

Not at all: everything named for Blitzkrieg 2's content — the single-player and
menu runtimes, the mission tracker, the `.xdb` descriptor plumbing, the libdb
bridge, and the Granny converter, which has nothing to convert here.

The build scaffolding transfers wholesale: the Gradle signing and NDK
configuration, the CMake game-activity/Oboe/bgfx setup, and the manifest.
