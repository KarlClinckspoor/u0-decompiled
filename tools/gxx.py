#!/usr/bin/env python3
"""Compile one C/C++ file with gcc 2.7.2.1 in DOSBox and compare its functions with AK.COF.

    python tools/gxx.py FILE.cc [--flags="FLAGS"] [--bnu27] [-v]

FLAGS defaults to the game's; give them with = so argparse keeps the dash: --flags="-O2 -g".
--bnu27 assembles with binutils 2.7 (as Allegro's objects were) instead of 2.8.1.

Every function symbol of the object is looked up by its (mangled) name in AK.COF and compared
over its whole length, up to the next function symbol (so the literals after a function count
too). Relocated 32-bit fields are masked on both sides. The first difference is shown as
disassembly of both sides (needs capstone; plain hex otherwise).

Exit status: 0 when every function matches.
"""
import argparse
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import coff  # noqa: E402
import dos   # noqa: E402

GAME_FLAGS = '-O0'   # the game was compiled without optimization (docs/MATCHING.md)
# gas 2.8.1's alignment fillers of 1-3 bytes (functions are 4-aligned at -O0 without -m486)
FILLERS = (b'\x90', b'\x89\xf6', b'\x8d\x76\x00')
WORK = os.path.join(ROOT, 'build', 'gxx')

try:
    import capstone
    _md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
except ImportError:
    _md = None


def disasm(b, addr, n=6):
    if _md is None:
        return ['  %06X %s' % (addr, b[:16].hex(' '))]
    out = []
    for i in _md.disasm(b, addr):
        out.append('  %06X %-20s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
        if len(out) >= n:
            break
    return out


def compile_file(src, flags, bnu27=False):
    os.makedirs(WORK, exist_ok=True)
    base = os.path.basename(src).lower()
    shutil.copy(src, os.path.join(WORK, base))
    obj = os.path.splitext(base)[0] + '.o'
    if os.path.exists(os.path.join(WORK, obj.upper())):
        os.unlink(os.path.join(WORK, obj.upper()))
    b = '-BD:/BNU27/BIN/ ' if bnu27 else ''
    log = dos.run(WORK, ['gcc %s%s -c %s -o %s' % (b, flags, base, obj)])
    path = os.path.join(WORK, obj.upper())
    if not os.path.exists(path):
        print(log)
        raise SystemExit('compile failed')
    msgs = [l for l in log.splitlines() if not l.startswith('## ')]
    if msgs:
        print('\n'.join(msgs))
    return path


def compare(objpath, ak, verbose=False):
    o = coff.Coff(open(objpath, 'rb').read())
    text = o.raw('.text')
    _, mask = coff.masked(text, o.relocs('.text'))
    funcs = sorted((s for s in o.syms if s.sclass in (2, 3) and s.sec == 1
                    and not s.name.startswith('.') and s.name not in coff.MARKERS),
                   key=lambda s: s.value)
    bynames = {s.name: s for s in ak.syms if s.sclass in (2, 3, 6) and s.sec == 1}
    ok = True
    for k, s in enumerate(funcs):
        start = s.value
        end = funcs[k + 1].value if k + 1 < len(funcs) else len(text)
        mine = text[start:end]
        ref = bynames.get(s.name)
        label = coff.dm(s.name)
        if ref is None:
            print('  ?        %-50s not in AK.COF' % label)
            ok = False
            continue
        orig = ak.read(ref.value, len(mine))
        diff = [i for i in range(len(mine)) if (start + i) not in mask and mine[i] != orig[i]]
        if not diff:
            print('  MATCH    %-50s %d bytes' % (label, len(mine)))
            continue
        # the last function of a partial file ends in the object's closing alignment filler,
        # where the original goes on with more literals or functions: that is still a match
        tail = mine[diff[0]:]
        if end == len(text) and tail in FILLERS:
            print('  MATCH    %-50s %d bytes (+%d bytes of closing filler, the original goes on)'
                  % (label, diff[0], len(tail)))
            continue
        ok = False
        d = diff[0]
        print('  DIFF     %-50s %d bytes, %d differ, first at +%X' % (label, len(mine), len(diff), d))
        if True:
            back = max(0, d - 8)
            print('   mine:')
            print('\n'.join(disasm(mine[back:], ref.value + back)))
            print('   AK:')
            print('\n'.join(disasm(orig[back:], ref.value + back)))
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('file')
    ap.add_argument('-O', '--flags', default=GAME_FLAGS)
    ap.add_argument('--bnu27', action='store_true')
    ap.add_argument('-v', '--verbose', action='store_true')
    a = ap.parse_args()
    ak = coff.Coff(open(os.path.join(ROOT, 'build', 'AK.COF'), 'rb').read())
    obj = compile_file(a.file, a.flags, a.bnu27)
    return 0 if compare(obj, ak, a.verbose) else 1


if __name__ == '__main__':
    sys.exit(main())
