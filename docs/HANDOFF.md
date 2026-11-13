# Handoff: Akalabeth decompilation (u0-decompiled)

State as of 2026-11-13: phases 0 to 3 of docs/PLAN.md are done. **The program links with
`.text` and `.data` identical to the original**, from the real libraries and assembly
stand-ins for the game objects. Phase 4 (the game in C++) has started: akmenu.cc,
akaux.cc, akchar.cc and ak.cc match completely, symbols included (ak.cc's `-g` debug symbols and
line numbers too). Only akworld.cc is left. Read this first, then
docs/PLAN.md and docs/MAP.md. The sister projects `../u1-decompiled` (compiled C, the closest
model), `../u2-decompiled` and `../u3-decompiled` are the models for conventions.

## Headline

- **AK.EXE is unpacked.** `tools/wdosx.py` reads the WDOSX 0.97 wrapper and unpacks the DJGPP
  COFF program to `build/AK.COF` (570462 bytes, sha1 3badf300cef0032f57b12468b51b448c3926bcab).
  The LZ decoder is the one the WDOSX kernel uses on itself, read from the kernel's 16-bit code
  at 0083. It also unpacks the kernel image. Every chunk ends exactly where the next begins.
  WDOSX's source was not needed.
- **The program is not stripped**, and that changes the size of the job (docs/MAP.md):
  - 268 objects in link order, each with its exact `.text`/`.data`/`.bss` range;
  - every function and variable named; C++ names demangle to classes, methods and argument
    types (`tools/coff.py` has a gcc 2.x demangler);
  - every local label (`L402`, `LC7`) kept;
  - **ak.cc was compiled with `-g`**: the game's class layouts with member names, locals with
    frame offsets, and 316 line numbers.
- **The game is 5 objects, 36 functions, 100132 bytes of `.text`** (code and literals), and has
  no `.data` or `.bss` of its own. The rest is library code: Allegro 2.2 (113 KB), libgpp/libio
  (57 KB), libgcc and the DJGPP 2.01 libc (59 KB), plus crt0. Two functions dominate:
  `akworld::drawmonsters` (39 KB) and `akworld::drawdungeon` (14 KB).
- The member names are Garriott's Applesoft variables (TX, TY, PX, PY, INOUT, TASK, LK, PER, LAD,
  FT, XX, YY, MZ, ML, ...). The Apple II listing will be a strong naming reference.

## Facts learned (beyond the plan's survey)

- The WFSE entry header is `WFSE`, entry size, unpacked size, a dword 0x26 (meaning unknown),
  and a 16-byte name field. The bytes after the name's zero are junk that differs between AK.EXE
  and SETUP.EXE (`0600b9020000` and `0600 8c060000`), probably stack leftovers in WDOSX's
  `stubit`. Phase 7 has to reproduce or copy them.
- A chunk that does not get smaller is stored as it is (two in SETUP.EXE).
- COFF timestamp is 0 and there are no relocations: nothing time-dependent in the link.
- Code fingerprints are in docs/MAP.md. The game has a frame pointer, uses register variables
  and aligns functions to 4 (no `-m486`). Allegro was built `-fomit-frame-pointer`. The doubles
  a function uses are in `.text` just before it. akmenu.cc has its own copy of the header inline
  `textout(BITMAP *, FONT *, char *, int, int, int)` (as a local label), and so does ak.cc (as
  STAT).
- AK.DAT (`tools/akdat.py`): one `slh!` LZSS packfile around an `ALL.` datafile of 7 objects:
  `akins` (the instructions), `aklb1`-`aklb3` (Lord British's speeches), fonts `ARIAL_BLACK`
  and `NEAT_FONT` (proportional, from PCX files), and the grabber's info. The ORIG properties
  show the game was built in `c:\djgpp\bin`, with a grabber header `c:\djgpp\bin\ak.h`, dated
  13-20 November 1997.
- SETUP.EXE's COFF is stripped (no symbols). It is Allegro 2.2's stock setup program.

## Phase 3 (done 2026-11-13): every library object identical, sections link identical

- `python tools/buildalleg.py` builds Allegro 2.2's liballeg.a with its own makefile (binutils
  2.7 first on `COMPILER_PATH`; make 3.75 misses `asmdef.c` through VPATH, so the script builds
  and runs asmdef first). It takes about 45 s.
- `python tools/libmatch.py` pairs every object of AK.COF with a library member **by symbol**
  and compares `.text`/`.data` (relocated fields masked) and `.bss` size. Result: **all 257
  library objects identical**: crt0, 187 libc.a (stubs included), 7 libgcc.a, 22 libiostr.a,
  40 liballeg.a. 6 objects are empty and 5 are the game.
