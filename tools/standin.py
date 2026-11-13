#!/usr/bin/env python3
"""Generate assembly stand-ins for the game objects of AK.COF (phase 3).

    python tools/standin.py [OBJ ...] [-o src/standin]      (default: the five ak*.cc)

A stand-in is a gas source that assembles to the object's original .text, byte for byte, with
every address in it written as a relocation, so the linker resolves it (and pulls the same
library members) as for the original object. The C++ that replaces a stand-in must give the
same bytes.

How it works:
- Code is found by following control flow from the object's function symbols (capstone).
  Bytes never reached are data: string literals, doubles, alignment fillers, switch tables.
- A scaled load `[reg*4 + table]` from the object's own .text marks a switch table (gcc 2.7.2.1
  at -O0 loads the entry, stores it and jumps through the stack slot); the preceding `cmp`
  bounds it. Its entries (code addresses in the object) are
  more entry points and are written `.long label`.
- In decoded instructions, a 32-bit immediate or displacement is an address when it points
  into the program and (for .text) exactly at a symbol, or (for .data/.bss) anywhere: it is
  written `.long symbol+offset`. A call or jump whose target lies outside the object is written
  `call _symbol` / `jmp _symbol`. Everything else stays `.byte`.
- Function and literal labels get the original names (global functions `.globl`).
The generated source is checked by assembling it (tools/gxx.py style compare) in phase 3's link.
"""
import argparse
import bisect
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import coff  # noqa: E402

import capstone  # noqa: E402

GAME = ['ak.cc', 'akaux.cc', 'akchar.cc', 'akmenu.cc', 'akworld.cc']


