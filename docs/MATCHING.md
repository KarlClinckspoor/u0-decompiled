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
- String literals of a function follow the function in `.text` (`LCn` labels). Doubles a function
  uses come before it.
