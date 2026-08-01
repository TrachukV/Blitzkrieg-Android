# The commands a touchscreen cannot reach yet

The gesture layer gives the game a pointer: a tap is a left click, a held finger
is a right click, a drag is the selection box, two fingers scroll and pinch.
That is enough to select units and order them about, and it is what the port
plays with today.

It is not enough to play Blitzkrieg. The game's orders — attack, guard, ambush,
formation, board, entrench — are keyboard commands, and a touchscreen has no
keyboard. Worse, the game *says so*: 85 files of player-facing text name a
specific key or button.

## What the text names, measured

Counted across `Data/Textes` and `Data/Scenarios`, in files the player reads:

| Named | Times | Reachable by touch today |
| --- | --- | --- |
| right-click | 52 | yes — a held finger |
| left-click | 12 | yes — a tap |
| double-click | 4 | yes |
| "mouse" | 5 | the word means nothing here |
| letter keys (`<Z>` `<X>` `<A>` `<C>` `<W>` …) | 53 | **no** |
| function keys, `<TAB>`, `<SPACE>` | 20 | **no** |
| `CTRL` / `SHIFT` / `ALT` | 21 | **no** |

Those counts are what matters; the conclusion I first drew from them was wrong,
and the section below says why.

## Correction: the orders are already reachable

An earlier version of this note said the orders could not be issued from a
touchscreen. That was wrong, and the mistake was reading the key bindings and
stopping there.

Neither `config.cfg` nor `defconf.cfg` binds a single gameplay order -- both
carry the same twelve commands, and all of them are developer tools: wireframe,
statistics, console, screenshot. What the config *does* mention is
`action_ui_button_`, and that is the answer: in this build the orders are issued
from the game's own command panel, the grid of buttons at the bottom of the
mission screen. Move, stop, dig in, attack, aggressive move, rotate, unload,
board -- eight of them, on screen, in the original's own interface.

They answer a finger. Tapping one raises the game's tooltip for it, which is
what a button under a pointer does.

So there is no panel to build. What is left is narrower and entirely about
words.

## What the text still gets wrong

The tooltip that appeared under the finger names a hotkey -- `[R]` -- and the
mission hints name `<A>`, `<CTRL>` and right-click. A player on a touchscreen
reads an instruction to press a key that does not exist, while the command it
describes is sitting on the panel a centimetre away.

That is the work: the 85 files counted above, rewritten to name the panel button
or the gesture instead of the key. The counts and the delivery mechanism below
still stand.

The engine's own name-to-command tables (`GameTT/WorldClient.cpp`,
`GameTT/iMissionInternal.cpp`) are still worth recording, because a future
gesture shortcut -- a two-finger tap for attack-move, say -- can ask for a
command by name rather than forging a key:

    action_move        action_attack      action_stop        action_guard
    action_ambush      action_follow      action_swarm       action_rotate
    action_formation   action_board       action_leave       action_install
    action_uninstall   action_ranging     action_suppress    action_stand_ground

## Delivering the text without touching the player's data

The port ships no game content and should not start now. The engine has a mod
file system (`STORAGE_TYPE_MOD`, `StreamIO/ModFileSystem.cpp`) and the port
already opens its storage that way, so adapted strings can ride inside the APK
and be mounted over the data at a higher priority. The player's own files stay
exactly as they were, and a port that is uninstalled leaves nothing behind.

## Tap to order: feasible, and here is the hinge

The one piece of the familiar scheme still missing is the one every mobile
strategy game has: with units selected, a tap on the ground orders them there.
The port asks for a held finger instead, which works but is a beat slower than
players expect.

It needs the port to tell three cases apart, and both tests exist:

- **Is the finger on the interface?** `IUIScreen::PickElement( pos, recursion )`
  answers it. A tap on the command panel or the minimap must stay a left click.
- **Is the finger on an object?** `CInterfaceMission::PickObjects( list, pos,
  type, bVisible )` answers it. A tap on one of your own units should select it,
  not order the rest to attack it.
- **Everything else is ground**, and a tap there is the order -- the right click
  the port sends today only after a hold.

There is one gap, and it is worth naming exactly so nobody looks for a getter
that is not there: `IMainLoop` offers `SetInterface`, `PushInterface` and
`PopInterface`, and no way to read the interface that is current. The port
cannot reach `CInterfaceMission` by asking. So the way in is the other
direction -- the mission hands the port a pointer to itself when it starts,
guarded on `_MSC_VER` like every other change to these sources.

With that hook the rest is small: an ownership test on whatever `PickObjects`
returns, and a branch in the gesture bridge. Written down because finding these
three things took longer than using them will.

## Status

The panel needed no building -- the game has one and it answers touch. Tap to
order is built: the mission hands the port a pointer to itself once it is
running, and a tap asks what it landed on before deciding what it means.

The text is still the game's own, and now that is closer to right than
rewriting it would have been: the keys it names can be pressed, and the orders
it describes can be tapped for.
