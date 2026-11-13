#!/usr/bin/env python3
"""Reader for DJGPP COFF images (build/AK.COF from tools/wdosx.py) and objects.

usage:  coff.py info  FILE              headers, sections, symbol classes, debug info
        coff.py files FILE [--md]       every object in link order with its .text/.data/.bss
        coff.py funcs FILE [OBJ]        functions (of one object, e.g. akworld.cc) with sizes
        coff.py syms  FILE [OBJ]        all symbols of one object, demangled
        coff.py types FILE [OBJ]        struct/class/enum definitions from -g debug symbols
        coff.py demangle NAME...        demangle gcc 2.x names

As a module: Coff(bytes) gives .sections, .syms (list of Sym), .files (list of Obj),
.lines (line-number entries), .functions(obj) and .read(vaddr, n).

Facts about AK.COF that the reader relies on:
- each object starts with its .file symbol; its .text/.data/.bss section symbols carry the
  object's address and length in their aux entry;
- GNU ld moves global (EXT) symbols away from their object, so they are given to objects by
  address (_place_globals);
- objects compiled without -g have no function type on their function symbols.
"""
import argparse
import collections
import struct
import sys

SCLASS = {0: 'NULL', 1: 'AUTO', 2: 'EXT', 3: 'STAT', 4: 'REG', 5: 'EXTDEF', 6: 'LABEL',
          7: 'ULABEL', 8: 'MOS', 9: 'ARG', 10: 'STRTAG', 11: 'MOU', 12: 'UNTAG', 13: 'TPDEF',
          14: 'USTATIC', 15: 'ENTAG', 16: 'MOE', 17: 'REGPARM', 18: 'FIELD', 100: 'BLOCK',
          101: 'FCN', 102: 'EOS', 103: 'FILE', 104: 'LINE', 105: 'ALIAS', 106: 'HIDDEN'}
MARKERS = ('gcc2_compiled.', '___gnu_compiled_cplusplus', '___gnu_compiled_c')


class Sym:
    __slots__ = ('index', 'name', 'value', 'sec', 'type', 'sclass', 'aux', 'obj')

    def __repr__(self):
        return '<%d %s %s %x sec%d>' % (self.index, SCLASS.get(self.sclass, self.sclass),
                                         self.name, self.value, self.sec)


class Obj:
    """One input object of the link, from its .file symbol to the next."""

    def __init__(self, name, first):
        self.name = name
        self.first = first
        self.syms = []
        self.text = self.data = self.bss = None   # (start, length) from the section symbols

    def __repr__(self):
        return '<Obj %s>' % self.name


