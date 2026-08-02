#!/usr/bin/env python3
"""Rewrite the game's instructions for a device with no keyboard.

Blitzkrieg's tutorials and mission briefings tell the player which key to press:
hold <CTRL> and right-click to force an attack, press <A> to move ready to
fight, <F1> for help, <TAB> for the objectives. On a phone none of those exist,
so the instruction is not merely awkward -- it is wrong, and a player following
it is stuck.

This rewrites those sentences to name what the player actually has: the buttons
the port draws down the left edge (A attack, M move, Q queue, C centre) and the
taps that replace the mouse.

The files are UTF-16LE, which is how the engine stores its wide strings, and are
rewritten in the same encoding. Nothing else about them is touched.

Order matters below: the longer, more specific phrases are replaced first, so
that "hold the <CTRL> key and right-click" does not get half-rewritten by the
plain "<CTRL>" rule before its own rule is reached.
"""

import os
import re
import sys

# (pattern, replacement). Case-insensitive, applied in order.
RULES = [
    # Whole instructions first -- these read as sentences and must survive as
    # sentences rather than as a key name swapped inside keyboard grammar.
    (r"hold\s+the\s+<CTRL>\s+key\s+and\s+right-?click\s+at\s+the\s+desired\s+position\s+to\s+issue\s+aggressive\s+movement",
     "press the M button on the left and then tap the destination to move ready to fight"),
    (r"hold\s+the\s+<CTRL>\s+key\s+and\s+right-?click",
     "press the A button on the left and then tap"),
    (r"use\s+aggressive\s+movement\s*\(<A>\s*key\)",
     "use aggressive movement (the M button on the left)"),
    (r"aggressive\s+movement\s*\(<A>\s*key\)",
     "aggressive movement (the M button on the left)"),
    (r"call\s+the\s+help\s+window\s+anytime\s+with\s+the\s+<F1>\s+key",
     "open the help window anytime from the menu"),
    (r"the\s+objectives\s+window\s+with\s+the\s+<TAB>\s+key",
     "the objectives window with the Show Last Objective button"),
    (r"press\s+the\s+button\s+at\s+the\s+bottom\s+or\s+the\s+<ESC>\s+key",
     "press the button at the bottom or the Back gesture"),

    # Then the bare key names, for the sentences not covered above.
    (r"<CTRL>\s+key", "A button on the left"),
    (r"<SHIFT>\s+key", "Q button on the left"),
    (r"<ALT>\s+key", "M button on the left"),
    (r"<A>\s+key", "M button on the left"),
    (r"<TAB>\s+key", "Show Last Objective button"),
    (r"<F1>\s+key", "help window"),
    (r"<ESC>\s+key", "Back gesture"),
    (r"<ENTER>\s+key", "confirm button"),

    # Finally the mouse, which a finger replaces outright.
    (r"right-?click", "tap the ground"),
    (r"left\s+click", "tap"),
    (r"left-?click", "tap"),
    (r"double\s+click", "double tap"),
]

COMPILED = [(re.compile(p, re.I), r) for p, r in RULES]


def rewrite(text):
    changed = 0
    for rx, rep in COMPILED:
        text, n = rx.subn(rep, text)
        changed += n
    return text, changed


def main(roots):
    files = 0
    edits = 0
    for root in roots:
        if not os.path.isdir(root):
            print("missing:", root, file=sys.stderr)
            continue
        for dirpath, _dirnames, filenames in os.walk(root):
            for name in filenames:
                path = os.path.join(dirpath, name)
                try:
                    raw = open(path, "rb").read()
                except OSError:
                    continue
                # Only the wide-string text files; anything else is left alone.
                if len(raw) < 2 or raw[1] != 0:
                    continue
                try:
                    text = raw.decode("utf-16-le")
                except UnicodeDecodeError:
                    continue
                new, n = rewrite(text)
                if n == 0:
                    continue
                with open(path, "wb") as f:
                    f.write(new.encode("utf-16-le"))
                files += 1
                edits += n
    print(f"rewritten {edits} instructions across {files} files")


if __name__ == "__main__":
    main(sys.argv[1:] or ["Versions/Current/Data/Scenarios"])
