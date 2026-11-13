# Handoff: Akalabeth decompilation (u0-decompiled)

State as of 2026-11-13: phases 0 and 1 of docs/PLAN.md are done. Read this first, then
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

## Repository

| file | what |
|---|---|
| `tools/wdosx.py` | WDOSX reader and unpacker (`info`, `unpack`); the LZ format is in its docstring |
| `tools/coff.py` | DJGPP COFF reader: `info`, `files`, `funcs`, `syms`, `types`, `demangle` |
| `tools/akdat.py` | AK.DAT reader: `list`, `extract` (to build/akdat, never committed) |
| `tools/dos.py`, `setup.py`, `exediff.py`, `fat12.py` | copied from u1; to be adapted in phase 2 |
| `docs/PLAN.md`, `docs/MAP.md` | the plan; the map of AK.EXE |

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

## Conventions

- On Windows, run the tools with plain `python` (no capstone needed) or with
  `../u1-decompiled/build/venv/Scripts/python.exe` when capstone is wanted. That venv's Python
  wants Windows paths (`C:/...`), not Git Bash `/c/...` paths, inside `-c` code.
- Commit early: a session that ended mid-write once left `tools/coff.py` as all zero bytes.
- Never commit gamedata, the toolchain archives, `build/` (the unpacked program, the extracted
  datafile texts and fonts are the game's), or bytes of the wrapper.

## Next steps

1. **Phase 2, the toolchain** (needs Karl): the archives listed in docs/PLAN.md section 4.
   DJGPP 2.01 (djdev201), gcc 2.7.2.1 + gpp, binutils 2.7, libgpp 2.7.x, make, CWSDPMI and
   Allegro 2.2 source. Then adapt `tools/setup.py` and `tools/dos.py` to run gcc in headless
   DOSBox with CWSDPMI and `DJGPP=...\djgpp.env`, and smoke-test hello.cc.
2. **Phase 3**: `tools/libmatch.py` for COFF (compare each library symbol's bytes in AK.COF with
   the archive members, by name, masking addresses). DJGPP's libc.a and libgpp should match as
   shipped. Build Allegro 2.2 and match it: that proves gcc's version and flags.
3. Then `tools/gxx.py` (compile one .cc in DOSBox, compare each function by mangled name) and
   start on akmenu.cc (2 KB, 5 functions), the smallest game object, to set up the loop.
4. Optional, any time: a disassembly of `rungame` beside its line table to see how the source's
   statements map to code (`coff.py` has the lines; a small `tools/lines.py` would print them).