class Coff:
    def __init__(self, data):
        self.data = d = data
        (self.magic, nsec, self.timestamp, self.symptr, self.nsyms, optsz,
         self.flags) = struct.unpack_from('<HHIIIHH', d, 0)
        if self.magic != 0x14C:
            raise ValueError('not an i386 COFF file')
        self.aout = None
        if optsz == 28:
            f = struct.unpack_from('<HHIIIIII', d, 20)
            self.aout = dict(magic=f[0], vstamp=f[1], tsize=f[2], dsize=f[3], bsize=f[4],
                             entry=f[5], text_start=f[6], data_start=f[7])
        self.sections = []
        for i in range(nsec):
            f = struct.unpack_from('<8sIIIIIIHHI', d, 20 + optsz + 40 * i)
            self.sections.append(dict(name=f[0].split(b'\0')[0].decode(), paddr=f[1], vaddr=f[2],
                                      size=f[3], scnptr=f[4], relptr=f[5], lnnoptr=f[6],
                                      nreloc=f[7], nlnno=f[8], flags=f[9]))
        self.strtab = self.symptr + 18 * self.nsyms
        self._read_syms()
        self._place_globals()
        self._read_lines()

    def _name(self, raw):
        if raw[:4] == b'\0\0\0\0':
            o = self.strtab + struct.unpack_from('<I', raw, 4)[0]
            return self.data[o:self.data.index(b'\0', o)].decode('latin-1')
        return raw.split(b'\0')[0].decode('latin-1')

    def _read_syms(self):
        d = self.data
        self.syms = []
        self.byindex = {}
        self.files = []
        obj = None
        i = 0
        while i < self.nsyms:
            p = self.symptr + 18 * i
            s = Sym()
            s.index = i
            s.name = self._name(d[p:p + 8])
            s.value, s.sec, s.type, s.sclass, naux = struct.unpack_from('<IhHBB', d, p + 8)
            s.aux = [d[p + 18 * (k + 1):p + 18 * (k + 2)] for k in range(naux)]
            if s.sclass == 103:
                obj = Obj(b''.join(s.aux).split(b'\0')[0].decode('latin-1'), i)
                self.files.append(obj)
            s.obj = obj
            if obj is not None:
                obj.syms.append(s)
                if s.sclass == 3 and s.name in ('.text', '.data', '.bss') and s.aux:
                    length = struct.unpack_from('<I', s.aux[0], 0)[0]
                    setattr(obj, s.name[1:], (s.value, length))
            self.syms.append(s)
            self.byindex[i] = s
            i += 1 + naux

    def _place_globals(self):
        """Give each global (EXT) symbol to the object whose section range holds it."""
        ranges = {1: [], 2: [], 3: []}
        for o in self.files:
            for k, attr in ((1, 'text'), (2, 'data'), (3, 'bss')):
                r = getattr(o, attr)
                if r and r[1]:
                    ranges[k].append((r[0], r[0] + r[1], o))
        for s in self.syms:
            if s.sclass != 2 or s.sec not in ranges:
                continue
            for lo, hi, o in ranges[s.sec]:
                if lo <= s.value < hi:
                    if s.obj is not o:
                        if s.obj is not None:
                            s.obj.syms.remove(s)
                        s.obj = o
                        o.syms.append(s)
                    break
        for o in self.files:
            o.syms.sort(key=lambda s: s.index)

    def _read_lines(self):
        self.lines = []
        for sec in self.sections:
            for k in range(sec['nlnno']):
                addr, ln = struct.unpack_from('<IH', self.data, sec['lnnoptr'] + 6 * k)
                self.lines.append((sec['name'], addr, ln))

    def section(self, name):
        for sec in self.sections:
            if sec['name'] == name:
                return sec
        return None

    def raw(self, name):
        """Raw bytes of a section (empty for .bss or a missing section)."""
        sec = self.section(name)
        if sec is None or not sec['scnptr']:
            return b''
        return self.data[sec['scnptr']:sec['scnptr'] + sec['size']]

    def relocs(self, name):
        """Relocations of a section in an object: (offset in the section, symbol index, type).
        Types: 6 = RELLONG (absolute 32-bit), 20 = PCRLONG (pc-relative 32-bit)."""
        sec = self.section(name)
        out = []
        if sec:
            for k in range(sec['nreloc']):
                va, sym, typ = struct.unpack_from('<IIH', self.data, sec['relptr'] + 10 * k)
                out.append((va - sec['vaddr'], sym, typ))
        return out

    def obj(self, name):
        for o in self.files:
            if o.name == name:
                return o
        raise KeyError(name)

    def section_of(self, vaddr):
        for sec in self.sections:
            if sec['vaddr'] <= vaddr < sec['vaddr'] + sec['size']:
                return sec
        return None

    def read(self, vaddr, n):
        """Bytes at a virtual address (zeros for .bss)."""
        sec = self.section_of(vaddr)
        if sec is None:
            raise ValueError('address %x outside the sections' % vaddr)
        if not sec['scnptr']:
            return bytes(n)
        o = sec['scnptr'] + vaddr - sec['vaddr']
        return self.data[o:o + n]

    def functions(self, obj=None):
        """Function symbols as (sym, size): every EXT/STAT symbol in .text that is not a
        section name or compiler marker; size runs to the next one or the end of the object's
        .text (so it includes the string literals gcc places after a function)."""
        out = []
        for o in ([obj] if obj else self.files):
            fs = sorted((s for s in o.syms if s.sclass in (2, 3) and s.sec == 1
                         and not s.name.startswith('.') and s.name not in MARKERS),
                        key=lambda s: s.value)
            end = o.text[0] + o.text[1] if o.text else None
            for k, s in enumerate(fs):
                nxt = fs[k + 1].value if k + 1 < len(fs) else end
                out.append((s, (nxt - s.value) if nxt is not None else None))
        return out

    # ------------------------------------------------------------ -g debug types

    BASIC = {0: '?', 1: 'arg', 2: 'char', 3: 'short', 4: 'int', 5: 'long', 6: 'float',
             7: 'double', 8: 'struct', 9: 'union', 10: 'enum', 11: 'moe', 12: 'unsigned char',
             13: 'unsigned short', 14: 'unsigned int', 15: 'unsigned long', 16: 'long double'}

    def typestr(self, s):
        """C spelling of a debug symbol's COFF type (base type plus derived ptr/fcn/array)."""
        base = s.type & 0xF
        if base in (8, 9, 10) and s.aux:
            tag = struct.unpack_from('<I', s.aux[0], 0)[0]
            t = self.byindex.get(tag)
            name = t.name.lstrip('_') if t else '?'
            b = '%s %s' % (self.BASIC[base], name)
        elif base == 0 and s.type == 0:
            b = 'void'
        else:
            b = self.BASIC.get(base, '?')
        derived = []
        t = s.type >> 4
        while t & 3:
            derived.append(t & 3)
            t >>= 2
        dims = []
        if s.aux and 3 in derived:
            dims = [x for x in struct.unpack_from('<4H', s.aux[0], 8) if x]
        suffix = ''
        for kind in reversed(derived):
            if kind == 1:
                b += ' *'
            elif kind == 2:
                b += ' ()'
            elif kind == 3:
                suffix += '[%d]' % dims.pop(0) if dims else '[]'     # dims run outermost first
        return b + suffix

    def types(self, obj):
        """Struct, union and enum definitions as (kind, tag, size, [(member, offset, type)])."""
        out = []
        syms = obj.syms
        k = 0
        while k < len(syms):
            s = syms[k]
            if s.sclass in (10, 12, 15):
                size = struct.unpack_from('<H', s.aux[0], 6)[0] if s.aux else 0
                members = []
                k += 1
                while k < len(syms) and syms[k].sclass != 102:
                    m = syms[k]
                    members.append((m.name.lstrip('_'), m.value, self.typestr(m)))
                    k += 1
                out.append((SCLASS[s.sclass], s.name.lstrip('_'), size, members))
            k += 1
        return out