- `python tools/standin.py` writes `src/standin/ak*.s`. Each is a gas source with the game
  object's original bytes, where every address is a relocation: `.long _screen`, `.long LC12`,
  `call _textout`, and switch tables as `.long L…`. Code is found by following control flow.
  At `-O0`, gcc 2.7.2.1 compiles a switch as `mov r,[r*4+table]; mov [ebp-N],r; jmp [ebp-N]`,
  bounded by the `cmp` before it. Every code label is reached. No constant was mistaken for an
  address: every target in the object's own code is a real label, and the only external data
  the game touches are `screen`, `font`, `cout`, `cin` and `__dj_stderr`.
- `python tools/build.py` assembles the stand-ins (or compiles `src/NAME.cc` once it exists) and
  links as gcc does: `ld -o ak crt0.o OBJS -lalleg -liostr -Tdjgpp.djl -lgcc -lc -lgcc`. It then
  compares with AK.COF. **File header, a.out header, section headers, `.text` (331608 bytes)
  and `.data` (27136 bytes) are identical**, in about 6 s. The symbol table differs (7678
  symbols against 9382, no line numbers, so ld also sets the F_LNNO flag, 0x107 against 0x103).
  That is ak.cc's `-g` information and the game's local symbols, which the C++ will bring back.

## Phase 4 (started 2026-11-13): the game in C++

