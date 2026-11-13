#!/usr/bin/env python3
"""Readable disassembly of a game object or function of AK.COF, for writing the C++.

    python tools/listing.py OBJ [FUNC]       e.g. listing.py akmenu.cc runmenu

Shows each function with its demangled name, gcc's labels (L, LC), operands that are
addresses by symbol name (from tools/standin.py's analysis), string literals as text,
doubles as numbers, frame slots as [ebp-N], and switch tables. Needs capstone.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import coff     # noqa: E402
import standin  # noqa: E402


def text_of(b):
    s = b.split(b'\0')[0]
    out = ''
    for c in s:
        ch = chr(c)
        out += {'\\': '\\\\', '"': '\\"', '\n': '\\n'}.get(ch, ch if 32 <= c < 127 else '\\x%02x' % c)
    return '"' + out + '"'


def line_table(c):
    """vaddr -> source line, from the COFF line numbers (relative to each function's .bf)."""
    out = {}
    base = None
    for sec, addr, ln in c.lines:
        if ln == 0:
            fn = c.byindex[addr]
            base = None
            k = fn.index + 1 + len(fn.aux)
            while k in c.byindex and k < fn.index + 8:
                s = c.byindex[k]
                if s.name == '.bf':
                    base = struct.unpack_from('<H', s.aux[0], 4)[0]
                    break
                k += 1 + len(s.aux)
            out[fn.value] = base
        elif base is not None:
            out[addr] = base + ln - 1
    return out


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    objname = sys.argv[1]
    want = sys.argv[2] if len(sys.argv) > 2 else None
    ak = coff.Coff(open(os.path.join(ROOT, 'build', 'AK.COF'), 'rb').read())
    g = standin.Gen(ak, ak.obj(objname))
    g.explore()

    def name(v):
        if g.a <= v < g.b and v in g.own:
            return g.own[v].name
        e = g.ext_name(v) if g.lo <= v < g.hi else None
        if e:
            return coff.dm(e[0]) + ('+%d' % e[1] if e[1] else '')
        return None
    lines = line_table(ak)
    funcs = [(s.value, s) for s, _ in ak.functions(g.obj)]
    funcs += [(v, s) for v, s in g.own.items() if s.sclass == 6 and '__F' in s.name]   # inline copies
    funcs.sort()
    starts = [v for v, _ in funcs]
    show = want is None
    pc = g.a
    while pc < g.b:
        if pc in starts:
            s = dict(funcs)[pc]
            show = want is None or want in coff.dm(s.name)
            if show:
                print('\n%s:    ; %s' % (s.name, coff.dm(s.name)))
        lab = g.own.get(pc)
        if pc in g.code:
            i = g.code[pc]
            if show and pc in lines:
                print('; line %s' % lines[pc])
            if show:
                if lab and lab.sclass == 6 and not lab.name.startswith('_'):
                    print('%s:' % lab.name)
                ops = i.op_str
                for off in (i.imm_offset, i.disp_offset):
                    f = pc + off
                    if f in g.relocs and off:
                        v = g.relocs[f][1]
                        n = name(v)
                        if n:
                            ops = ops.replace(hex(v), n)
                            if v in g.own and g.own[v].name.startswith('LC'):
                                ops += '    ' + text_of(ak.read(v, 200))
                if i.mnemonic in ('call', 'jmp') or i.mnemonic.startswith('j'):
                    try:
                        v = int(i.op_str, 16)
                        n = name(v)
                        if n:
                            ops = n
                    except ValueError:
                        pass
                print('  %06X  %-8s %s' % (pc, i.mnemonic, ops))
            pc += i.size
            continue
        # data: literals, doubles, tables, fillers
        end = pc + 1
        while end < g.b and end not in g.code and end not in g.own and end not in starts:
            end += 1
        if show:
            if pc in g.tables:
                n = (end - pc) // 4
                print('  %06X  table   %s' % (pc, ', '.join(name(int.from_bytes(ak.read(pc + 4 * k, 4), 'little'))
                                                     or '?' for k in range(n))))
            elif lab and lab.name.startswith('LC'):
                raw = ak.read(pc, end - pc)
                for f in sorted(standin_fillers(), key=len, reverse=True):
                    if raw.endswith(b'\0' + f):     # alignment filler after the last string
                        raw = raw[:-len(f)]
                        break
                if end - pc == 8 and raw.count(0) < 7 and not raw.rstrip(b'\0').isascii():
                    print('%s:  double %r' % (lab.name, struct.unpack('<d', raw)[0]))
                else:
                    for k, part in enumerate(raw.split(b'\0')[:-1]):
                        print('%s  %s' % ((lab.name + ':') if k == 0 else ' ' * (len(lab.name) + 1), text_of(part)))
            else:
                raw = ak.read(pc, end - pc)
                if raw not in standin_fillers():
                    print('  %06X  data    %s' % (pc, raw.hex(' ')))
        pc = end


def standin_fillers():
    return (b'\x90', b'\x89\xf6', b'\x8d\x76\x00')


if __name__ == '__main__':
    main()