def ar_members(data):
    """Members of a Unix ar archive (DJGPP's libc.a ...) as (name, bytes), in archive order."""
    if data[:8] != b'!<arch>\n':
        raise ValueError('not an ar archive')
    p = 8
    names = b''
    while p + 60 <= len(data):
        h = data[p:p + 60]
        name = h[:16].decode('latin-1').strip()
        size = int(h[48:58])
        body = data[p + 60:p + 60 + size]
        if name == '//':
            names = body
        elif name not in ('/', '__.SYMDEF', '__.SYMDEF/', 'ARFILENAMES/'):
            if name.startswith('/') and name[1:].isdigit():
                o = int(name[1:])
                e = min(i for i in (names.find(b'/', o), names.find(b'\n', o), len(names)) if i >= 0)
                name = names[o:e].decode('latin-1')
            yield name.rstrip('/'), body
        p += 60 + size + (size & 1)


def masked(raw, relocs):
    """Bytes with every relocated 32-bit field set to zero, plus the set of masked offsets."""
    b = bytearray(raw)
    mask = set()
    for off, _sym, _typ in relocs:
        for i in range(off, off + 4):
            if 0 <= i < len(b):
                b[i] = 0
                mask.add(i)
    return bytes(b), mask


# ---------------------------------------------------------------- gcc 2.x demangler

OPS = {'nw': 'new', 'dl': 'delete', 'vn': 'new[]', 'vd': 'delete[]', 'as': '=', 'ne': '!=',
       'eq': '==', 'ge': '>=', 'gt': '>', 'le': '<=', 'lt': '<', 'pl': '+', 'apl': '+=',
       'mi': '-', 'ami': '-=', 'ml': '*', 'aml': '*=', 'dv': '/', 'adv': '/=', 'md': '%',
       'amd': '%=', 'er': '^', 'aer': '^=', 'ad': '&', 'aad': '&=', 'or': '|', 'aor': '|=',
       'co': '~', 'nt': '!', 'ls': '<<', 'als': '<<=', 'rs': '>>', 'ars': '>>=', 'oo': '||',
       'aa': '&&', 'pp': '++', 'mm': '--', 'cl': '()', 'vc': '[]', 'rf': '->', 'cm': ',',
       'rm': '->*'}
BUILTIN = {'v': 'void', 'c': 'char', 's': 'short', 'i': 'int', 'l': 'long', 'x': 'long long',
           'f': 'float', 'd': 'double', 'r': 'long double', 'b': 'bool', 'w': 'wchar_t',
           'e': '...'}


