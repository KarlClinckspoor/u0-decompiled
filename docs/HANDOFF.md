# Handoff: Akalabeth decompilation (u0-decompiled)

State as of 2026-11-13. Read this first, then docs/MATCHING.md (how to change the source
without breaking the match), docs/MAP.md (what is in AK.EXE) and docs/FINDINGS.md (what the
code revealed). docs/PLAN.md is the original plan, phases 0-8.

## Where it stands

**AK.EXE rebuilds identical to the GOG file, byte for byte** (238962 bytes, sha1
`756637c80622b84b8167f8bc4192e221ac50187d`), from:
- the game in C++ (`src/`, five files, 36 functions);
- Allegro 2.2, rebuilt from its source as the game's copy was built;
- DJGPP 2.01's libc, libgcc, libg++ 2.7.1's libiostr and crt0, linked as shipped;
- the WDOSX 0.97 kernel and stub, copied from gamedata at build time.

The program inside (AK.COF) is identical down to its 9382 symbols, ak.cc's debug
information and the 316 line numbers. The compiler is gcc 2.7.2.1 for DJGPP at `-O0`
(ak.cc with `-g`), assembled with binutils 2.8.1, in headless DOSBox.

| phase (docs/PLAN.md) | state |
|---|---|
| 0-1 survey, unpack | done: `tools/wdosx.py` unpacks AK.EXE; the COFF is not stripped |
| 2 toolchain | done: gcc 2.7.2.1 + binutils 2.8.1 (game), binutils 2.7 (Allegro) |
| 3 libraries, link | done: all 257 library objects identical, the link reproduces the layout |
| 4 the game in C++ | done: akmenu, akaux, akchar, ak (with `-g`), akworld all match, symbols included |
| 7 the shipped file | done: `tools/postlink.py` with a Python port of WDOSX's packer |
| 5 readable pass | **open** |
| 6 mechanics, findings checked in play | **open** (findings so far are read from the code only) |
| 8 port | optional, not started |

