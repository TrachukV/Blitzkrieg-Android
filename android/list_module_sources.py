#!/usr/bin/env python3
"""Prints a module's source files, one per line, as listed in its own .dsp.

The .dsp is the build file the game was actually built with, so it is the only
honest answer to "what is in this module". Globbing *.cpp sweeps in files that
were never in the build -- abandoned experiments, editor-only helpers -- and
hand-copied lists drift away from it.

CMake calls this at configure time and android/compile_check.sh calls it too,
so what is checked and what is built cannot disagree.

The files are ISO-8859-1 and carry high bytes in their comments, which is why
this reads bytes and decodes with latin-1 rather than trusting the locale.

    list_module_sources.py <Sources/src> <ModuleName>
"""
import os
import sys


def module_sources(sources_dir, module):
    dsp = os.path.join(sources_dir, module, module + '.dsp')
    if not os.path.isfile(dsp):
        raise SystemExit("no %s -- the module list comes from the .dsp" % dsp)

    found = []
    seen = set()
    with open(dsp, 'rb') as f:
        for raw in f:
            line = raw.decode('latin-1').strip()
            if not line.upper().startswith('SOURCE='):
                continue
            rel = line[len('SOURCE='):].strip().strip('"')
            # Paths are written .\Foo.cpp, sometimes .\Sub\Foo.cpp
            if rel.startswith('.\\') or rel.startswith('./'):
                rel = rel[2:]
            rel = rel.replace('\\', '/')
            if not rel.lower().endswith(('.cpp', '.c')):
                continue
            full = os.path.join(sources_dir, module, rel)
            # A .dsp may list a file that is not in this copy of the tree; the
            # caller is told what is really there, not what was claimed.
            if os.path.isfile(full) and full not in seen:
                seen.add(full)
                found.append(full)
    return found


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    for path in module_sources(sys.argv[1], sys.argv[2]):
        print(path)


if __name__ == '__main__':
    main()
