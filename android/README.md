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

The renderer is **Direct3D 8** — `Sources/src/GFX` carries `GeometryBuffer`,
`GeometryMesh`, `Shader` and `GraphicsEngine`, with `IDirect3DDevice8` behind
them. This is a 3D engine with an isometric camera, not a sprite blitter, which
is the usual assumption about this game. `GFX` is only ~9 000 lines, small
enough to reimplement on bgfx rather than port.

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
| `hash_map`, `hash_set` | STLport put these in the global namespace; aliased onto `std::unordered_*`, whose template parameters sit in the same order |
| `comutil.h` | `_variant_t` and `_bstr_t`, the only COM types the engine's property system uses |
| `windows.h`, `bk1_win32_types.h` | The Win32 scalar types, handles and macros `StdAfx.h` reaches for |
| `imagehlp.h` | BugSlayer's declarations, included from every module's `StdAfx.h` |
| `bk1_msvc_types.h` | Force-included ahead of every unit |

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

One 64-bit defect is already visible: `Misc/HashFuncs.h` casts pointers to
`int` (`reinterpret_cast<int>( pData )`), which does not compile on arm64 and
must widen.

## Data

`Versions/Current` carries the complete retail runtime: `game.exe`, the engine
DLLs, and 2.5 GB of `Data` with Scenarios, Maps, Units, Squads, Terrain, Music,
Movies and Textes. Unlike the Blitzkrieg 2 tree, no campaign descriptors appear
to be missing. FMOD and Bink are the proprietary audio and video layers, both of
which the Blitzkrieg 2 port already replaced with Oboe and a native decoder.
