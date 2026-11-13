# Matching with gcc 2.7.2.1 (DJGPP)

How to write C++ that compiles to the original bytes of AK.EXE. Everything here was proven in
phase 2 (2026-11-13). Add to it as phase 3 and phase 4 teach more, the way U1's MATCHING.md grew.

## The toolchain (proven)

| part | what built AK.EXE | evidence |
|---|---|---|
| compiler | **gcc 2.7.2.1** (DJGPP, `gcc2721b` + `gpp2721b`) | libc.a's ident in AK.EXE says "by gcc 2.7.2.1"; Allegro objects rebuild identical |
| game flags | **`-O0`** (no optimization), plus `-g` for ak.cc only; no `-m486` | `akmenu::akmenu()`, `akmenu::~akmenu()` and `akworld::sgn(int)` match; `-O`/`-O2` remove the jump-to-next-instruction the original has |
| game assembler | **binutils 2.8.1** (`bnu281b`) | 2-byte alignment filler `89 f6` (2.7 writes `8d 36`); local `L` labels kept in the symbol table |
| Allegro 2.2 | gcc 2.7.2.1 `-m486 -O3 -ffast-math -fomit-frame-pointer` (the makefile's), assembled with **binutils 2.7** | text.c, math.c, gfx.c rebuild identical (code and data); their fillers are 2.7's |
| libraries | DJGPP 2.01 libc.a/libm.a, libgcc.a, libg++ 2.7.1 libiostr.a, crt0.o, **as shipped** | 159 + 5 + 21 objects and crt0 byte-identical to AK.COF |

So Allegro was built (as liballeg.a) with binutils 2.7, and the game later, after an upgrade to
2.8.1. `tools/setup.py` installs 2.8.1 as the default `as`/`ld` and keeps 2.7 in
`D:\BNU27\BIN`. `gcc -BD:/BNU27/BIN/` (or `tools/gxx.py --bnu27`) selects 2.7.

The two binutils differ only in alignment fillers. Measured with the same `.align 4` test
(`build/fill_test`):

| fill | 2.7 | 2.8.1 |
|---|---|---|
| 1 | `90` | `90` |
| 2 | `8d 36` | `89 f6` |
| 3 | `8d 76 00` | `8d 76 00` |
| 4-8 | same in both | same in both |
| 9-14 | second part `8d b4 26 ...` (esi) | second part `8d bc 27 ...` / `8d bf ...` (edi) |
| 15 | `eb 0d` + 13 `90` | same |

Alignment is relative to the object: ld aligns each object's `.text` only to 4, so look at a
function's offset from its object's start, not at its absolute address. At `-O0` without
`-m486`, gcc aligns functions (and labels after jumps) to 4, so the game only shows fills of 1-3.

## The loop

```sh
python tools/setup.py                                # once: build/tc from the archives
python tools/gxx.py FILE.cc                          # compile (default -O0) and compare
python tools/gxx.py FILE.cc --flags="-O0 -g"         # other flags
python tools/gxx.py FILE.c --bnu27 --flags="-m486 -O3 -ffast-math -fomit-frame-pointer"
```

`gxx.py` copies the file to `build/gxx`, compiles it in DOSBox (about 4 s), and compares every
function of the object with the function of the same mangled name in `build/AK.COF`. The
comparison covers the function's whole length up to the next function symbol, so the literals
placed after a function count too. Relocated words are masked on both sides. The last function of
a partial file ends in alignment filler where the original continues; that is reported as a
match. Use the u1 venv's Python for disassembly of the first difference (capstone).

`tools/dos.py` runs any DJGPP command: `python tools/dos.py WORKDIR "gcc ..." "ld ..."`. Long
command lines go through `@response` files automatically (DOS cuts lines at 127 characters).
Source names must be given in lower case (`foo.cc` is C++; `FOO.CC` is taken for a linker input).

## What -O0 code from gcc 2.7.2.1 looks like (observed)

- Frame `push ebp; mov ebp,esp; [sub esp,N]; [push edi; push esi; push ebx]`. `this` is
  loaded into a register (edx or ebx) even at -O0.
- Every `return` jumps to a shared epilogue label, even from the instruction right before it
  (`jmp` with displacement 0: `eb 00`). The epilogue is `leave; ret`, or
  `mov ebx,[ebp-4]; leave; ret` when ebx was saved.
- After a call, `mov eax,eax` before the result is stored (`akaux::akaux()`).
- Locals and arguments live in memory and are reloaded for every use.
- An assignment inside a return stores and then loads the constant again: `return n = 1;` gives
  `mov dword [ebp+0xc],1; mov eax,1; jmp epilogue`. That is how `akworld::sgn` was written:

  ```cpp
  int akworld::sgn(int n)
  {
      if (n > 0) return n = 1;
      if (n < 0) return n = -1;
      if (n == 0) return n = 0;
      return n;
  }
  ```

- An empty constructor of a class without virtual functions returns `this`
  (`mov eax,[ebp+8]; jmp L; L: leave; ret`). An empty destructor tests the delete flag
  (`and eax,1; test eax,eax; je`), and calls `__builtin_delete(this)` when it is set.
- String literals and doubles of a function come **before** it in `.text` (`LCn` labels), so
  they sit between the previous function and this one.
- Operand order shows in the code: `pos == max` loads the left operand and compares the right one
  from memory (`mov eax,[pos]; cmp [max],eax`); `max == pos` gives the reverse. In a sum,
  `4 + t1 + t2` gives `mov eax,[t2]; add eax,4; mov esi,[t1]; add esi,eax`, and `4 + t2 + t1`
  the reverse. Try the other order when only the registers or operands swap.
- A bitwise `&` between comparisons (`t1 == 1 & t2 != 1`) gives `cmp/sete dl; movzx eax,dl;
  cmp/setne cl; movzx edx,cl; and al,dl; test al,al; je`; `&&` would branch instead. The game
  uses `&` this way throughout akmenu.cc.
- Locals take frame slots downwards in declaration order. A declaration in the middle of a block
  (`int key = 0;` after two `ifstream` objects) takes its slot after them. When objects with
  destructors are in scope, `return x;` stores x in a hidden slot below the locals, runs the
  destructors (virtual, with flag 2, through the object's ios pointer for an ifstream), and
  then loads it.
- A header's `extern inline` function that is used (allegro.h's `textout(..., char *, ...)`) is
  emitted as a local copy at the end of the object.
- `x % 8 == 0` and `(x & 7) == 0` both become an `and`, but differently: `% 8` copies the value
  first (`mov edx,eax; and edx,7; test edx,edx`), `& 7` works in place (`and eax,7; test eax,eax`).
  akaux.cc's scrollers use `% 8`. Likewise `readkey() & 0xff` gives `movzx esi,al`.
- A local array with an initialiser (`char *quote[] = {...}`) is copied at entry from a table in
  `.text` placed before the function (`lea edi,[frame]; mov esi,LCn; cld; mov ecx,N; rep movsd`).
  The table's words point at the strings' `LC` labels; identical strings share one literal.
  Entries missing from a sized array are zero (akaux.cc's credits end in a 0 word).
- An assignment inside a condition stores and reloads: `if ((f = fopen(...)) == NULL)` gives
  `mov [f],eax; mov eax,[f]; test eax,eax`, while `if (f == NULL)` after a separate assignment
  gives `cmp [f],0`. Same for `while ((line = strsep(rest, ";")) != NULL)`.
- Identical aggregate initialisers share one table: akchar.cc's `charcreate()` and `shop()` both
  copy their `char *stat[]` from the same `LC6` (gcc hashes constants).
- Block-local variables get stack slots that are freed at the end of the block and reused by a
  later block, last-freed first: in `lb()` the third block must declare `text` before `rest` to
  get the slots the original has.
- `while`, `do` and `for` start with `emit_nop()`, which emits a `nop` at -O0 only when the last
  instruction is a label. Two labels before a loop's `nop` therefore mean a user label: akchar.cc's
  `shop()` has `again:` before its `while`, and `default:` does `goto again;` (a `continue` would
  jump past the `nop`).

### Dead jumps at the end of a function

Some functions end, after their last statement, with one or two dead `jmp`s to the epilogue, each
on its own 4-aligned slot (`eb 06 89 f6 eb 02 89 f6`). They come from gcc's C++ front end
(`finish_function` in cp/decl.c, read in gcc2721s.zip): when the function has a return label and
no recorded return value, it emits `goto no_return_label` and then `jump return_label`. So:
- a function ending with `return N;` gets its own jump plus two dead ones;
- a function falling off its end gets one live and one dead jump;
- otherwise there are none.

"No recorded return value" is the global `current_function_return_value`. gcc sets it when a
`return` returns a declaration outside any condition, loop or switch: a constructor's implicit
`return this`, or a return through a temporary when destructors are pending (akmenu.cc's
`runmenu`). It clears it at user labels and `case` labels, and **never resets it between
functions**. The dead jumps therefore depend on the functions before, in file order:
- in akchar.cc the constructor sets it, so `charcreate()` gets none;
- `shop()`'s `switch` clears it, so `shop()` and everything after get them;
- in akmenu.cc `runmenu()` sets it again, so `draw()` and `redraw()` get none.

Compile whole files in their original order; a one-function probe can show a difference that
the full file does not have.

- **Labels are numbered across the whole file, headers included.** The order of `#include`s
  shows in the label numbers even when the code is identical: akmenu.cc needs `<fstream.h>`
  before `<allegro.h>` to get the original's numbers (L332 for the textout copy, L455 on in the
  game's code). akaux.cc on the other hand includes no iostream header before allegro.h (its
  textout copy is L51). `tools/gxx.py` compares the symbols when a file is a whole object.
- Calls to the object's own inline copies are pc-relative without relocation, so any size
  difference earlier in the file changes all of them. `gxx.py` compares such calls by the name
  of their target, which keeps the reported first difference the real one.
- A `switch` with a jump table is `cmp [ebp-N],max; ja default; mov r,[ebp-N];
  mov r,[r*4+table]; mov [ebp-N],r; jmp [ebp-N]`. The table (absolute addresses of the cases)
  sits in `.text` right after the `jmp`, aligned to 4.

### Debug information (ak.cc, `-g`)

`-g` changes no code (`rungame` matched at once), but it records a lot, and all of it can be
matched. `tools/gxx.py` compares every debug symbol (`DEBUG`) and the line numbers (`LINES`) when
the object has them. `tools/listing.py` prints the line before each statement.
- **Line numbers** place every statement on its line. Statements on one line share an entry.
  Case labels, declarations without an initialiser, braces and `else` get none. The function's
  `.bf` line is its `{`. A `for` loop's increment gets the line of the body's closing `}`.
  Everything else is free: the comments and blank lines in `src/ak.cc` fill exactly the gaps
  the line table leaves. Where code shares a line in the original (`case 80: player.sav.dx = ...`,
  `case 'y': break;`), it has to share a line in the source too.
- **The types** come out in the order of their definitions, headers included. The anonymous
  structs of stdlib.h come first, then libio's, then math.h's `exception`, then dpmi.h's (pulled
  in by allegro.h), then Allegro's. So ak.cc includes stdlib.h, iostream.h, math.h and allegro.h
  in that order. The textout copy's label (L278) confirms it.
