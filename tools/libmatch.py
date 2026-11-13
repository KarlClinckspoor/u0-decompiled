#!/usr/bin/env python3
"""Pair every object of AK.COF with a library member and check that they are identical.

    python tools/libmatch.py [--md] [--quiet]

Libraries (from build/tc, see tools/setup.py; liballeg.a from build/alleg after `make lib`):
  crt0.o, libc.a, libm.a, libgcc.a, libiostr.a (DJGPP 2.01 / gcc 2.7.2.1 / libg++ 2.7.1)
  liballeg.a (Allegro 2.2, built by phase 3)

A member is paired with the AK.COF object that holds its first global symbol, at the same
offset from the start of the section. Pairing by symbol, not by file name, finds the libc
`*.stub` objects and the assembly objects that AK.COF lists as `fake`. Each pair's .text and
.data are compared with the member's relocated 32-bit fields masked on both sides, and the
member's section sizes must equal the object's.

Exit status 0 when every library object of AK.COF is paired and identical.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import coff  # noqa: E402

LIB = os.path.join(ROOT, 'build', 'tc', 'DJGPP', 'LIB')
LIBRARIES = [
    ('crt0.o', os.path.join(LIB, 'CRT0.O')),
    ('libc.a', os.path.join(LIB, 'LIBC.A')),
    ('libm.a', os.path.join(LIB, 'LIBM.A')),
    ('libgcc.a', os.path.join(LIB, 'LIBGCC.A')),
    ('libiostr.a', os.path.join(LIB, 'LIBIOSTR.A')),
    ('liballeg.a', os.path.join(ROOT, 'build', 'alleg', 'LIB', 'DJGPP', 'LIBALLEG.A')),
]
GAME = ('ak.cc', 'akaux.cc', 'akchar.cc', 'akmenu.cc', 'akworld.cc')


def members():
    for lib, path in LIBRARIES:
        if not os.path.exists(path):
            print('(missing %s: %s)' % (lib, path))
            continue
        data = open(path, 'rb').read()
        if data[:8] == b'!<arch>\n':
            for name, body in coff.ar_members(data):
                yield lib, name, coff.Coff(body)
        else:
            yield lib, os.path.basename(path).lower(), coff.Coff(data)


def section_start(obj, secnum):
    return {1: obj.text, 2: obj.data, 3: obj.bss}[secnum]


def compare(ak, ob, m):
    """None when identical, else a short reason."""
    for name, rng in (('.text', ob.text), ('.data', ob.data)):
        raw = m.raw(name)
        size = rng[1] if rng else 0
        if len(raw) != size:
            return '%s size %d, original %d' % (name, len(raw), size)
        if not size:
            continue
        mine, mask = coff.masked(raw, m.relocs(name))
        orig = ak.read(rng[0], size)
        om = bytes(0 if i in mask else orig[i] for i in range(size))
        if mine != om:
            first = next(i for i in range(size) if mine[i] != om[i])
            return '%s differs from +%X (%d bytes)' % (name, first, sum(1 for i in range(size) if mine[i] != om[i]))
    bss = m.section('.bss')
    want = ob.bss[1] if ob.bss else 0
    if (bss['size'] if bss else 0) != want:
        return '.bss size %d, original %d' % (bss['size'] if bss else 0, want)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--md', action='store_true')
    ap.add_argument('--quiet', action='store_true', help='list only problems')
    a = ap.parse_args()
    ak = coff.Coff(open(os.path.join(ROOT, 'build', 'AK.COF'), 'rb').read())
    akglob = {}
    for s in ak.syms:
        if s.sclass == 2 and s.sec in (1, 2, 3):
            akglob[s.name] = s
    pairs = {}
    for lib, name, m in members():
        for s in m.syms:
            if s.sclass != 2 or s.sec not in (1, 2, 3):
                continue
            t = akglob.get(s.name)
            if t is None or t.sec != s.sec or t.obj is None:
                continue
            rng = section_start(t.obj, s.sec)
            msec = m.sections[s.sec - 1]
            if rng and t.value - rng[0] == s.value - msec['vaddr']:
                pairs.setdefault(id(t.obj), []).append((lib, name, m))
            break
    ok = True
    rows = []
    for k, ob in enumerate(ak.files):
        cands = pairs.get(id(ob), [])
        if ob.name in GAME:
            rows.append((k, ob.name, 'game', '', 'to be written in C++'))
            continue
        if not cands:
            empty = not any(r and r[1] for r in (ob.text, ob.data, ob.bss))
            rows.append((k, ob.name, '', '', 'empty object, nothing to pair' if empty else 'NOT PAIRED'))
            ok = ok and empty
            continue
        results = [(lib, name, compare(ak, ob, m)) for lib, name, m in cands]
        good = [r for r in results if r[2] is None]
        lib, name, why = good[0] if good else results[0]
        rows.append((k, ob.name, lib, name, 'identical' if why is None else why))
        ok = ok and why is None
    for k, obj, lib, mem, res in rows:
        if a.quiet and res in ('identical', 'to be written in C++', 'empty object, nothing to pair'):
            continue
        if a.md:
            print('| %d | %s | %s | %s | %s |' % (k, obj, lib, mem, res))
        else:
            print('%3d %-16s %-10s %-16s %s' % (k, obj, lib, mem, res))
    counts = {}
    for _k, _o, lib, _m, res in rows:
        key = (lib or '-', res if res in ('identical', 'to be written in C++') else 'other')
        counts[key] = counts.get(key, 0) + 1
    print('summary:', ', '.join('%s %s: %d' % (l, r, n) for (l, r), n in sorted(counts.items())))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