class _Dm:
    def __init__(self, s):
        self.s = s
        self.p = 0
        self.args = []

    def peek(self):
        return self.s[self.p] if self.p < len(self.s) else ''

    def num(self):
        q = self.p
        while self.peek().isdigit():
            self.p += 1
        if q == self.p:
            raise ValueError
        return int(self.s[q:self.p])

    def name(self):
        n = self.num()
        r = self.s[self.p:self.p + n]
        if len(r) != n:
            raise ValueError
        self.p += n
        return r

    def qualname(self):
        if self.peek() == 'Q':
            self.p += 1
            if self.peek() == '_':
                self.p += 1
                k = self.num()
                self.p += 1
            else:
                k = int(self.s[self.p])
                self.p += 1
            return '::'.join(self.name() for _ in range(k))
        if self.peek() == 't':
            self.p += 1
            base = self.name()
            k = self.num()
            parms = []
            for _ in range(k):
                if self.peek() == 'Z':
                    self.p += 1
                    parms.append(self.type())
                else:
                    t = self.type()
                    q = self.p
                    while self.peek().isdigit() or self.peek() == 'm':
                        self.p += 1
                    parms.append(t + '=' + self.s[q:self.p].replace('m', '-'))
            return '%s<%s>' % (base, ', '.join(parms))
        return self.name()

    def type(self):
        pre = []
        while self.peek() and self.peek() in 'CVUS':
            pre.append({'C': 'const', 'V': 'volatile', 'U': 'unsigned', 'S': 'signed'}[self.peek()])
            self.p += 1
        c = self.peek()
        self.p += 1
        if c in BUILTIN:
            base = BUILTIN[c]
        elif c == 'P':
            base = self.type() + ' *'
        elif c == 'R':
            base = self.type() + ' &'
        elif c == 'A':
            n = self.num()
            self.p += 1           # '_'
            base = '%s[%d]' % (self.type(), n)
        elif c == 'F':
            a = self.arglist(end='_')
            self.p += 1
            base = '%s (*)(%s)' % (self.type(), a)
        elif c == 'M':
            cls = self.qualname()
            base = '%s %s::*' % (self.type(), cls)
        elif c == 'T':
            k = self.num()
            base = self.args[k] if k < len(self.args) else '?'
        elif c and (c.isdigit() or c in 'Qt'):
            self.p -= 1
            base = self.qualname()
        else:
            raise ValueError
        return ' '.join(pre + [base]) if pre else base

    def arglist(self, end=''):
        out = []
        while self.p < len(self.s) and self.peek() != end:
            if self.peek() == 'N':
                self.p += 1
                cnt = int(self.s[self.p])
                self.p += 1
                k = self.num() if self.peek().isdigit() else 0
                if self.peek() == '_':
                    self.p += 1
                for _ in range(cnt):
                    out.append(self.args[k])
                    self.args.append(self.args[k])
                continue
            t = self.type()
            out.append(t)
            self.args.append(t)
        return '' if out == ['void'] else ', '.join(out)


def _function(fn, rest):
    """fn = the part before '__' ('' for a constructor), rest = the encoding after it."""
    d = _Dm(rest)
    const = ''
    if d.peek() == 'F':
        d.p += 1
        cls = None
    else:
        if d.peek() == 'C':
            const = ' const'
            d.p += 1
        cls = d.qualname()
    args = d.arglist()
    if d.p != len(rest):
        raise ValueError
    if fn == '':
        if cls is None:
            raise ValueError
        fn = cls.split('::')[-1].split('<')[0]
    elif fn in OPS:
        fn = 'operator' + OPS[fn]
    elif fn.startswith('op') and len(fn) > 2:
        fn = 'operator ' + _Dm(fn[2:]).type()
    return '%s%s(%s)%s' % (cls + '::' if cls else '', fn, args, const)


def demangle(sym):
    """Demangle a gcc 2.x C++ name (with or without DJGPP's leading underscore).
    Returns the name unchanged when it is not a C++ name."""
    s = sym[1:] if sym.startswith('_') else sym
    try:
        for marker in ('$', '.'):
            if s.startswith('_vt' + marker) or s.startswith('vt' + marker):
                return 'vtable for ' + _Dm(s.split(marker, 1)[1]).qualname()
            if s.startswith('_' + marker + '_'):
                cls = _Dm(s[3:]).qualname()
                return '%s::~%s()' % (cls, cls.split('::')[-1].split('<')[0])
        for kind, what in (('I', 'constructors'), ('D', 'destructors')):
            for marker in ('$', '.'):
                pre = '_GLOBAL_%s%s%s' % (marker, kind, marker)
                if s.startswith(pre):
                    return 'global %s keyed to %s' % (what, s[len(pre):])
        if s.startswith('__'):
            # constructor (__5akaux...) or operator (__ls__7ostreamPCc)
            if s[2:3].isdigit() or s[2:3] in ('Q', 't'):
                return _function('', s[2:])
            i = s.find('__', 2)
            if i > 2:
                return _function(s[2:i], s[i + 2:])
            raise ValueError
        if s[:1].isdigit() or s[:1] == 'Q':
            d = _Dm(s)
            cls = d.qualname()
            if d.peek() in ('$', '.'):
                return '%s::%s' % (cls, s[d.p + 1:])
            raise ValueError
        i = s.find('__', 1)
        while i > 0 and s[i + 2:i + 3] == '_':
            i += 1
        if i < 0:
            return sym
        return _function(s[:i], s[i + 2:])
    except (ValueError, IndexError, KeyError):
        return sym


