#!/usr/bin/env python3
"""Pseudo-C for a game object of AK.COF: gcc 2.7.2.1 -O0 code folded back into expressions.

    python tools/pseudo.py OBJ [FUNC]          e.g. pseudo.py akworld.cc drawworld

A reading aid, not a decompiler: it follows registers, the frame's spill slots and the x87
stack through each basic block and prints one line per store, call or branch, with gotos.
Member accesses through `this` (akworld) or a savestruct reference are named from the class
layouts (docs/MAP.md), with array indexes recovered from the address arithmetic. Frame slots
that are only ever written from a register and read back in the same block are taken for
gcc's spill temporaries and folded away; the others are shown as variables vN (frame offset
-N, hex), arguments as this/p1/p2. Instructions it does not model are printed as /* asm */.
Needs capstone (../u1-decompiled/build/venv).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import coff     # noqa: E402
import standin  # noqa: E402
from capstone import x86 as X  # noqa: E402

SAV = [('character', 0, (6,)), ('owned', 24, (6,)), ('name', 48, (24,), 1), ('ChClass', 72, (), 1),
       ('ln', 76, ()), ('level', 80, ()), ('tx', 84, ()), ('ty', 88, ()), ('px', 92, ()),
       ('py', 96, ()), ('dx', 100, ()), ('dy', 104, ()), ('inout', 108, ()), ('task', 112, ()),
       ('visit', 116, ()), ('lk', 120, ())]
WORLD = [(('sav.' + m[0]),) + m[1:] for m in SAV] + [
    ('count', 124, ()), ('world', 128, (21, 21)), ('dng', 1892, (11, 11)), ('mdraw', 2376, ()),
    ('color', 2380, ()), ('run1', 2384, ()), ('worldbmp', 2388, ()), ('backmid', 2392, ()),
    ('per', 2396, (11, 4)), ('cd', 2572, (11, 4)), ('lad', 2748, (11, 4)), ('ld', 2924, (11, 6)),
    ('ft', 3188, (11, 6)), ('xx', 3452, (11,)), ('yy', 3496, (11,)), ('mz', 3540, (11, 2)),
    ('ml', 3628, (11, 2))]
LAYOUTS = {'this': WORLD, 'p2': SAV, 'p1': SAV}

DOUBLE_FUNCS = {'atan', 'floor', 'sqrt', 'sin', 'cos', 'tan', 'pow', 'fabs', 'exp', 'log', 'atan2',
                'ceil', 'atof'}
R32 = {'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi'}
SUB = {'al': 'eax', 'ah': 'eax', 'ax': 'eax', 'bl': 'ebx', 'bh': 'ebx', 'cl': 'ecx', 'ch': 'ecx',
       'dl': 'edx', 'dh': 'edx', 'si': 'esi', 'di': 'edi', 'bx': 'ebx', 'cx': 'ecx', 'dx': 'edx'}


class U(str):
    """A string atom that is equal only to itself: two calls to random() are two values."""
    __eq__ = object.__eq__
    __ne__ = object.__ne__
    __hash__ = object.__hash__


class Lin:
    """A linear expression: {atom: coefficient} + constant. Atoms are strings."""

    def __init__(self, terms=None, const=0):
        self.t = {k: v for k, v in (terms or {}).items() if v}
        self.c = const

    @staticmethod
    def of(x):
        if isinstance(x, Lin):
            return x
        if isinstance(x, int):
            return Lin({}, x)
        return Lin({x: 1})

    def __add__(self, o):
        o = Lin.of(o)
        t = dict(self.t)
        for k, v in o.t.items():
            t[k] = t.get(k, 0) + v
        return Lin(t, self.c + o.c)

    def scale(self, k):
        return Lin({a: v * k for a, v in self.t.items()}, self.c * k)

    def __sub__(self, o):
        return self + Lin.of(o).scale(-1)

    def const(self):
        return None if self.t else self.c

    def __str__(self):
        parts = []
        for a, v in self.t.items():
            a = a if is_simple(a) else '(%s)' % a
            parts.append(a if v == 1 else '-' + a if v == -1 else '%d*%s' % (v, a))
        s = ' + '.join(parts).replace('+ -', '- ')
        if self.c or not parts:
            if parts:
                s += (' + %d' % self.c if self.c > 0 else ' - %d' % -self.c)
            else:
                s = str(self.c)
        return s


def is_simple(a):
    return all(ch.isalnum() or ch in '_.[]>' for ch in a)


def S(x):
    return str(x) if isinstance(x, Lin) else str(x)


class Pseudo:
    def __init__(self, ak, objname):
        self.ak = ak
        self.g = standin.Gen(ak, ak.obj(objname))
        self.g.explore()
        g = self.g
        self.labels = set()
        for pc, i in g.code.items():
            if i.mnemonic.startswith('j') or i.mnemonic == 'loop':
                try:
                    self.labels.add(int(i.op_str, 16))
                except ValueError:
                    pass
        for v, s in g.own.items():
            if s.sclass == 6:
                self.labels.add(v)
        self.doubles = set()
        for pc0, i in g.code.items():
            if 'qword ptr' in i.op_str and i.disp_offset and (pc0 + i.disp_offset) in g.relocs:
                self.doubles.add(g.relocs[pc0 + i.disp_offset][1])

    # ------------------------------------------------------------------ naming
    def name(self, v):
        g = self.g
        if g.a <= v < g.b and v in g.own:
            return g.own[v].name
        e = g.ext_name(v) if g.lo <= v < g.hi else None
        if e:
            return coff.dm(e[0]).split('(')[0] + ('+%d' % e[1] if e[1] else '')
        return None

    def literal(self, v):
        if v in self.doubles:
            return repr(struct.unpack('<d', self.ak.read(v, 8))[0])
        s = self.ak.read(v, 120).split(b'\0')[0]
        return '"' + s.decode('latin-1').replace('\n', '\\n') + '"'

    # ------------------------------------------------------------------ slots
    def classify(self, start, end):
        """Frame slots: 'temp' (spill), 'cw' (FPU control word) or 'var'."""
        kind = {}
        written = set()
        pc = start
        while pc < end:
            i = self.g.code.get(pc)
            if i is None:
                pc += 1
                continue
            if pc in self.labels:
                written = set()
            ops = i.operands
            for k, op in enumerate(ops):
                if op.type == X.X86_OP_MEM and op.mem.base == X.X86_REG_EBP and op.mem.index == 0 \
                        and op.mem.disp < 0:
                    n = -op.mem.disp
                    if i.mnemonic in ('fnstcw', 'fldcw'):
                        kind[n] = 'cw'
                        continue
                    if kind.get(n) == 'cw':
                        continue
                    is_dst = k == 0 and i.mnemonic in ('mov', 'fstp', 'fistp', 'fst')
                    if is_dst:
                        src_reg = len(ops) > 1 and ops[1].type == X.X86_OP_REG
                        if i.mnemonic == 'mov' and not src_reg:
                            kind[n] = 'var'
                        written.add(n)
                    else:
                        if k == 0 and i.mnemonic not in ('push', 'cmp', 'test', 'fld', 'fild', 'jmp', 'call') \
                                and not (i.mnemonic in ('add', 'sub', 'shl', 'and', 'or', 'mov')
                                         and n in written):
                            kind[n] = 'var'          # updated in place
                        if n not in written:
                            kind[n] = 'var'
                        kind.setdefault(n, 'temp')
                    kind.setdefault(n, 'temp')
            if i.mnemonic in ('jmp', 'ret'):
                written = set()
            pc += i.size
        # a slot stored from registers again and again is a spill slot even when gcc reads it
        # across one of its own internal labels (a switch tree, a sign fix for %)
        stores = {}
        pc = start
        while pc < end:
            i = self.g.code.get(pc)
            if i is None:
                pc += 1
                continue
            ops = i.operands
            if i.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == X.X86_OP_MEM                     and ops[0].mem.base == X.X86_REG_EBP and ops[0].mem.index == 0                     and ops[0].mem.disp < 0 and ops[1].type == X.X86_OP_REG:
                stores[-ops[0].mem.disp] = stores.get(-ops[0].mem.disp, 0) + 1
            pc += i.size
        for n, c in stores.items():
            if c >= 8 and kind.get(n) == 'var' and n not in self.immstores(start, end):
                kind[n] = 'temp'
        for n in os.environ.get('PSEUDO_TEMPS', '').split(','):      # e.g. PSEUDO_TEMPS=94,90
            if n:
                kind[int(n, 16)] = 'temp'
        return kind

    def immstores(self, start, end):
        out = set()
        pc = start
        while pc < end:
            i = self.g.code.get(pc)
            if i is None:
                pc += 1
                continue
            ops = i.operands
            if ops and ops[0].type == X.X86_OP_MEM and ops[0].mem.base == X.X86_REG_EBP                     and ops[0].mem.index == 0 and ops[0].mem.disp < 0                     and (i.mnemonic in ('inc', 'dec') or (i.mnemonic == 'mov' and ops[1].type == X.X86_OP_IMM)):
                out.add(-ops[0].mem.disp)
            pc += i.size
        return out

    # ------------------------------------------------------------------ operands
    def reg(self, r):
        n = self.i.reg_name(r)
        return SUB.get(n, n)

    def rd_reg(self, n):
        if n == 'eax':
            self.eax_used = True
        v = self.regs.get(n)
        return v if v is not None else Lin.of(n + '?')

    def addr(self, m):
        """Address of a memory operand as a Lin, plus the frame slot if it is one."""
        if m.base == X.X86_REG_EBP and m.index == 0:
            return None, m.disp
        a = Lin.of(m.disp)
        if m.base:
            a = a + Lin.of(self.rd_reg(self.reg(m.base)))
        if m.index:
            a = a + Lin.of(self.rd_reg(self.reg(m.index))).scale(m.scale)
        return a, None

    def memname(self, a, size=4):
        """Render a memory address (a Lin) as a C lvalue."""
        for base in ('this', 'p1', 'p2'):
            if a.t.get(base) == 1:
                rest = Lin({k: v for k, v in a.t.items() if k != base}, a.c)
                return self.member(base, rest, size)
        c = a.const()
        if c is not None:
            n = self.name(c)
            if n:
                return n.lstrip('_')
        # a constant part that is a symbol (a table): name it
        n = self.name(a.c) if a.c else None
        if n:
            rest = Lin(a.t, 0)
            return '%s[%s]' % (n.lstrip('_'), self.index(rest, 4))
        return '*(%s)' % a

    def index(self, lin, size):
        if all(v % size == 0 for v in lin.t.values()) and lin.c % size == 0:
            return str(lin.scale(1) if size == 1 else Lin({k: v // size for k, v in lin.t.items()}, lin.c // size))
        return '{bytes %s}/%d' % (lin, size)

    def member(self, base, rest, size):
        lay = LAYOUTS[base]
        prefix = '' if base == 'this' else base + '.'
        off = rest.c
        best = None
        for m in lay:
            if m[1] <= off:
                best = m
        # a negative constant with a variable index (row - 1): pick by nearest lower anyway
        if best is None:
            best = lay[0]
        nm, mo, dims = best[0], best[1], best[2]
        esz = best[3] if len(best) > 3 else 4
        rem = Lin(rest.t, off - mo)
        if not dims:
            if rem.t or rem.c:
                return '%s%s{+%s}' % (prefix, nm, rem)
            return prefix + nm
        if len(dims) == 1:
            return '%s%s[%s]' % (prefix, nm, self.index(rem, esz))
        row = dims[1] * esz
        # split terms into row and column parts by divisibility
        rt, ct = {}, {}
        for k, v in rem.t.items():
            if v % row == 0:
                rt[k] = v // row
            elif v % esz == 0:
                ct[k] = v // esz
            else:
                return '%s%s{bytes %s}' % (prefix, nm, rem)
        rc, cc = divmod(rem.c, row)
        if cc % esz:
            return '%s%s{bytes %s}' % (prefix, nm, rem)
        cc //= esz
        if cc > dims[1] // 2 + 1 and ct:       # a column index minus something: borrow a row
            pass
        return '%s%s[%s][%s]' % (prefix, nm, Lin(rt, rc), Lin(ct, cc))

    def val(self, op, size=None):
        """Value of an operand (Lin or string)."""
        if op.type == X.X86_OP_REG:
            n = self.i.reg_name(op.reg)
            return self.rd_reg(SUB.get(n, n))
        if op.type == X.X86_OP_IMM:
            v = op.imm
            if v >= 0x80000000:
                v -= 1 << 32
            n = self.reloc_target()
            if n is not None:
                if n in self.g.own and self.g.own[n].name.startswith('LC'):
                    return self.literal(n)
                nm = self.name(n)
                if nm:
                    return '&' + nm.lstrip('_')
            return Lin.of(v)
        a, slot = self.addr(op.mem)
        if slot is not None:
            return self.rd_slot(slot)
        t = self.reloc_target()
        if t is not None and a.const() is not None:
            if t in self.g.own and self.g.own[t].name.startswith('LC'):
                return self.literal(t)
        return Lin.of(self.memname(a, op.size))

    def reloc_target(self):
        i = self.i
        for off in (i.imm_offset, i.disp_offset):
            if off and (i.address + off) in self.g.relocs:
                return self.g.relocs[i.address + off][1]
        return None

    def rd_slot(self, d):
        """Value of the frame slot at ebp+d (d > 0: an argument)."""
        if d > 0:
            return Lin.of({8: 'this', 12: 'p1', 16: 'p2', 20: 'p3', 24: 'p4'}.get(d, 'arg%d' % d))
        n = -d
        if self.kinds.get(n) == 'temp' and n in self.slots:
            return self.slots[n]
        return Lin.of('v%X' % n)

    def lvalue(self, op):
        a, slot = self.addr(op.mem)
        if slot is not None:
            if slot > 0:
                return {8: 'this', 12: 'p1', 16: 'p2'}.get(slot, 'arg%d' % slot), None
            return 'v%X' % -slot, -slot
        return self.memname(a, op.size), None

    # ------------------------------------------------------------------ output
    def emit(self, s):
        self.flush_call()
        self.out.append('    ' + s)

    def flush_call(self):
        if self.pending and not self.eax_used:
            self.out.append('    %s;' % self.pending)
        self.pending = None

    def store(self, op, value):
        name, slot = self.lvalue(op)
        if slot is not None and self.kinds.get(slot) == 'temp':
            self.slots[slot] = value
            return
        if slot is not None and self.kinds.get(slot) == 'cw':
            return
        self.emit('%s = %s;' % (name, S(value)))

    # ------------------------------------------------------------------ the walk
    def function(self, start, end, title):
        self.kinds = self.classify(start, end)
        self.out = ['', '// %s' % title]
        temps = sorted(k for k, v in self.kinds.items() if v == 'temp')
        vars_ = sorted(k for k, v in self.kinds.items() if v == 'var')
        self.out.append('//   variables: %s' % ', '.join('v%X' % k for k in vars_))
        self.out.append('//   temporaries: %s' % ', '.join('-%X' % k for k in temps))
        self.dbl_args = set()
        self.pin(start, end)
        self.reset()
        pc = start
        while pc < end:
            i = self.g.code.get(pc)
            if i is None:
                pc += 1
                continue
            if pc in self.labels:
                self.flush_call()
                lab = self.g.own.get(pc)
                self.out.append('%s:' % (lab.name if lab else 'L_%06X' % pc))
                self.keep_slots = True
                self.reset()
                self.keep_slots = False
            self.i = i
            try:
                self.step(i)
            except Exception as e:          # keep going: print the instruction
                self.emit('/* asm %s %s */  // (%s)' % (i.mnemonic, i.op_str, e))
            pc += i.size
        self.flush_call()
        return self.out

    def pin(self, start, end):
        """Callee-saved registers loaded from an argument in the prologue and never written
        again (gcc keeps `this` in ebx): their value holds across labels."""
        self.pinned = {}
        pc, n = start, 0
        while pc < end and n < 12:
            i = self.g.code.get(pc)
            if i is None:
                break
            ops = i.operands
            if i.mnemonic == 'mov' and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_MEM                     and ops[1].mem.base == X.X86_REG_EBP and ops[1].mem.disp > 0                     and i.reg_name(ops[0].reg) in ('ebx', 'esi', 'edi'):
                self.pinned[i.reg_name(ops[0].reg)] = (pc, ops[1].mem.disp)
            pc += i.size
            n += 1
        for r, (at, d) in list(self.pinned.items()):
            pc = start
            while pc < end:
                i = self.g.code.get(pc)
                if i is None:
                    pc += 1
                    continue
                if pc != at and i.operands and i.operands[0].type == X.X86_OP_REG                         and SUB.get(i.reg_name(i.operands[0].reg), i.reg_name(i.operands[0].reg)) == r                         and i.mnemonic not in ('push', 'pop', 'cmp', 'test'):
                    del self.pinned[r]
                    break
                pc += i.size
        self.pinned = {r: self.rd_slot(d) for r, (at, d) in self.pinned.items()}

    def reset(self):
        self.regs = dict(getattr(self, 'pinned', {}))
        self.slots = getattr(self, 'slots', {}) if getattr(self, 'keep_slots', False) else {}
        self.fst = []
        self.args = []
        self.flags = None
        self.pending = None
        self.eax_used = True

    def setreg(self, n, v):
        if n == 'eax':
            self.flush_call()
            self.eax_used = True
        self.regs[n] = v

    def step(self, i):
        m = i.mnemonic
        ops = i.operands
        if m in ('push',) and ops[0].type == X.X86_OP_REG and self.i.reg_name(ops[0].reg) in ('ebp', 'edi', 'esi', 'ebx') \
                and i.address < self.func_start + 8:
            return
        if m == 'push':
            if ops[0].type == X.X86_OP_REG and self.i.reg_name(ops[0].reg) in ('ebp',):
                return
            if ops[0].type == X.X86_OP_REG and self.i.reg_name(ops[0].reg) in ('ebx', 'esi', 'edi')                     and self.at_epilogue():
                return
            self.args.append(self.val(ops[0]))
            return
        if m == 'pop' and self.args and ops[0].type == X.X86_OP_REG:
            n = self.i.reg_name(ops[0].reg)
            v = self.args.pop()
            self.setreg(SUB.get(n, n), Lin.of(U(v)) if isinstance(v, str) else Lin.of(v))
            return
        if m in ('pop', 'leave', 'ret', 'nop', 'cld'):
            if m == 'ret':
                self.emit('return;  // eax = %s' % S(self.regs.get('eax', '?')))
            return
        if m == 'call':
            t = self.reloc_target()
            if t is None:
                try:
                    t = int(i.op_str, 16)
                except ValueError:
                    t = None
            fn = (self.name(t) if t is not None else None) or ('(*%s)' % S(self.val(ops[0])))
            fn = fn.lstrip('_') if fn.startswith('_') and '::' not in fn else fn
            # the call takes as many dwords as the `add esp, N` after it removes
            nxt = self.g.code.get(i.address + i.size)
            want = 0
            if nxt is not None and nxt.mnemonic == 'add' and nxt.op_str.startswith('esp, '):
                want = int(nxt.op_str.split(', ')[1], 0) // 4
            taken = []
            while want > 0 and self.args:
                a = self.args.pop()
                taken.append(a)
                want -= 2 if (isinstance(a, str) and not a.startswith('"') and not a.startswith('&')
                              and a != '<double>' and self.is_double_expr(a)) or a == '<double>' else 1
            # two immediate pushes make a double constant (low word pushed last)
            args = []
            k = 0
            while k < len(taken):
                a = taken[k]
                if k + 1 < len(taken) and isinstance(a, Lin) and isinstance(taken[k + 1], Lin)                         and a.const() is not None and taken[k + 1].const() is not None                         and 0x3f000000 <= (taken[k + 1].c & 0xffffffff) <= 0x41000000:
                    args.append(repr(struct.unpack('<d', struct.pack('<II', a.c & 0xffffffff,
                                                                         taken[k + 1].c & 0xffffffff))[0]))
                    k += 2
                    continue
                args.append(a)
                k += 1
            if '::' in fn and args:
                call = '%s.%s(%s)' % (S(args[0]), fn.split('::')[-1], ', '.join(S(a) for a in args[1:]))
            else:
                call = '%s(%s)' % (fn, ', '.join(S(a) for a in args))
            self.flush_call()
            self.regs = {k: v for k, v in self.regs.items() if k in ('ebx', 'esi', 'edi')}
            self.fst = []
            if fn.split('(')[0] in DOUBLE_FUNCS:
                self.fst.append(call)
                return
            self.regs['eax'] = Lin.of(U(call))
            self.pending = call
            self.eax_used = False
            return
        if m in ('add', 'sub') and ops[0].type == X.X86_OP_REG and self.i.reg_name(ops[0].reg) == 'esp':
            if m == 'sub' and ops[1].type == X.X86_OP_IMM and ops[1].imm == 8:
                self.args.append('<double>')
            if m == 'sub' and ops[1].type == X.X86_OP_IMM and ops[1].imm == 4:
                self.args.append('<int>')
            return
        if m == 'mov' and ops[0].type == X.X86_OP_REG and self.i.reg_name(ops[0].reg) == 'ebp':
            return
        if m == 'lea':
            dst = self.reg(ops[0].reg)
            a, slot = self.addr(ops[1].mem)
            if slot is not None:
                v = Lin.of('&v%X' % -slot) if slot < 0 else Lin.of('&arg%d' % slot)
            else:
                v = a
            if self.i.reg_name(ops[0].reg) == 'esp':
                return
            self.setreg(dst, v)
            return
        if m in ('mov', 'movzx', 'movsx'):
            if ops[0].type == X.X86_OP_REG:
                n = self.i.reg_name(ops[0].reg)
                if n in ('ah', 'ch', 'dh') and ops[1].type == X.X86_OP_IMM:
                    return                          # FPU control word rounding mode
                self.setreg(SUB.get(n, n), self.val(ops[1]))
            else:
                self.store(ops[0], self.val(ops[1]))
            return
        if m in ('add', 'sub', 'and', 'or', 'xor', 'shl', 'sar', 'shr', 'imul') and len(ops) == 2:
            a = self.val(ops[0])
            b = self.val(ops[1])
            if m == 'xor' and ops[1].type == X.X86_OP_REG and ops[0].type == X.X86_OP_REG \
                    and ops[0].reg == ops[1].reg:
                r = Lin.of(0)
            elif m == 'add':
                r = Lin.of(a) + Lin.of(b) if not isinstance(a, str) and not isinstance(b, str) else '%s + %s' % (S(a), S(b))
            elif m == 'sub':
                r = Lin.of(a) - Lin.of(b)
            elif m == 'shl' and isinstance(b, Lin) and b.const() is not None:
                r = Lin.of(a).scale(1 << b.c)
            elif m == 'imul' and isinstance(b, Lin) and b.const() is not None:
                r = Lin.of(a).scale(b.c)
            elif m == 'imul' and isinstance(a, Lin) and a.const() is not None:
                r = Lin.of(b).scale(a.c)
            else:
                sym = {'and': '&', 'or': '|', 'xor': '^', 'sar': '>>', 'shr': '>>u', 'shl': '<<', 'imul': '*'}[m]
                r = Lin.of('%s %s %s' % (wrap(a), sym, wrap(b)))
            if ops[0].type == X.X86_OP_REG:
                n = self.i.reg_name(ops[0].reg)
                self.setreg(SUB.get(n, n), r)
            else:
                name, slot = self.lvalue(ops[0])
                if slot is not None and self.kinds.get(slot) == 'temp':
                    self.slots[slot] = r
                else:
                    op = {'add': '+=', 'sub': '-=', 'and': '&=', 'or': '|=', 'xor': '^=', 'shl': '<<=', 'sar': '>>=', 'imul': '*='}[m]
                    self.emit('%s %s %s;' % (name, op, S(b)))
            self.flags = ('val', r)
            return
        if m in ('imul',) and len(ops) == 3:
            self.setreg(self.reg(ops[0].reg), Lin.of(self.val(ops[1])).scale(ops[2].imm))
            return
        if m in ('inc', 'dec', 'neg', 'not'):
            if ops[0].type == X.X86_OP_REG:
                n = self.reg(ops[0].reg)
                a = Lin.of(self.val(ops[0]))
                self.setreg(n, a + 1 if m == 'inc' else a - 1 if m == 'dec' else a.scale(-1) if m == 'neg' else Lin.of('~' + wrap(a)))
            else:
                name, slot = self.lvalue(ops[0])
                if slot is not None and self.kinds.get(slot) == 'temp':
                    a = Lin.of(self.slots.get(slot, 'v%X' % slot))
                    self.slots[slot] = a + 1 if m == 'inc' else a - 1 if m == 'dec' else a.scale(-1)
                else:
                    self.emit('%s%s;' % (name, {'inc': '++', 'dec': '--', 'neg': ' = -' + name, 'not': ' = ~' + name}[m]))
            return
        if m == 'cdq':
            return
        if m == 'idiv' or m == 'div':
            d = self.val(ops[0])
            a = self.rd_reg('eax')
            self.regs['eax'] = Lin.of('%s / %s' % (wrap(a), wrap(d)))
            self.regs['edx'] = Lin.of('%s %% %s' % (wrap(a), wrap(d)))
            return
        if m in ('cmp', 'test'):
            self.flags = (m, self.val(ops[0]), self.val(ops[1]))
            return
        if m.startswith('set'):
            n = self.i.reg_name(ops[0].reg) if ops[0].type == X.X86_OP_REG else None
            c = self.cond(m[3:])
            if n:
                self.setreg(SUB.get(n, n), Lin.of(c))
            else:
                self.store(ops[0], Lin.of(c))
            return
        if m.startswith('j'):
            try:
                t = int(i.op_str, 16)
                lab = self.g.own.get(t)
                tn = lab.name if lab else 'L_%06X' % t
            except ValueError:
                tn = None
            if m == 'jmp':
                if tn is None:
                    self.emit('goto *%s;  // switch' % S(self.val(ops[0])))
                else:
                    self.emit('goto %s;' % tn)
                self.reset_soft()
                return
            self.emit('if (%s) goto %s;' % (self.cond(m[1:]), tn))
            return
        if m.startswith('f'):
            return self.fpu(i, m, ops)
        if m == 'rep movsd':
            self.emit('memcpy(%s, %s, 4 * %s);' % (S(self.rd_reg('edi')), S(self.rd_reg('esi')), S(self.rd_reg('ecx'))))
            return
        self.emit('/* asm %s %s */' % (m, i.op_str))

    def is_double_expr(self, a):
        return a in self.dbl_args

    def at_epilogue(self):
        return False

    def reset_soft(self):
        self.regs = dict(self.pinned)
        self.slots = {}

    def cond(self, cc):
        f = self.flags
        if f is None:
            return '?flags %s' % cc
        if f[0] == 'cmp':
            a, b = S(f[1]), S(f[2])
            op = {'e': '==', 'z': '==', 'ne': '!=', 'nz': '!=', 'g': '>', 'ge': '>=', 'l': '<', 'le': '<=',
                  'a': '>u', 'ae': '>=u', 'b': '<u', 'be': '<=u'}[cc]
            return '%s %s %s' % (a, op, b)
        if f[0] == 'test':
            a, b = f[1], f[2]
            x = S(a) if S(a) == S(b) else '%s & %s' % (wrap(a), wrap(b))
            op = {'e': '==', 'z': '==', 'ne': '!=', 'nz': '!=', 'g': '>', 'ge': '>=', 'l': '<', 'le': '<=',
                  's': '<', 'ns': '>='}[cc]
            return '%s %s 0' % (x, op)
        if f[0] == 'fcom':
            return '%s ?%s' % (f[1], cc)
        return '(%s) %s 0' % (S(f[1]), cc)

    def fpu(self, i, m, ops):
        st = self.fst

        def top():
            return st[-1] if st else 'st?'
        if m in ('fnstcw', 'fldcw', 'fwait', 'sahf'):
            return
        if m == 'fild':
            if ops[0].mem.base == X.X86_REG_ESP and self.args:      # push x; fild [esp]
                st.append('(double)%s' % wrap(self.args.pop()))
                return
            st.append('(double)%s' % wrap(self.val(ops[0])))
            return
        if m == 'fld':
            if ops[0].type == X.X86_OP_REG:
                k = int(i.op_str.split('(')[1].rstrip(')')) if '(' in i.op_str else 0
                st.append(st[-1 - k] if len(st) > k else 'st?')
            else:
                st.append(S(self.val(ops[0])))
            return
        if m == 'fld1':
            st.append('1.0')
            return
        if m == 'fldz':
            st.append('0.0')
            return
        if m == 'fchs':
            st[-1] = '-' + wrap(top())
            return
        if m in ('fstp', 'fst', 'fistp', 'fist'):
            v = top()
            if m in ('fistp', 'fist'):
                v = '(int)' + wrap(v)
            if ops and ops[0].type == X.X86_OP_MEM and ops[0].mem.base == X.X86_REG_ESP:
                if self.args and self.args[-1] in ('<double>', '<int>'):
                    self.args[-1] = v
                else:
                    self.args.append(v)
                self.dbl_args.add(v)
            elif ops and ops[0].type == X.X86_OP_MEM:
                self.store(ops[0], v)
            if m.endswith('p') and st:
                st.pop()
            return
        base = m.rstrip('p')
        pop = m.endswith('p') and m not in ('fcomp', 'fucomp')
        sym = {'fadd': '+', 'fsub': '-', 'fsubr': '-r', 'fmul': '*', 'fdiv': '/', 'fdivr': '/r',
               'fiadd': '+', 'fisub': '-', 'fimul': '*', 'fidiv': '/', 'fisubr': '-r', 'fidivr': '/r'}
        if base in sym:
            s = sym[base]
            rev = s.endswith('r')
            s = s[0]
            if len(ops) == 0 or (ops and ops[0].type == X.X86_OP_REG and len(ops) == 1):
                # fXXXp st(1): st1 = st1 op st0 (or st0 op st1 for r), pop
                if len(st) >= 2:
                    a, b = st[-2], st[-1]
                    st.pop()
                    st[-1] = '%s %s %s' % (wrap(b), s, wrap(a)) if rev else '%s %s %s' % (wrap(a), s, wrap(b))
                return
            if ops[0].type == X.X86_OP_MEM:
                v = S(self.val(ops[0]))
                if base.startswith('fi'):
                    v = '(double)' + wrap(v)
                st[-1] = '%s %s %s' % (wrap(v), s, wrap(top())) if rev else '%s %s %s' % (wrap(top()), s, wrap(v))
                return
            # two register operands
            d = int(i.op_str.split(',')[0].split('(')[1].rstrip(')')) if 'st(' in i.op_str.split(',')[0] else 0
            k = int(i.op_str.split(',')[1].split('(')[1].rstrip(')')) if 'st(' in i.op_str.split(',')[1] else 0
            a, b = st[-1 - d], st[-1 - k]
            st[-1 - d] = '%s %s %s' % (wrap(b), s, wrap(a)) if rev else '%s %s %s' % (wrap(a), s, wrap(b))
            if pop:
                st.pop()
            return
        if base in ('fcom', 'fucom', 'fcompp', 'fucompp', 'ftst'):
            other = S(self.val(ops[0])) if ops and ops[0].type == X.X86_OP_MEM else (st[-2] if len(st) > 1 else '?')
            self.flags = ('fcom', '%s <=> %s' % (wrap(top()), wrap(other)))
            if m.endswith('pp'):
                st.pop()
                st.pop()
            elif m.endswith('p'):
                st.pop()
            return
        if m == 'fnstsw':
            return
        self.emit('/* asm %s %s */' % (m, i.op_str))


def wrap(x):
    s = S(x)
    return s if is_simple(s) or (s.startswith('(') and s.endswith(')') and s.count('(') == 1) else '(%s)' % s


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    ak = coff.Coff(open(os.path.join(ROOT, 'build', 'AK.COF'), 'rb').read())
    p = Pseudo(ak, sys.argv[1])
    want = sys.argv[2] if len(sys.argv) > 2 else None
    funcs = [(s.value, s) for s, _ in ak.functions(p.g.obj)]
    funcs += [(v, s) for v, s in p.g.own.items() if s.sclass == 6 and '__F' in s.name]
    funcs.sort()
    for k, (v, s) in enumerate(funcs):
        end = funcs[k + 1][0] if k + 1 < len(funcs) else p.g.b
        title = coff.dm(s.name)
        if want and want not in title:
            continue
        p.func_start = v
        print('\n'.join(p.function(v, end, '%s  %06X' % (title, v))))


if __name__ == '__main__':
    main()
