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

## What blocks the next step

Inline x86 assembly: **47 statements in 4 files** — `Misc/Tools.h`,
`Misc/Basic.h`, `Misc/HPTimer.cpp` and `Scene/FastSinCos.h`. All of it is
2003-era micro-optimisation with well-defined portable equivalents: `Sign`,
`Min`/`Max`, the `select_*` family, `MemSetDWord`/`MemSetInt`,
`Copy8/16/32Bytes`, `Float2Int`, `GetCPUID`, an RDTSC timer and fast sin/cos.
Each needs an arm64 path beside the x86 one, keeping the original build intact.
`Float2Int` in particular rounds to nearest rather than truncating, so a plain
C cast would change gameplay arithmetic.

Two more source-level defects are confirmed:

- `Misc/HashFuncs.h` casts pointers to `int` in three places
  (`reinterpret_cast<int>( pData )`, `int( a.GetPtr() )`), which does not
  compile on arm64 and must widen.
- `Misc/Geometry.h`'s `CVec4` anonymous union declares `w` twice, in
  `struct { float x, y, z, w; }` and again in `struct { float u, v, q, w; }`.
  MSVC 6 accepted it; clang rejects the union and the rejection poisons every
  downstream member initialiser.
- `Misc/Tools.h` redefines `fabs`, `cos`, `sin`, `acos` and `asin` for `float`
  at global scope. C++11 added those overloads to `<cmath>`, so libc++ already
  has them and the engine's collide. They need guarding on non-MSVC.

Compiling `Misc/StrProc.cpp` today leaves 20 errors: 14 inline-assembly, 5
`<cmath>` collisions, and the error limit. Both classes are inventoried above.

A full-tree attempt over all 170 `AILogic/*.cpp` — with backslash includes
rewritten, the shim force-included and STLport aliased — compiled none of them.
That number is inflated by a harness without the vendored STLport or a
precompiled header, but two contributions are not harness-dependent: the 18
inline-assembly errors `Misc/Tools.h` injects into every translation unit, and
the `CVec4` union, which alone accounted for most of the errors in the files
where it was measured.

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
