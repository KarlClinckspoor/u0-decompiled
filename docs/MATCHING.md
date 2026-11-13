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

## The link (proven in phase 3)

gcc 2.7.2.1 links with

```
ld -o ak crt0.o ak.o akaux.o akchar.o akmenu.o akworld.o -lalleg -liostr -Tdjgpp.djl -lgcc -lc -lgcc
```

and then runs `stubify` (which the COFF comparison leaves out). That command, with liballeg.a
built by `tools/buildalleg.py` and the stand-ins, reproduces AK.COF's headers, `.text` and `.data`
exactly (`tools/build.py`). The game objects must therefore keep their exact sizes. Each ends on
a 4-byte boundary, and ld places objects back to back.