SETUP.EXE (Allegro's stock sound setup, same wrapper) is **not a target**: Karl decided it
isn't needed.

## Rebuilding

Prerequisites (none of them is in git):
- `gamedata/`: the GOG files (table below). `tools/dos.py` uses `$DOSBOX`, else `dosbox` on
  the PATH, else GOG's `gamedata/DOSBOX/DOSBox.exe`.
- `tools/*.zip`: the toolchain archives (table below), sha1-checked by `tools/setup.py`.
- Python 3. capstone is needed by the disassembly tools (`gxx.py`'s first-difference window,
  `listing.py`, `pseudo.py`); use `../u1-decompiled/build/venv/Scripts/python.exe`. That venv's
  Python wants Windows paths (`C:/...`), not Git Bash `/c/...` paths, inside `-c` code.

```sh
python tools/wdosx.py unpack gamedata/AK.EXE   # build/AK.COF, the reference for every compare
python tools/setup.py                          # build/tc from tools/*.zip
python tools/buildalleg.py                     # build/alleg/LIB/DJGPP/LIBALLEG.A (~45 s)
python tools/libmatch.py --quiet               # optional: all library objects identical
python tools/build.py                          # compile, link, wrap, compare (~20 s)
```

`tools/build.py` should end with:

```
  whole file: IDENTICAL
SECTIONS IDENTICAL to the original COFF
AK.EXE IDENTICAL to the shipped file (238962 bytes, sha1 756637c80622b84b8167f8bc4192e221ac50187d)
```

This is the gate for every change. For one file at a time:
`python tools/gxx.py src/akworld.cc` (or `src/ak.cc --flags="-O0 -g"`). It compares every
function, every symbol, and for ak.cc every debug symbol and line number.

### What the build does

1. Compiles each `src/NAME.cc` in DOSBox (`gcc -O0 -IE:/ -c`, ak.cc `-O0 -g`, binutils 2.8.1).
2. Links like gcc 2.7.2.1:
   `ld -o ak crt0.o ak.o akaux.o akchar.o akmenu.o akworld.o -lalleg -liostr -Tdjgpp.djl -lgcc -lc -lgcc`.
3. Compares `build/link/AK` with `build/AK.COF`: headers, sections, symbol table, whole file.
4. Wraps it (`tools/postlink.py`):
   - the bytes of gamedata/AK.EXE before its WFSE entry (WDOSX kernel and `go32stub v2.00T`);
   - then a WFSE entry `WdosxMain` holding the COFF in 4096-byte chunks, each packed with
     WDOSX-Pack 1.07 (`tools/wpack.py`).
   It then compares the result with gamedata/AK.EXE.

## Repository

| path | what |
|---|---|
| `src/ak.cc` | the game loops: `rungame` (world map), `rundungeon` (dungeon), `main` (menu, game loop). Compiled `-g`: **its line layout is fixed** (see below) |
| `src/akworld.cc`, `akworld.h` | the world map, dungeon generator, 3-D view, monster pictures, combat, monster moves; the perspective tables of the constructor |
| `src/akchar.cc`, `akchar.h` | character creation, the shop, save/load (PLAYER.U0), Lord British, upkeep, death |
| `src/akmenu.cc`, `akmenu.h` | the main menu |
| `src/akaux.cc`, `akaux.h`, `ak.h` | intro, instructions, quotes, credits; `ak.h` holds AK.DAT's object indexes |
| `tools/build.py` | the whole build and every comparison (above) |
| `tools/gxx.py` | compile one file in DOSBox and compare it function by function, symbols and debug info included |
| `tools/postlink.py`, `tools/wpack.py` | phase 7: the WFSE entry, as WDOSX 0.97's `addfile.asm` writes it; WDOSX-Pack 1.07 ported from `wpack.c` |
| `tools/wdosx.py` | WDOSX reader and unpacker (`info`, `unpack`); the WFSE and LZ formats are in its docstring |
| `tools/coff.py` | DJGPP COFF reader: `info`, `files`, `funcs`, `syms`, `types`, gcc 2.x demangler |
| `tools/listing.py` | annotated disassembly of a game object or function: labels, strings, doubles, switch tables, source lines (ak.cc) |
| `tools/pseudo.py` | the -O0 code folded into C-like pseudo code (registers, spill slots and x87 stack become expressions; members and array indexes named). `PSEUDO_TEMPS=94,90` forces frame slots to be temporaries |
| `tools/setup.py`, `tools/dos.py` | the toolchain in `build/tc`; the headless DOSBox runner (long lines via `@response` files) |
| `tools/buildalleg.py`, `tools/libmatch.py` | Allegro 2.2's build; library objects paired by symbol and compared |
| `tools/akdat.py` | AK.DAT reader (`list`, `extract` to `build/akdat`) |
| `tools/standin.py` | phase 3's generator of assembly stand-ins; `build.py` still falls back to `src/standin/NAME.s` when a `src/NAME.cc` is missing |
| `tools/exediff.py`, `fat12.py` | copied from u1, unused |
| `docs/` | PLAN, MAP, MATCHING, FINDINGS, this file |

Git: branch `master`, pushed to github.com/KarlClinckspoor/u0-decompiled (each push so far was
on Karl's word). Ignored: `gamedata/`, `build/`, the archives in `tools/`, `tools/u1/`.

## Changing the source without breaking the match

Phase 5 means editing matched code, so these rules matter. Each one was learned the hard way;
docs/MATCHING.md has the details and evidence.
- **Build after every change** (`tools/build.py`, or `tools/gxx.py` for one file). Only the
  compiler decides.
- **ak.cc's line numbers are in the binary.** A statement may not move to another line. Comments
  and blank lines can change only where they don't shift a statement: edit within a line, or
  swap a comment line for a blank line. Its types (the headers' classes) are in the debug
  information too, member names and array dimensions included.
- **Label numbers are in the symbol table**, numbered across each file, headers included.
  Adding or removing a `goto` label, a `case`, an `if`, a loop or an inline-function call
  changes them. Changing the include order changes them. Unused labels (`done:` in
  `drawdungeon`, `stay:` in `movemonsters`) must stay.
- **Function order matters beyond each function.** Dead jumps before an epilogue depend on a gcc
  global that earlier functions set (MATCHING.md, "Dead jumps").
- **Names are mostly free, code shape is not.**
  - Free: the names of locals, parameters and user labels in the four files without `-g`
    (gcc numbers its labels `Ln` whatever the source calls them).
  - Not free:
    - anything ak.cc's debug information records: its locals' names, and the classes in
      `akaux.h`, `akchar.h`, `akmenu.h` and `akworld.h`, member names and order included;
    - method and class names, which are in the mangled symbols;
    - declaration order, which gives the frame slots (unused locals keep theirs).
  - In the code itself: operand order (`a == b` vs `b == a`), how sums are grouped
    (`a - (b + 3)`), casts (`(int)floor(...)`), `&`/`|` instead of `&&`/`||`, and empty
    statements (`if (...) ; else`).
- **Named constants**: `#define` is free as long as value and type stay the same. A new
  `enum` or type in a header that ak.cc includes would add debug symbols and break the match.
  Put such types in a separate header that ak.cc does not include.

## Open work

1. **Phase 5, readable pass.**
   - akworld.cc has no debug info, so its locals' and parameters' names are guesses (mostly the
     Apple BASIC's: `x`, `w`, `cent`, `l1`, `mn`, `dam`...).
   - Map values (world: 1 mountains, 2 tree, 3 town, 4 dungeon, 5 castle; dungeon: 1 wall,
     2 trap, 3 hidden door, 4 door, 5 chest, 7/9 ladder down, 8 ladder up, monster * 10), the
     key codes and the colours could become named constants.
   - Every function has a comment; some say "probably". The Apple II Applesoft listing would
     confirm the names and the meaning of variables (Corey Roth ported it closely).
2. **Phase 6, mechanics.**
   - Write docs/game/ from docs/FINDINGS.md (world and dungeon generation, combat, the amulet,
     quests, food, the save file, AK.DAT texts).
   - Check the findings in play: GOG's DOSBox with AK.EXE. Mark each "checked in play" or
     "read only". The bugs (towns never thinned, uninitialised `itoa` buffers, the mistyped
     range check, mages buying rapiers and bows) are good first checks.
   - Compare with the Apple II original where it differs (random numbers, the generators,
     rounding).
3. Optional: build the WDOSX kernel and stub from WDOSX's sources (`kernel/wdosx.asm`,
   `coff/stubcf.asm`; needs TASM) instead of copying them. They carry the 1996-2002 copyright,
   and the archived sources are the 2005 final, so they may differ.
4. Optional, phase 8: a port against a modern Allegro or SDL shim.

## Facts worth knowing

- The COFF is not stripped: 268 objects in link order, every name, every local label, and ak.cc's
  `-g` information (class layouts, locals). The game has no `.data` or `.bss` of its own;
  literals are in `.text` before each function.
- Member names are Garriott's Applesoft variables (TX, TY, PX, PY, DX, DY, INOUT, TASK, LK,
  PER, CD, LAD, LD, FT, XX, YY, MZ, ML).
- `akworld` inherits `charsave`, and its own `sav` is never used except by `viewintro`, which
  nothing calls. The player's record is always passed in as a `savestruct &`.
- AK.DAT: an Allegro 2.2 packfile (`slh!` LZSS) around a datafile of 7 objects: instructions,
  three Lord British speeches, two fonts, the grabber's info. Built in `c:\djgpp\bin`,
  13-20 November 1997.
- The WDOSX format, read from WDOSX 0.97's sources: WFSE flags 0x26 = stub class 6 (DJGPP COFF)
  | 0x20 (WDOSX-Pack); the name field is `WdosxMain\0`, word 6, and the last chunk's packed size;
  a chunk that does not shrink is stored raw.

## Conventions

- Never commit gamedata, the toolchain archives, `build/`, the extracted AK.DAT contents or
  WDOSX's bytes.
- Downloads and pushes only with Karl's OK.
- Never kill DOSBox processes wholesale: other sessions may run their own.
- Commit early: a session that ended mid-write once left `tools/coff.py` as all zero bytes.
- Keep the user's email out of anything sent outside.

## Reference

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

`build/AK.COF` (unpacked by `tools/wdosx.py`): 570462 bytes, sha1
3badf300cef0032f57b12468b51b448c3926bcab.

### Toolchain archives (in `tools/`, ignored by git)

| archive | bytes | sha1 | from | what |
|---|---|---|---|---|
| djdev201.zip | 1538295 | ef2129363cdc031c1163543b9dbdf4e763750173 | delorie.com/pub/djgpp/deleted/v2/ | DJGPP 2.01: crt0.o, libc.a, libm.a, headers, stubify |
| djlsr201.zip | 1472671 | 9dd9de039f632093f03ba6ca53059dd3388209b5 | delorie.com/pub/djgpp/deleted/v2/ | DJGPP 2.01 libc sources (reference) |
| bnu27b.zip | 1712024 | 89696fbc9766b32ab5d554a1df591f7093dbbcb0 | delorie.com/pub/djgpp/deleted/v2gnu/ | binutils 2.7 (Allegro's assembler) |
| bnu281b.zip | 1896445 | 7230cb4ab1e3a8a11312fce8c7634e265059cc53 | archive.org item `czchip200001cd` (Czech *Chip* 2000-01 CD), `ZKUSTE/FREESOFT/BIN/BNU281B/BNU281B.ZIP`, read from the CD image with HTTP range requests | binutils 2.8.1 (the game's assembler) |
| mak375b.zip | 209185 | 2ba09b8fdbb0af7fe955c4966c6083ad8ff512a0 | delorie.com/pub/djgpp/deleted/v2gnu/ | GNU make 3.75 |
| csdpmi3b.zip | 40719 | 9e8a4487ef1e2e154552e9f4afd4306f8958910d | delorie.com/pub/djgpp/deleted/v2misc/ | CWSDPMI r3 |
| csdpmi4b.zip | 41506 | 4f56ee401bafa6e8881e96dbd85794a1b08dc690 | delorie.com/pub/djgpp/deleted/v2misc/ | CWSDPMI r4 |
| gcc2721b.zip | 1096967 | a52d1dfce480ad64b30cc24a6febc475d3cad792 | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | gcc 2.7.2.1 (cc1, libgcc.a) |
| gpp2721b.zip | 681045 | 6e2b72e9b60fb54aa79177b97c39f490e1cc474f | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | cc1plus 2.7.2.1 |
| lgp271b.zip | 638706 | c13396c901ca3c2ebd8f01a6f751723146fc0296 | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | libg++ 2.7.1 (libiostr.a) |
| alleg22.zip | 1007783 | 56f8c1362a3f2920b584201cc159faa0f80e0162 | liballeg.org/files/ | Allegro 2.2 source (1997-03-01) |
| gcc2721s.zip | 4574996 | 77086f804981a16a429ac29209200a35cc155521 | www.ludd.luth.se/~ams/djgpp/old/v2gnu/ | gcc 2.7.2.1 source: read to explain codegen (`build/gccsrc/` has the files used); not needed to build |

WDOSX 0.97 Final's sources: github.com/amindlost/wdosx, cloned to `build/dl/wdosx` (ignored).
Only read, for `src/wpack/wpack.c`, `src/util/stubit.c` and `src/util/addfile.asm`; the build
does not need them.