class Gen:
    def __init__(self, ak, obj):
        self.ak = ak
        self.obj = obj
        self.a, n = obj.text
        self.b = self.a + n
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.lo = ak.sections[0]['vaddr']
        self.hi = ak.sections[2]['vaddr'] + ak.sections[2]['size']
        self.textend = ak.sections[0]['vaddr'] + ak.sections[0]['size']
        # symbols: own labels by address; globals of the whole program by address
        self.own = {}
        for s in obj.syms:
            if s.sec == 1 and s.sclass in (2, 3, 6) and not s.name.startswith('.') \
                    and s.name not in coff.MARKERS and self.a <= s.value < self.b:
                self.own.setdefault(s.value, s)
        self.glob = sorted((s.value, s.sec, s.name) for s in ak.syms if s.sclass == 2 and s.sec in (1, 2, 3))
        self.globvals = [g[0] for g in self.glob]
        self.textsyms = {}
        for s in ak.syms:
            if s.sec == 1 and s.sclass in (2, 3, 6):
                self.textsyms.setdefault(s.value, s)
        self.code = {}       # address -> instruction
        self.relocs = {}     # address of field -> (kind, target) kind: 'abs' or 'pcrel'
        self.tables = set()  # addresses of table words

    def ext_name(self, v):
        """A global symbol for address v (exact for .text), as (name, addend)."""
        sec = self.ak.section_of(v)
        k = bisect.bisect_right(self.globvals, v) - 1
        while k >= 0 and self.glob[k][1] != self.ak.sections.index(sec) + 1:
            k -= 1
        if k < 0:
            return None
        val, _sec, name = self.glob[k]
        if sec['name'] == '.text' and val != v:
            return None
        return name, v - val

    def is_addr(self, v):
        if not (self.lo <= v < self.hi):
            return False
        if v < self.textend:
            return v in self.textsyms or (self.a <= v < self.b)
        return True

    def explore(self):
        todo = [s.value for s in self.own.values() if s.sclass in (2, 3)]
        while todo:
            pc = todo.pop()
            lastcmp = None
            while self.a <= pc < self.b and pc not in self.code:
                insn = next(self.md.disasm(self.ak.read(pc, 16), pc), None)
                if insn is None:
                    raise SystemExit('%s: cannot decode at %X' % (self.obj.name, pc))
                self.code[pc] = insn
                g = set(insn.groups)
                ops = insn.operands
                # relocation fields
                if insn.imm_size == 4 and capstone.CS_GRP_JUMP not in g and capstone.CS_GRP_CALL not in g:
                    v = ops[-1].imm & 0xFFFFFFFF if ops and ops[-1].type == capstone.x86.X86_OP_IMM else None
                    for op in ops:
                        if op.type == capstone.x86.X86_OP_IMM:
                            v = op.imm & 0xFFFFFFFF
                    if v is not None and self.is_addr(v):
                        self.relocs[pc + insn.imm_offset] = ('abs', v)
                if insn.disp_size == 4:
                    for op in ops:
                        if op.type == capstone.x86.X86_OP_MEM:
                            v = op.mem.disp & 0xFFFFFFFF
                            if self.is_addr(v):
                                self.relocs[pc + insn.disp_offset] = ('abs', v)
                            # switch table. gcc 2.7.2.1 at -O0 writes
                            #   cmp [ebp-N], max / ja default / mov r,[ebp-N]
                            #   mov r,[r*4 + table] / mov [ebp-N],r / jmp [ebp-N]
                            # so any scaled load from our own .text is a table of max+1 entries
                            if op.mem.scale == 4 and self.a <= v < self.b:
                                if lastcmp is None:
                                    raise SystemExit('%s: table at %X without a bound' % (self.obj.name, v))
                                for k in range(lastcmp + 1):
                                    t = v + 4 * k
                                    e = int.from_bytes(self.ak.read(t, 4), 'little')
                                    if not (self.a <= e < self.b):
                                        raise SystemExit('%s: table entry %X -> %X' % (self.obj.name, t, e))
                                    self.tables.add(t)
                                    self.relocs[t] = ('abs', e)
                                    todo.append(e)
                if insn.mnemonic == 'cmp' and ops and ops[-1].type == capstone.x86.X86_OP_IMM:
                    lastcmp = ops[-1].imm
                # control flow
                if capstone.CS_GRP_JUMP in g or capstone.CS_GRP_CALL in g:
                    if ops and ops[0].type == capstone.x86.X86_OP_IMM:
                        tgt = ops[0].imm & 0xFFFFFFFF
                        if self.a <= tgt < self.b:
                            if capstone.CS_GRP_CALL in g or True:
                                todo.append(tgt)
                        else:
                            self.relocs[pc + insn.imm_offset] = ('pcrel', tgt)
                    if insn.mnemonic == 'jmp':
                        break
                if capstone.CS_GRP_RET in g or insn.mnemonic in ('ret', 'iret', 'hlt'):
                    break
                pc += insn.size
        self.pointer_tables()

    def pointer_tables(self):
        """Initialisers of local pointer arrays (akaux.cc's quotes) sit in .text as words that
        point at the object's own literals (LC labels). A run of at least two such aligned words
        outside the code is taken for one, and each word becomes a relocation."""
        covered = set()
        for a, i in self.code.items():
            covered.update(range(a, a + i.size))
        lits = {v for v, s in self.own.items() if s.name.startswith('LC')}
        t = self.a
        while t + 4 <= self.b:
            run = []
            u = t
            while u + 4 <= self.b and not covered.intersection(range(u, u + 4)) and u not in self.tables:
                e = int.from_bytes(self.ak.read(u, 4), 'little')
                if e not in lits and not (e == 0 and run):
                    break
                run.append((u, e))
                u += 4
            if sum(1 for _, e in run if e) >= 2:
                for u, e in run:
                    if e:
                        self.relocs[u] = ('abs', e)
                    self.tables.add(u)
                t = u
            else:
                t += 4 if run else 1

    def label(self, v):
        s = self.own.get(v)
        return s.name if s else 'Q%06X' % v

    def emit(self):
        out = ['/* Stand-in for %s: generated by tools/standin.py from AK.COF %06X-%06X.' % (self.obj.name, self.a, self.b),
               '   Original bytes, with addresses as relocations. Replaced by C++ in phase 4. */',
               '\t.file\t"%s"' % self.obj.name, '.text']
        need = {v for k, (kind, v) in self.relocs.items() if kind == 'abs' and self.a <= v < self.b}
        labels = set(self.own) | need
        for s in self.own.values():
            if s.sclass == 2:
                out.append('\t.globl\t%s' % s.name)
        pc = self.a
        pending = []

        def flush():
            for i in range(0, len(pending), 16):
                out.append('\t.byte\t' + ','.join('0x%02x' % x for x in pending[i:i + 16]))
            pending.clear()
        while pc < self.b:
            if pc in labels:
                flush()
                out.append('%s:' % self.label(pc))
            if pc in self.relocs:
                kind, v = self.relocs[pc]
                if kind == 'abs':
                    flush()
                    if self.a <= v < self.b:
                        out.append('\t.long\t%s' % self.label(v))
                    else:
                        e = self.ext_name(v)
                        if e is None:
                            raise SystemExit('%s: no symbol for address %X (field at %X)' % (self.obj.name, v, pc))
                        out.append('\t.long\t%s%s' % (e[0], '+%d' % e[1] if e[1] else ''))
                    pc += 4
                    continue
                else:
                    # pc-relative field of a call/jump to another object: rewrite the whole insn
                    op = pending.pop() if pending else None
                    flush()
                    e = self.ext_name(v)
                    if e is None or e[1]:
                        raise SystemExit('%s: call target %X is not a global symbol' % (self.obj.name, v))
                    mnem = {0xE8: 'call', 0xE9: 'jmp'}.get(op)
                    if mnem is None:
                        raise SystemExit('%s: pc-relative field at %X after opcode %r' % (self.obj.name, pc, op))
                    out.append('\t%s\t%s' % (mnem, e[0]))
                    pc += 4
                    continue
            pending.append(self.ak.read(pc, 1)[0])
            pc += 1
        flush()
        return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('objs', nargs='*', default=GAME)
    ap.add_argument('-o', '--outdir', default=os.path.join(ROOT, 'src', 'standin'))
    a = ap.parse_args()
    ak = coff.Coff(open(os.path.join(ROOT, 'build', 'AK.COF'), 'rb').read())
    os.makedirs(a.outdir, exist_ok=True)
    for name in a.objs:
        g = Gen(ak, ak.obj(name))
        g.explore()
        src = g.emit()
        path = os.path.join(a.outdir, os.path.splitext(name)[0] + '.s')
        open(path, 'w', newline='\n').write(src)
        code = sum(i.size for i in g.code.values())
        nabs = sum(1 for k, v in g.relocs.items() if v[0] == 'abs')
        npc = sum(1 for k, v in g.relocs.items() if v[0] == 'pcrel')
        print('%-11s %6d bytes: %6d code, %d tables words, %d absolute and %d pc-relative relocations -> %s'
              % (name, g.b - g.a, code, len(g.tables), nabs, npc, os.path.relpath(path, ROOT)))


if __name__ == '__main__':
    sys.exit(main())
