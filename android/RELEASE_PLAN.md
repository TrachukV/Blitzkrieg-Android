# What is left before this is releasable

Written after a day of measurement, not from a template. Ordered by what blocks
a player, not by what is interesting to fix.

Two things decide the order. A defect that stops a player finishing a mission
outranks a defect that annoys them. And anything that cannot be measured on the
emulator has to be batched, because each round trip through a real device costs
a message to the person holding it.

## 1. Blocking: the game must survive a session

| # | Defect | State |
|---|--------|-------|
| 1.1 | SIGSEGV when a selected unit is queried through Lua | located to a pointer truncated to 32 bits and back; one fix applied, crash persists with a different fault address, so the fix is unproven |
| 1.2 | Save cannot be confirmed | the Confirm button raises no command; seven explanations measured and discarded; the click reaches the dialog and the list selects, so it is the button's own command path |
| 1.3 | Load aborts | diagnosed to an object of the wrong type in the update queue; eight explanations discarded |
| 1.4 | Manual save writes nothing | not investigated |

None of these is guesswork away from a fix. 1.1 has a fault address that names
the mechanism; 1.2 has a probe already in place that fires on everything except
the button; 1.3 has the failing object identified.

## 2. Blocking: the campaign has to be playable end to end

* Play each of the three campaigns' first three missions on a real device.
* Any mission that cannot be finished is a defect above this line, not below.
* This is hours of play and cannot be done by script. It needs the person with
  the phone.

## 3. Adaptation still owed

| # | Item | State |
|---|------|-------|
| 3.1 | The strip along the bottom edge | window, surface and viewport all report the full screen here; the strip does not reproduce on the emulator, so it is unverified and open |
| 3.2 | Two-finger box selection | written, never once executed -- SELinux refuses injected multi-touch, so it has never run |
| 3.3 | Firing verified by hand | never tested |
| 3.4 | Unit-to-unit collision | needs a group, which needs 3.2 |

## 4. Release mechanics, none of it started

* A signing key, and a release build signed with it rather than the debug key.
* Data cannot ship inside the APK -- it is 2.6 GB and belongs to the rights
  holder. The port loads it from external storage, so a release needs a first-run
  screen that says where to put it, and a clear failure when it is absent
  instead of a crash.
* Blitzkrieg is not ours to distribute. The port is a source project; the
  binary needs the player's own copy of the game. That has to be stated plainly
  in the README and on any page that offers a build.
* Minimum device, and what happens below it: the engine wants ~600 MB of data
  resident and GLES 3.
* An icon and a name that do not claim to be the original publisher's.

## 5. Quality, after the above

* Sound has never been checked beyond "it starts".
* Frame rate on a real device is unmeasured; every fps number so far is the
  emulator's.
* Battery and heat over a 30-minute mission.
* The 4:3 interface stretched to a phone's aspect: accepted deliberately, but a
  letterboxed option costs little and some players will want it.

## How the remaining work has to be run

The emulator has cost more time than it saved on every item in section 3. It
does not reproduce the strip, it refuses multi-touch, it floods the log so
probes vanish, and it crashes where a phone may not. Everything in sections 2
and 3 should be measured on the device, in batches, with the questions written
down before the build is handed over -- not one question per round trip.

Sections 1 and 4 can be done here.
