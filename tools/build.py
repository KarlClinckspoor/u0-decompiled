#!/usr/bin/env python3
"""Build AK's COFF program and compare it with the original (build/AK.COF).

    python tools/build.py [-v]

For each game object, in link order (docs/MAP.md), the source is src/NAME.cc when it exists
(compiled -O0, binutils 2.8.1; ak.cc with -g), otherwise the stand-in src/standin/NAME.s
(tools/standin.py). The link is the one gcc 2.7.2.1 runs:

    ld -o ak crt0.o OBJS -lalleg -liostr -Tdjgpp.djl -lgcc -lc -lgcc

with liballeg.a from build/alleg (Allegro 2.2, built in phase 3; see docs/HANDOFF.md).
Output in build/link/. The comparison covers the file header, the a.out header, the section
headers (pointers to line numbers and symbols aside) and the bytes of .text and .data. The
symbol table is reported separately: stand-ins cannot reproduce it.

Exit status 0 when the sections are identical.
"""
import argparse
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import coff  # noqa: E402
import dos   # noqa: E402

OBJS = ['ak', 'akaux', 'akchar', 'akmenu', 'akworld']
CFLAGS = {'ak': '-O0 -g'}   # the others: -O0
WORK = os.path.join(ROOT, 'build', 'link')
LIBALLEG = os.path.join(ROOT, 'build', 'alleg', 'LIB', 'DJGPP', 'LIBALLEG.A')


def build():
    if os.path.isdir(WORK):
        shutil.rmtree(WORK)
    os.makedirs(WORK)
    if not os.path.exists(LIBALLEG):
        raise SystemExit('missing %s: build Allegro first (docs/HANDOFF.md, phase 3)' % LIBALLEG)
    shutil.copy(LIBALLEG, os.path.join(WORK, 'LIBALLEG.A'))
    cmds = []
    for name in OBJS:
        cc = os.path.join(ROOT, 'src', name + '.cc')
        if os.path.exists(cc):
            shutil.copy(cc, os.path.join(WORK, name + '.cc'))
            cmds.append('gcc %s -c %s.cc -o %s.o' % (CFLAGS.get(name, '-O0'), name, name))
        else:
            shutil.copy(os.path.join(ROOT, 'src', 'standin', name + '.s'), os.path.join(WORK, name + '.s'))
            cmds.append('as -o %s.o %s.s' % (name, name))
    cmds.append('ld -o ak D:/lib/crt0.o -LD:/lib -LC:/ %s -lalleg -liostr -Tdjgpp.djl -lgcc -lc -lgcc'
                % ' '.join(n + '.o' for n in OBJS))
    log = dos.run(WORK, cmds)
    msgs = [l for l in log.splitlines() if not l.startswith('## ')]
    if msgs:
        print('\n'.join(msgs))
    out = os.path.join(WORK, 'AK')
    if not os.path.exists(out):
        raise SystemExit('link failed')
    return out


def compare(path, verbose):
    new = coff.Coff(open(path, 'rb').read())
    old = coff.Coff(open(os.path.join(ROOT, 'build', 'AK.COF'), 'rb').read())
    ok = True

    def check(what, a, b):
        nonlocal ok
        same = a == b
        ok = ok and same
        print('  %-28s %s' % (what, 'identical' if same else 'DIFFERENT: %r / original %r' % (a, b)))
    # flag 4 (F_LNNO, "line numbers stripped") follows the line numbers: part of the symbol report
    check('file header (sans symbols)', (new.magic, len(new.sections), new.timestamp, new.flags & ~4),
          (old.magic, len(old.sections), old.timestamp, old.flags & ~4))
    check('a.out header', new.aout, old.aout)
    for sn, so in zip(new.sections, old.sections):
        keys = ('name', 'paddr', 'vaddr', 'size', 'scnptr', 'nreloc', 'flags')
        check('section header %s' % so['name'], tuple(sn[k] for k in keys), tuple(so[k] for k in keys))
        a, b = new.raw(so['name']), old.raw(so['name'])
        if a != b:
            ok = False
            n = min(len(a), len(b))
            first = next((i for i in range(n) if a[i] != b[i]), n)
            where = so['vaddr'] + first
            objs = [o.name for o in old.files if o.text and o.text[0] <= where < o.text[0] + o.text[1]]
            print('  %-28s DIFFERENT from %06X (%s), %d bytes differ' % (
                so['name'] + ' bytes', where, ', '.join(objs) or '?', sum(1 for i in range(n) if a[i] != b[i])))
        elif so['scnptr']:
            print('  %-28s identical (%d bytes)' % (so['name'] + ' bytes', len(a)))
    print('  symbol table: %d symbols (original %d), %d line numbers (original %d), flags %#x (original %#x)'
          % (new.nsyms, old.nsyms, len(new.lines), len(old.lines), new.flags, old.flags))
    print('  whole file: %s' % ('IDENTICAL' if new.data == old.data else 'differs (symbol table)'))
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-v', '--verbose', action='store_true')
    a = ap.parse_args()
    path = build()
    ok = compare(path, a.verbose)
    print('SECTIONS IDENTICAL to the original COFF' if ok else 'NOT identical')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
