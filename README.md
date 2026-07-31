# Blitzkrieg — Android port

A native Android port of **Blitzkrieg** (Nival Interactive, 2003), the real-time
strategy game whose singleplayer source Nival released in 2025.

The approach is to compile the original C++ for arm64 and replace only the
platform layer, rather than rewriting the game. The goal is the game as it was —
its own menus, campaign, missions and art — with touch controls in place of
mouse and keyboard.

This repository is a fork of [nival/Blitzkrieg](https://github.com/nival/Blitzkrieg);
the original project README is kept as [README_Original.md](README_Original.md).

## State: not playable yet

This is honest work in progress, and nothing runs. What exists is the
compatibility layer, the platform replacements written so far, and the original
sources brought to the point where the compiler accepts them.

| Module | Translation units building for arm64 |
| --- | --- |
| `Misc`, `StreamIO`, `Formats`, `Image`, `Anim`, `Common` | complete |
| `zlib`, `GameSpy`, `LuaLib`, `RandomMapGen` | complete |
| `Net` | 11 / 15 |
| `Scene` | 29 / 50 |
| `UI` | 12 / 33 |
| **Total** | **~220** |

Still ahead, and each is a replacement rather than a port: Direct3D 8 (`GFX`),
DirectInput (`Input`), FMOD (`SFX`), and the `AILogic`, `Main`, `GameTT` and
`Game` modules, which have not been started.

```bash
android/compile_check.sh Misc StreamIO      # what builds, per module
android/tests/run_tests.sh                  # the tests that guard the rewritten parts
```

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