- **Array dimensions** are stored outermost first: akworld's `per` is `int[11][4]`.
- **Blocks** (`.bb`/`.eb`) come from binding levels that hold declarations or other blocks.
  `main` also has empty ones, a `.bb` and `.eb` at the same address, at each `break` of its menu
  switch. They are not in the source: `expand_fixup` (stmt.c) makes one for every forward jump
  when an object with a destructor is live (`aux`, `player`, `w`, `menu`). gcc reproduces them.

### Constant folding moves constants

gcc's `fold` reassociates `+`/`-` with a constant, so where the constant ends up in the code
tells the source form. With the call to `rand()` evaluated first:
- `a - 3 - b` becomes `a - (b + 3)`: `lea esi,[ebx+3]; sub eax,esi`;
- `a - b - 3` becomes `(a - b) + -3`: `sub; add -3`;
- `a - (b + 3)` becomes `(a - 3) - b`: `add ecx,-3` before the division, then `sub`. This is
  `rundungeon`'s trap damage, `character[0] - (rand() % inout + 3)`, the Apple original's
  `C(0)-INT(RND(1)*IN+3)` form.

### The order of label symbols shows the order of case labels

gas enters a local label into the symbol table when it first sees it, defined or referenced.
A switch's dispatch (the decision tree or the jump table) comes before the cases. So the labels
of a group like `case 'X': case 'x': case 'E': case 'e':` are listed in the order of the case
values (`E e X x`), while their numbers follow the order of the source. `gxx.py`'s SYMBOLS check
sorts by address and misses this; the DEBUG check compares the symbol table in order and finds
it. In ak.cc: `X x E e` on the world map, `X x K k E e` in the dungeon, `Q q 19` for saving.

## The link (proven in phase 3)

gcc 2.7.2.1 links with

```
ld -o ak crt0.o ak.o akaux.o akchar.o akmenu.o akworld.o -lalleg -liostr -Tdjgpp.djl -lgcc -lc -lgcc
```

and then runs `stubify` (which the COFF comparison leaves out). That command, with liballeg.a
built by `tools/buildalleg.py` and the stand-ins, reproduces AK.COF's headers, `.text` and `.data`
exactly (`tools/build.py`). The game objects must therefore keep their exact sizes. Each ends on
a 4-byte boundary, and ld places objects back to back.