dm = demangle


# ---------------------------------------------------------------- command line

def cmd_info(c):
    print('magic %#x, %d sections, timestamp %d, %d symbols at %#x, flags %#x' %
          (c.magic, len(c.sections), c.timestamp, c.nsyms, c.symptr, c.flags))
    if c.aout:
        print('a.out header:', ', '.join('%s=%#x' % kv for kv in c.aout.items()))
    for s in c.sections:
        print('  %-6s vaddr %06X size %06X file %06X  lnno %d at %06X  reloc %d  flags %#x'
              % (s['name'], s['vaddr'], s['size'], s['scnptr'], s['nlnno'], s['lnnoptr'],
                 s['nreloc'], s['flags']))
    cnt = collections.Counter(SCLASS.get(s.sclass, s.sclass) for s in c.syms)
    print('symbol classes:', ', '.join('%s %d' % kv for kv in cnt.most_common()))
    dbg = collections.Counter()
    for s in c.syms:
        if s.sclass in (1, 4, 8, 9, 10, 13, 15, 16, 100, 101) and s.obj:
            dbg[s.obj.name] += 1
    print('objects with debug symbols (-g):', ', '.join('%s %d' % kv for kv in dbg.most_common()))
    print('line numbers: %d entries' % len(c.lines))


def cmd_files(c, md):
    if md:
        print('| # | object | .text | size | .data | size | .bss | size |')
        print('|---|---|---|---|---|---|---|---|')
    for k, o in enumerate(c.files):
        def f(r):
            return ('%06X' % r[0], '%d' % r[1]) if r else ('', '')
        row = (k, o.name) + f(o.text) + f(o.data) + f(o.bss)
        if md:
            print('| %d | %s | %s | %s | %s | %s | %s | %s |' % row)
        else:
            print('%3d %-16s text %6s %6s  data %6s %6s  bss %6s %6s' % row)


def cmd_funcs(c, objname):
    for o in c.files:
        if objname not in (None, o.name):
            continue
        fs = c.functions(o)
        if not fs:
            continue
        print('== %s (%d)' % (o.name, len(fs)))
        for s, size in fs:
            print('  %06X %6s %-4s %s' % (s.value, size, SCLASS[s.sclass], dm(s.name)))


def cmd_syms(c, objname):
    for o in c.files:
        if objname not in (None, o.name):
            continue
        print('== %s  text %s data %s bss %s' % (o.name, o.text, o.data, o.bss))
        for s in o.syms:
            if s.sclass == 103:
                continue
            d = dm(s.name)
            print('  %5d %-6s %06X sec%-2d type %04X  %s' %
                  (s.index, SCLASS.get(s.sclass, s.sclass), s.value, s.sec, s.type,
                   s.name + ('   = ' + d if d != s.name else '')))


def cmd_types(c, objname):
    for o in c.files:
        if objname not in (None, o.name):
            continue
        for kind, tag, size, members in c.types(o):
            print('%s %s  /* size %d */' % (kind.lower(), tag, size))
            for name, val, t in members:
                print('    %-28s %5d  %s' % (name, val, t))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['info', 'files', 'funcs', 'syms', 'types', 'demangle'])
    ap.add_argument('args', nargs='*')
    ap.add_argument('--md', action='store_true')
    a = ap.parse_args()
    if a.cmd == 'demangle':
        for n in a.args:
            print(n, '=', dm(n))
        return
    c = Coff(open(a.args[0], 'rb').read())
    obj = a.args[1] if len(a.args) > 1 else None
    {'info': lambda: cmd_info(c), 'files': lambda: cmd_files(c, a.md),
     'funcs': lambda: cmd_funcs(c, obj), 'syms': lambda: cmd_syms(c, obj),
     'types': lambda: cmd_types(c, obj)}[a.cmd]()


if __name__ == '__main__':
    sys.exit(main())
