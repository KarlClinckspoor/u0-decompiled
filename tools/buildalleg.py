#!/usr/bin/env python3
"""Build Allegro 2.2's liballeg.a as AK.EXE's was built: build/alleg/LIB/DJGPP/LIBALLEG.A.

    python tools/buildalleg.py

Copies build/tc/ALLEGRO (from tools/setup.py) to build/alleg and runs its own makefile
(`make lib`: gcc 2.7.2.1 -m486 -O3 -ffast-math -fomit-frame-pointer) in DOSBox, with
binutils 2.7 first on gcc's COMPILER_PATH: AK.EXE's Allegro objects carry 2.7's alignment
fillers. make 3.75 does not find asmdef.c through VPATH for its explicit rule, so asmdef.exe is
built and run by hand first (the same command the makefile would run). About 45 seconds.
tools/libmatch.py then shows all 40 Allegro objects of AK.EXE identical.
"""
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import dos  # noqa: E402

SRC = os.path.join(ROOT, 'build', 'tc', 'ALLEGRO')
DST = os.path.join(ROOT, 'build', 'alleg')
FLAGS = '-I. -Isrc -Isrc/djgpp -Iobj/djgpp -Wall -Werror -m486 -O3 -ffast-math -fomit-frame-pointer'


def main():
    if not os.path.isdir(SRC):
        raise SystemExit('missing %s: run tools/setup.py first' % SRC)
    if os.path.isdir(DST):
        shutil.rmtree(DST)
    shutil.copytree(SRC, DST)
    t = time.time()
    log = dos.run(DST, ['SET COMPILER_PATH=D:/BNU27/BIN',
                        'gcc %s -s -o obj/djgpp/asmdef.exe src/asmdef.c' % FLAGS,
                        'obj\\djgpp\\asmdef.exe obj/djgpp/asmdef.h',
                        'make lib'], timeout=3000)
    open(os.path.join(ROOT, 'build', 'alleg_make.log'), 'w').write(log)
    lib = os.path.join(DST, 'LIB', 'DJGPP', 'LIBALLEG.A')
    if not os.path.exists(lib):
        print(log[-3000:])
        raise SystemExit('make lib failed (log in build/alleg_make.log)')
    print('built %s (%d bytes) in %d s; log in build/alleg_make.log' % (
        os.path.relpath(lib, ROOT), os.path.getsize(lib), time.time() - t))


if __name__ == '__main__':
    sys.exit(main())