| object | state |
|---|---|
| akmenu.cc | **done**: `src/akmenu.cc` + `src/akmenu.h`. All 5 functions and the inline textout copy match, and all 53 symbols (label numbers included) are the original's. Stand-in deleted. The link stays identical |
| akaux.cc | **done**: `src/akaux.cc` + `src/akaux.h` + `src/ak.h` (AK.DAT's object indexes). All 6 functions, the quote and credit tables, the two inline copies and all 158 symbols match. Stand-in deleted |
| akchar.cc | **done**: `src/akchar.cc` + `src/akchar.h`. All 9 functions (character creation, shop, save/load, Lord British, upkeep, death) and all 249 symbols match. Stand-in deleted. The tricky part, end-of-function dead jumps that depend on the functions before them, is explained in MATCHING.md |
| ak.cc | **done**: `src/ak.cc` + `src/akworld.h`. Compiled `-O0 -g`. All 3 functions (`rungame`, `rundungeon`, `main`) and the textout copy match. So do the 185 label symbols, all 1179 debug symbols (types, locals, blocks, `.bf`/`.ef`) and all 316 line numbers. Stand-in deleted |
| akworld.cc | stand-in |

`python tools/listing.py OBJ [FUNC]` prints a readable disassembly to write from, with symbol
names, strings, switch tables and, for ak.cc, the source line of each statement.
`tools/gxx.py src/NAME.cc` compares functions, and also symbols when the file is a whole object.
For a `-g` object it also compares every debug symbol and the line numbers (`DEBUG`, `LINES`).

## Repository

| file | what |
|---|---|
| `tools/wdosx.py` | WDOSX reader and unpacker (`info`, `unpack`); the LZ format is in its docstring |
| `tools/coff.py` | DJGPP COFF reader: `info`, `files`, `funcs`, `syms`, `types`, `demangle` |
| `tools/akdat.py` | AK.DAT reader: `list`, `extract` (to build/akdat, never committed) |
| `tools/setup.py` | unpacks the sha1-checked archives into `build/tc` (DJGPP tree, binutils 2.7 in `BNU27`, Allegro, libc sources) |
| `tools/dos.py` | headless DOSBox runner for DJGPP (C: work, D: DJGPP, E: Allegro), output via `redir`, long lines via `@response` files |
| `tools/gxx.py` | compile one .cc/.c and compare each function with AK.COF by mangled name (docs/MATCHING.md) |
| `tools/buildalleg.py` | builds liballeg.a in `build/alleg` as AK.EXE's was built |
| `tools/libmatch.py` | pairs every library object of AK.COF with an archive member by symbol and compares |
| `tools/standin.py` | generates `src/standin/*.s`, the assembly stand-ins of the game objects |
| `tools/build.py` | builds the COFF (C++ where it exists, stand-ins elsewhere), links, compares with AK.COF |
| `tools/listing.py` | readable disassembly of a game object or function, to write the C++ from |
| `src/*.cc`, `src/*.h` | the game in C++ (so far akmenu) |
| `src/standin/*.s` | generated stand-ins for ak.cc, akaux.cc, akchar.cc, akmenu.cc, akworld.cc; each is deleted when its C++ matches |
| `tools/exediff.py`, `fat12.py` | copied from u1; not used yet |
| `docs/PLAN.md`, `docs/MAP.md` | the plan; the map of AK.EXE |
| `docs/MATCHING.md`, `docs/FINDINGS.md` | how gcc 2.7.2.1 -O0 code reads; bugs and mechanics found in the game's code |

Git: `master`, local only, author set in the repository's config as in u1. `tools/u1/`
(Lattice-era tools from u1) and the Lattice/MS C archives in `tools/` are ignored by git.

### gamedata (GOG)

| file | bytes | sha1 |
|---|---|---|
| AK.EXE | 238962 | 756637c80622b84b8167f8bc4192e221ac50187d |
| AK.DAT | 17241 | 7209a15d6fe310bd92cc5c2d2299224456011297 |
| SETUP.EXE | 190043 | ce7882ccace12504e440e4ad4ece67a8ed702c40 |
| SOUND.CFG | 126 | bdeb999839fb2c3267bdd53080cc63512cbe7bc0 |
| DUNGEON.MID | 3488 | 3873a4310626366226b7d7ccb0c30e539aaf1e8b |
| INTRO.MID | 4998 | 65ff091a21665050ab16defd482e4d9611764045 |
| LORDBRIT.MID | 5680 | a270def93f7aedcef5c79fe91f8a71c66bf243cb |
| OVER.MID | 3388 | 3ef74c1a283d6555299e581b464678b34e89bf41 |
| SHOP.MID | 4156 | 5d5a337e603041a8973512db955968bdc79725f9 |
| README.NOW | 2488 | 015d114b405b6b57c4b28dcd212fada18825cfce |
| WHATS.NEW | 2320 | 355a8616d1b7eb25e907c8d17aacd87eab4aa9d9 |

### Toolchain archives (downloaded 2026-11-13, in `tools/`, ignored by git)

| archive | bytes | sha1 | from | what |
|---|---|---|---|---|
| djdev201.zip | 1538295 | ef2129363cdc031c1163543b9dbdf4e763750173 | delorie.com/pub/djgpp/deleted/v2/ | DJGPP 2.01: crt0.o, libc.a, libm.a, headers, stubify |
| djlsr201.zip | 1472671 | 9dd9de039f632093f03ba6ca53059dd3388209b5 | delorie.com/pub/djgpp/deleted/v2/ | DJGPP 2.01 libc sources (reference) |
| bnu27b.zip | 1712024 | 89696fbc9766b32ab5d554a1df591f7093dbbcb0 | delorie.com/pub/djgpp/deleted/v2gnu/ | binutils 2.7 (as, ld, ar, objdump) |
| mak375b.zip | 209185 | 2ba09b8fdbb0af7fe955c4966c6083ad8ff512a0 | delorie.com/pub/djgpp/deleted/v2gnu/ | GNU make 3.75 |
| csdpmi3b.zip | 40719 | 9e8a4487ef1e2e154552e9f4afd4306f8958910d | delorie.com/pub/djgpp/deleted/v2misc/ | CWSDPMI r3 (1996) |
| csdpmi4b.zip | 41506 | 4f56ee401bafa6e8881e96dbd85794a1b08dc690 | delorie.com/pub/djgpp/deleted/v2misc/ | CWSDPMI r4 (Oct 1997) |
| gcc2721b.zip | 1096967 | a52d1dfce480ad64b30cc24a6febc475d3cad792 | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | gcc 2.7.2.1 (cc1, libgcc.a) |
| gpp2721b.zip | 681045 | 6e2b72e9b60fb54aa79177b97c39f490e1cc474f | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | cc1plus 2.7.2.1 |
| lgp271b.zip | 638706 | c13396c901ca3c2ebd8f01a6f751723146fc0296 | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | libg++ 2.7.1 (libgpp.a, libiostr.a, libstdcx.a) |
| alleg22.zip | 1007783 | 56f8c1362a3f2920b584201cc159faa0f80e0162 | liballeg.org/files/ | Allegro 2.2 source (1997-03-01) |
| gcc2721s.zip | 4574996 | 77086f804981a16a429ac29209200a35cc155521 | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | gcc 2.7.2.1 source: read to explain codegen (cp/decl.c, stmt.c, cp/parse.y); not needed to build |
| bnu281b.zip | 1896445 | 7230cb4ab1e3a8a11312fce8c7634e265059cc53 | archive.org item `czchip200001cd` (Czech *Chip* 2000-01 CD), file `ZKUSTE/FREESOFT/BIN/BNU281B/BNU281B.ZIP`, read from the CD image with HTTP range requests | binutils 2.8.1 (files dated 1997-07 to 1998-01): the game's assembler |

gcc 2.7.2.1 is no longer in the DJGPP archive. Its `deleted/v2gnu/gcc2721.txt` points to Martin
Strömberg's site, which still has it.

What was checked:
- Every zip passes its integrity test.
- libc.a's ident is exactly AK.EXE's: `$Id: DJGPP libc built Oct 31 1996 19:13:19 by gcc 2.7.2.1 $`.
- cc1plus says 2.7.2.1. allegro.h says 2.2.
- A quick name-based pairing of archive members with AK.COF's objects (`.text` compared, with
  relocated words masked) found them **byte-identical**: 159 libc.a objects, all 5 libgcc.a
  objects, 21 libiostr.a objects (so the game was linked with `-liostr`, as Allegro's setup is)
  and crt0.o.
- Not yet paired: the libc `*.stub` objects and the `fake` (assembly) math objects, which need
  pairing by symbol rather than by file name.
- Allegro 2.2's makefile builds with `-m486 -O3 -ffast-math -fomit-frame-pointer`, and those
  are the flags AK.EXE's Allegro was built with (phase 2). My earlier doubt came from looking
  at absolute addresses; alignment is relative to each object.

## Phase 2 (done 2026-11-13): the toolchain runs and matches

docs/MATCHING.md has the details. In short:
- `tools/setup.py` unpacks the archives (sha1-checked) into `build/tc`. `tools/dos.py` runs
  DJGPP in headless DOSBox, with output captured through `redir` and long command lines passed
  as `@response` files. hello.cc compiles, links with `-liostr` and runs (about 4.5 s).
- **Allegro 2.2 rebuilds identical** (text.c, math.c, gfx.c: code and data, relocated words
  masked) with gcc 2.7.2.1, the makefile's flags and **binutils 2.7**.
- **The game is `-O0`, assembled with binutils 2.8.1.** `tools/gxx.py` compiles a .cc and
  compares each function with AK.COF. First probes all match: `akmenu::akmenu()`,
  `akmenu::~akmenu()` and `akworld::sgn(int)` (written as `return n = 1;` etc.).
- Binutils 2.8.1 was not in DJGPP's archive any more. A copy came from a 2000 magazine CD on
  archive.org (table above).
- Rebuilding libc from djlsr201 with libc's flags gives libc's code, apart from alignment
  fillers (libc was assembled with 2.7). That doesn't matter: libc links from the archive.

## Conventions

- On Windows, run the tools with plain `python` (no capstone needed) or with
  `../u1-decompiled/build/venv/Scripts/python.exe` when capstone is wanted. That venv's Python
  wants Windows paths (`C:/...`), not Git Bash `/c/...` paths, inside `-c` code.
- Commit early: a session that ended mid-write once left `tools/coff.py` as all zero bytes.
- Never commit gamedata, the toolchain archives, `build/` (the unpacked program, the extracted
  datafile texts and fonts are the game's), or bytes of the wrapper.

## Next steps

To rebuild everything from the archives:

```sh
python tools/setup.py            # build/tc from tools/*.zip (sha1-checked)
python tools/buildalleg.py       # build/alleg/LIB/DJGPP/LIBALLEG.A
python tools/libmatch.py --quiet # all library objects identical
python tools/build.py            # SECTIONS IDENTICAL to the original COFF
```

1. **Phase 4, the game in C++**, one object at a time, smallest first: akmenu.cc, akaux.cc,
   akchar.cc and ak.cc (done), then akworld.cc (74 KB, of which `drawmonsters` is 39 KB). Its
   function return types are still guesses in `src/akworld.h` (ak.cc never uses them).
   - Loop per function: `tools/gxx.py src/akmenu.cc`.
   - When a whole object matches, `tools/build.py` picks up `src/NAME.cc` instead of the
     stand-in, and the sections must stay identical. Delete the stand-in then.
   - The class layouts are in docs/MAP.md; MATCHING.md has the -O0 code patterns.
   - akworld.cc can go to parallel agents by function once the first objects have set the
     conventions (lesson 11 of docs/PLAN.md).
2. Then the symbol table: with all five objects in C++ (ak.cc with `-g`), compare the whole
   COFF, symbols and line numbers included.
3. With ak.cc done, the symbol table differs only by akworld.cc's stand-in symbols (9337 symbols
   against 9382; the line numbers, 316, are already complete).
