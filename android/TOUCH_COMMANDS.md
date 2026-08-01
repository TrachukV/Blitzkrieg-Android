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

Rewriting that text to say "tap" while the command has no touch route would be
describing controls that do not exist. So the order of work is: give the
commands a route first, then make the text name it.

## What has to be reachable

The engine already resolves these by name — `GameTT/WorldClient.cpp` and
`GameTT/iMissionInternal.cpp` each carry a table of name → command. That is the
good news: an on-screen control does not have to forge a key press, it can ask
for the command the same way a binding does.

Unit orders (`WorldClient.cpp`):

    action_move        action_attack      action_stop        action_guard
    action_ambush      action_follow      action_swarm       action_rotate
    action_formation   action_board       action_leave       action_install
    action_uninstall   action_ranging     action_suppress    action_stand_ground
    action_hook_artillery                 select_by_type

Mission-level (`iMissionInternal.cpp`):

    show_objectives    show_help_screen   show_escape_menu   show_save_menu
    show_status_bar    select_next_object reset_selection    show_avia_buttons
    force_action_move_on/off              force_action_attack_on/off
    add_action_on/off  clear_screen_acks

The modifier-style ones matter as much as the orders: `add_action_*` is how a
player queues an order rather than replacing it, and `force_action_*` is how an
order is forced onto ground that would otherwise mean something else. On a
keyboard they are held modifiers. On a touchscreen they have to become state —
a button that stays down — because there is no second hand.

## Delivering the text without touching the player's data

The port ships no game content and should not start now. The engine has a mod
file system (`STORAGE_TYPE_MOD`, `StreamIO/ModFileSystem.cpp`) and the port
already opens its storage that way, so adapted strings can ride inside the APK
and be mounted over the data at a higher priority. The player's own files stay
exactly as they were, and a port that is uninstalled leaves nothing behind.

## Status

Scoped, not built. The measurements above are real; the panel is not written.
