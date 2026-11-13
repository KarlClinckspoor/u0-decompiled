# Plan: Akalabeth decompilation (u0-decompiled)

Written 2026-11-13. **Status: phases 0 and 1 done (2026-11-13). AK.EXE is unpacked to
`build/AK.COF` without WDOSX's source, and the COFF is not stripped: link order, every name, and
`-g` information for ak.cc (docs/MAP.md). The game is 5 objects, 36 functions and 100 KB of
`.text`. Phase 2 done the same day: gcc 2.7.2.1 runs headless in DOSBox, Allegro objects rebuild
identical, and the game turns out to be `-O0` assembled with binutils 2.8.1 (docs/MATCHING.md).
Phase 3 done too: all 257 library objects are identical, and with assembly stand-ins for the
game the link reproduces the original's headers, `.text` and `.data`. Next is phase 4, the game
in C++. docs/HANDOFF.md has the results.** This plan is based on the sister projects
`../u1-decompiled`, `../u2-decompiled` and `../u3-decompiled` (their README, docs/HANDOFF.md,
FINDINGS.md, MATCHING.md and U3's PLAN.md), on `../u6md-decompiled/U6-LESSONS_DECOMPILATION.md`,
and on a first look at the GOG files in `gamedata/`. The goal is the same as theirs: source that
period tools rebuild into the shipped file byte for byte, then readable names, then the mechanics
written down, and maybe a port.

**The main point:** Akalabeth is nothing like Ultima I-III. It is not 16-bit Lattice C or
hand-written 8086 assembly. It is a **1997/98 32-bit protected-mode C++ program** (DJGPP 2.01,
gcc 2.7.2.x, the Allegro 2.2 library) by Corey Roth, a port of Richard Garriott's 1979 Applesoft
BASIC game. It is packed inside a **WDOSX 0.97** wrapper. The sisters' methods carry over. Their
tools and toolchain mostly do not.

## 1. Lessons carried over from U1, U2, U3 (and U6)

| # | lesson | from | how it applies to Akalabeth |
|---|---|---|---|
| 1 | The only gate is a whole-file sha1 match, checked after every change. Only the real compiler decides whether C is right. | all | Same. But there are two files to match: the COFF program inside the wrapper (the real build) and AK.EXE as GOG ships it (§2.1). |
| 2 | Find out what the code is before choosing a method. U1 was compiled C, so its functions were matched in C. U2 and U3 were assembly, so the work was recovering symbols. | U1-U3 | Compiled C++ with a symbol table, so this is the **U1 path**: match functions in C++. It is easier than U1 in two ways: the names are known, and the libraries are public. |
| 3 | Build the compile-and-compare loop before writing any C: a one-command compile in headless DOSBox, a one-function compare that names the first instruction that really differs (fixups masked, fixup targets checked), and a full build that names the first differing byte. | U1 `lc.py`, U6 | Rebuild this for DJGPP COFF objects: a `gxx.py` like `lc.py`. Doing it first is the biggest single saving. |
| 4 | Find the library code before cutting the program into modules. Matching existing objects separates the game's own code from library code that already exists. | U1 `libmatch.py`, U6 §10 | Even more true here. The game links libc (DJGPP 2.01), libgpp/libio and Allegro 2.2, and all three exist as public binaries or source. If they rebuild identical, only the `ak*.cc` files are left to write. |
| 5 | The link order is the layout. Each EXE has its own order of library modules. | U1, U6 | The COFF symbol table lists every object's `.file` entry in link order, so the order is read off the file, not guessed. |
| 6 | Keep the original address on everything (`/*C_SSSS_OOOO*/`, `;@addr`). Rename only with a checked tool. | U2, U3, U6 | Keep each function's original virtual address in a comment. The symbol table already gives real (mangled) names. |
| 7 | Tools must not lie. When a difference looks impossible, check the tool first. "0 problems" means only what the tool checks. | U6 §6 | Expect C++ specific false alarms: vtables, `__GLOBAL_$I$`/`$D$` constructors, thunks, string literals placed in `.text`. |
| 8 | A copy-protection or loader layer added after the link gets a post-link step. The step copies the layer's remains from gamedata at build time, and those bytes are never committed. | U1 ULTIMA.EXE, U2 `postlink.py` | WDOSX is that kind of layer, added after the link (§2.1). Reproduce it with WDOSX's own `stubit`, or fall back to a post-link step. |
| 9 | Releases can be patched. U2's 1989 EXE has dead 1983 code; in U3 the disk check was stubbed out. Keep what you find and document it. | U2, U3 | The WDOSX wrapper is dated 2002, after the game, so the GOG file is a re-wrap of Origin's file. An Ultima IX / Ultima Collection CD copy would show the original stub (§4). |
| 10 | DOSBox: never kill every DOSBox process (other sessions run their own). Run commands from a batch file (0.74's autoexec overflows at about 4 KB). A command that waits for input hangs forever, so guard it. Send real key presses when playing. | U1, U3 | All still true. Also new: DJGPP needs a DPMI host (CWSDPMI.EXE on the path) and the `DJGPP` environment variable. DOSBox 0.74 has no long file names, so keep everything 8.3. |
| 11 | Parallel agents work when each gets a disjoint unit, a written brief, and its own work files, and one integrator merges. Small units go first, to check the brief. Agents may not change shared tools, but they report tool bugs precisely. | U1 `AGENT_BRIEF.md`, U6 §7 | The natural unit is one `ak*.cc` object (or one class). Only start agents after the toolchain is proven (phase 3). |
| 12 | Repository hygiene: never commit gamedata, tool archives or wrapper bytes. Follow U1's git author and trailer conventions. Never name a tool `dis.py` (it shadows Python's `dis` and breaks capstone). Use the u1 venv for capstone. | all | Same. There is no MASM CRLF issue unless assembly turns up. |
| 13 | Readable names and comments may not change the bytes. Codegen depends only on the token stream, so a token-stream comparison proves a rename safe. Bugs go into FINDINGS.md, never into the code. | U6 §8, U2/U3 | The same rule applies to the C++. Proposed names wait until the build is identical. |
| 14 | Write down what the code reveals (bugs, mechanics, dead code), and mark each item as checked in play or read only. | U2, U3 FINDINGS | Here there is also a reference to compare with: the Apple II original. Where the port differs from Garriott's BASIC is a finding in its own right. |

What does **not** carry over: Lattice/MSC/MASM matching rules, the 8-bit-branch module cuts,
`genasm.py`/`gen.py`/`regen.py`, `xmap.py`/`addrimm.py` (in 32-bit code with a symbol table,
addresses are relocations, not guesses), BCD and 6502 idioms, and FCB/CGA details. The copies of
U1's tools and archives already in `tools/` are mostly for 16-bit Lattice work. Keep `dos.py`,
`fat12.py`, `setup.py` (as a pattern) and `exediff.py` (as a pattern). The Lattice and MS C
archives are not needed.

## 2. What Akalabeth is (survey of the GOG files)

### 2.1 AK.EXE: three layers

| file offset | what |
|---|---|
| 0000-25F8 | **WDOSX 0.97 DOS extender** kernel (MZ, 9720 bytes; "Copyright (c) 1996-2002 Michael Tippach", marker `TIPPACH`/`$WdX`) |
| 25F8-2A21 | WDOSX's version of the DJGPP stub (`go32stub, v 2.00T`, inner MZ of 1065 bytes) |
| 2A21-end | **WFSE** container (WDOSX's embedded file system): one file `WdosxMain`, header `WFSE` + 0x00037B51 (228177 bytes packed: exactly the rest of the file) + 0x0008B45E (570462 bytes, probably the unpacked size), then a table of chunk offsets about 0x800 apart. The data has high entropy (7.8 bits/byte), so it is compressed. |

- AK.EXE is 238962 bytes, sha1 `756637c80622b84b8167f8bc4192e221ac50187d`.
- The packed `WdosxMain` is the **DJGPP COFF program** (the stub's messages say ".coff").
  Unpacked it would be about 557 KB. That is far more than code and data need, which suggests a
  full symbol table and maybe `-g` debug information (still to confirm).
- **The wrapper is not Origin's.** The 2002 date is years after the game. The README.NOW says
  "CWSDPMI (provided) must be present", which means a normal `stubify` stub. So GOG (or a
  re-release) probably re-stubbed the program with WDOSX `stubit` so that it no longer needs
  CWSDPMI. SETUP.EXE has the same WDOSX header.

### 2.2 Toolchain evidence

Read from fragments that survive the compression (to be confirmed after unpacking):

- `$Id: DJGPP libc built Oct 31 1996` means **DJGPP 2.01** (djdev201).
- `gcc2_compiled.` and `__gnu_compiled_cplusplus` mean gcc, compiling C++. In the DJGPP 2.01 era
  that is **gcc 2.7.2.1** (to be confirmed; 2.7.2 is the other candidate).
- `Allegro 2.2, by Shawn Hargreaves, 1997`, and Allegro object names (`mouse.c`, `_keyboard_hooks`,
  `allegro_errno`, `pack_igetl`).
- libio objects (`iounget.c`, `iostream.cc`?) mean **libgpp/libstdc++** for gcc 2.7.
- Game objects in the symbol table: `akmenu.cc`, `akworld.cc`, ... (more after unpacking).
  WHATS.NEW: "Reorganized code into several objects", so expect C++ classes.
- WHATS.NEW also says Revision 1 was Borland Turbo C++ 3.0 (not here, not a target).
- Version: README.NOW says "Rev 2 v.0910", WHATS.NEW says "v0.955". The title screen will say
  which one this is.

### 2.3 Data and other files

| file | bytes | what |
|---|---|---|
| AK.DAT | 17241 | **Allegro datafile**, LZSS-packed (`slh!`, then `ALL.`), made with the grabber on 11-15-1997. Probably fonts, pictures and palette. Easy to read in Python (Allegro 2.2's `packfile.c` is the reference). |
| SETUP.EXE | 190043 | Allegro's stock sound-setup program (`setup/setup.c` in Allegro 2.2), same WDOSX wrapper. A cheap bonus target once Allegro rebuilds. |
| SOUND.CFG | 126 | Allegro sound configuration (written by SETUP) |
| INTRO/SHOP/OVER/DUNGEON/LORDBRIT.MID | 3-6 KB | background music, from earlier Ultimas (WHATS.NEW) |
| QUOTES.FLG, ENDGAME.FLG | (not shipped) | zero-byte files that unlock the menu's "View Quotes" and "View Ending" (WHATS.NEW) |
| Akalabeth-Manual.pdf | | reference for names and mechanics |

The game saves with Q and restores with ^R (README.NOW). The save format is to be decoded in
phase 6.

## 3. Phases

Each phase ends with a commit, docs/HANDOFF.md updated, and the gates of the phases before it
still passing.

**Phase 0: repository and inventory.** `git init` (ask Karl about the branch name; the sisters use
`master`). Add `.gitignore` (gamedata/, build/, tools/*.7z, tools/*.zip, wrapper bytes),
README.md, docs/HANDOFF.md. Clear out or set aside the Lattice tools in `tools/`, keeping
`dos.py`, `fat12.py` and the pattern of `setup.py`. Write down the sha1 of every gamedata file.

**Phase 1: unpack (done).** Result: the decompressor is the one the kernel uses on itself (read
from its 16-bit code), so WDOSX's source was not needed. `tools/wdosx.py`, `tools/coff.py` and
`tools/akdat.py` exist; docs/MAP.md has the outputs. The original text of the phase follows.
Write `tools/wdosx.py`, which reads the WDOSX/WFSE layers and unpacks
`WdosxMain` into `build/AK.COF`. Work out the decompressor from WDOSX 0.97's published source
(its packer is Tippach's own). The fallback, if the source cannot be had, is to run AK.EXE in a
DOSBox debugger build and dump the program memory after load. That dump is weaker: it gives the
loaded image, not the file. Then write `tools/coff.py`, a DJGPP COFF reader for headers,
`.text`/`.data`/`.bss`, the symbol table with aux entries, line numbers and relocations (if any
survive; a linked COFF usually has none), and demangling of gcc 2.7 C++ names. Outputs:
- `docs/MAP.md`: every object in link order with its address range, and which objects are game,
  libgpp, libc, Allegro or startup (crt0)
- whether `-g` information is there (`.bf`/`.ef`, line numbers, locals, types)
- the game's code size, which sets the scale of phase 4

Do the same for SETUP.EXE and AK.DAT (`tools/akdat.py`: list and extract the objects).
Gate: the unpacker reproduces the exact packed bytes when re-packed (phase 7) or, until then,
the result is a well-formed COFF whose sizes match the WFSE header.

**Phase 2: toolchain.** Gather the period tools (§4) and make them run headless in DOSBox:
`tools/setup.py` unpacks them into `build/tc`, and `tools/dos.py` runs `gcc`/`gxx`/`ld` from a
batch file with CWSDPMI and `DJGPP=...\djgpp.env`. Smoke test: compile and link hello.cc. Then
read the compiler settings off the game code: frame pointer or not (`-fomit-frame-pointer`),
function alignment and its padding bytes (`-m486` gives 16-byte alignment with `lea esi,[esi]`
style fillers; plain `-O` gives 4), `-O`/`-O2`/`-O3` habits (inlining, strength reduction, loop
shapes), and where string literals go (gcc 2.7 for DJGPP puts them in `.text`). Settle 2.7.2 vs
2.7.2.1 by compiling a library file whose bytes are known (phase 3).

**Phase 3: libraries identical.** First check the libraries with `tools/libmatch.py` (rewritten
for COFF). It compares every library symbol's bytes in AK.COF with the same member of `libc.a`,
`libgpp.a`/`libiostream.a` and a rebuilt `liballeg.a`, with relocations masked and targets
checked. Allegro 2.2 ships as source: build it once in DOSBox with its own makefile flags and the
gcc chosen in phase 2. **An Allegro that matches proves the compiler and its flags before any
game code is written.** Then link a stand-in program: the real crt0, libraries and link order
from phase 1, with the game objects replaced by generated assembly stand-ins (gas syntax, from
`tools/genasm.py` rewritten for 32-bit). Gate: `build.py` links a COFF identical to AK.COF (with
the COFF timestamp field and symbol table handled; see risks).

**Phase 4: the game in C++.** One `src/ak*.cc` per original object, in link order. The loop per
function: `python tools/gxx.py src/akworld.cc` compiles in DOSBox, then compares each function
with the original by mangled name. It reports the first instruction that really differs, then
checks fixup targets, then data and literals. The mangled names give each method's class,
parameter types and constness, so the class declarations (`src/ak.h`) can be written before the
bodies. C++ points to watch: vtable and type-info placement (gcc 2.7 emits the vtable with the
first non-inline virtual function), inline functions emitted out of line, global constructors
and destructors (`__GLOBAL_$I$file`), and the order of statics and literals. Start with the
smallest object to check the brief. Then run parallel agents, one object each, with an
AGENT_BRIEF.md adapted from U1's and the lessons collected in docs/MATCHING.md as they are
learned. Gate: every game function matches, and `build.py` links an identical AK.COF from C++
with no stand-ins left.

**Phase 5: readable pass.** Most names come free from the symbol table (classes, methods,
globals, statics, and maybe locals with `-g`). What remains: locals and parameters (if no `-g`),
magic numbers turned into named constants and enums, struct members, and comments on every
function, all checked by the rebuild or a token-stream check (lesson 13). Use the manual for
names, and the Applesoft original for the meaning of variables (Corey Roth ported it closely).

**Phase 6: mechanics and findings.** Write docs/game/ (as U1 did): character creation and the
difficulty levels 1-10, shops and prices, the overworld, the dungeon generator and its 3D view,
monsters and combat, the magic amulet, Lord British's quests, food, save files, the end game, and
the QUOTES/ENDGAME flags. Write docs/FINDINGS.md: bugs and oddities, each marked "checked in play"
or "read only", plus every place where the port differs from the Apple II original (RNG, the
dungeon generator, rounding, the order of turns). Write docs/FORMATS.md (AK.DAT, the save file,
SOUND.CFG), with `tools/akdat.py` rendering the pictures to build/ (never committed).

**Phase 7: the shipped file.** Rebuild AK.EXE itself. First try WDOSX 0.97's own `stubit` on our
COFF in DOSBox. If its output is identical (same kernel, same compression), the build is complete
with period tools. If not, write `tools/postlink.py` in the U1/U2 style: it takes the kernel and
stub bytes from gamedata at build time and re-packs our COFF with a Python port of the packer, and
the bytes copied are never committed. Gate: AK.EXE sha1 `756637c8...`. Optional: SETUP.EXE the
same way (Allegro's setup.c plus its link). Optional, if Karl has an Ultima IX or Ultima
Collection CD: compare with Origin's original stubified AK.EXE and add a "as Origin shipped it"
target.

**Phase 8 (optional): a port.** The game already runs on top of Allegro, so a port means building
the C++ against a modern Allegro or SDL shim rather than replacing DOS details. Delay loops and
timer-driven code are the places to look first. Check behaviour against recorded DOSBox
sessions (Exhume's `runtime/port` + `replay`, in `../underworld-exhumed-enhanced/exhume/`).

## 4. Tools to acquire

**Done 2026-11-13** (Karl said to download them). The archives are in `tools/`, and
docs/HANDOFF.md lists the exact files, sources and sha1s. In short: djdev201, djlsr201, bnu27b,
mak375b, csdpmi3b/4b, gcc2721b, gpp2721b, lgp271b, alleg22. The libraries match AK.EXE byte for
byte. WDOSX 0.97 is not downloaded yet; it is only needed in phase 7. The original table:

| what | where it was distributed | why |
|---|---|---|
| DJGPP 2.01 development kit (djdev201) | DJGPP archive, `v2/` (old releases under `deleted/` or `oldrel/`) | crt0, libc.a, headers, stubify, djgpp.env |
| gcc 2.7.2.1 for DJGPP (gcc2721b), C++ compiler (gpp2721b) | `v2gnu/` | the compiler |
| libgpp 2.7.x (lgpp27xb) | `v2gnu/` | iostream/libio and the C++ runtime |
| binutils 2.7 (bnu27b) | `v2gnu/` | as, ld, ar, objdump |
| make (mak37xb) | `v2gnu/` | to build Allegro |
| CWSDPMI (csdpmi3b or 4b) | `v2misc/` | DPMI host in DOSBox |
| Allegro 2.2 source (alleg22) | Allegro's old-release archive | rebuild liballeg.a, SETUP.EXE |
| WDOSX 0.97 | Michael Tippach's site (archived) | `stubit` (phase 7); no longer needed for unpacking |
| (optional) gcc 2.7.2, binutils 2.6/2.8 | `v2gnu/` | if 2.7.2.1 does not match |
| (optional) the Applesoft listing of Akalabeth | published widely | reference for names and findings, not copied into the repo |

## 5. Open questions and risks

- **Exact compiler version.** gcc 2.7.2 or 2.7.2.1, and the binutils version. Allegro (phase 3)
  answers this. If neither matches, a DJGPP 2.00/2.02 era compiler is the next candidate.
- **The COFF header and symbol table.** A whole-COFF match also needs the header timestamp (GNU ld
  may write the link time), the symbol table and string table, and, with `-g`, line numbers.
  Line numbers tie each function to its line in Corey Roth's source. They are a constraint (the
  source must keep the same line layout) but also a strong guide (they tell where every statement
  was). Decide in phase 1 what the gate covers. The sections come first, the symbol table after.
- **Speed in DOSBox.** cc1plus under DOSBox may take seconds per compile. Use cycles=max and the
  dynamic core. If it is too slow, try DOSBox-X, or a cross gcc 2.7.2 (hard to build today).
- **Optimizer sensitivity.** gcc 2.7's register allocation responds to small changes in the
  source. U1's MATCHING.md style of notes (statement order, temporaries, `register`, block-local
  variables) will be needed. Write them down as they are found.
- **Allegro flags.** Corey Roth may have built Allegro with flags other than the defaults, or
  patched it. libmatch will show which modules differ.
- **Scale (settled in phase 1).** 36 game functions, 100 KB of `.text` including literals. Two
  functions dominate: `drawmonsters` (39 KB) and `drawdungeon` (14 KB), probably long runs of
  similar drawing calls. Smaller than U1's OUT.EXE in function count, larger in bytes.
- **The packer (settled in phase 1).** The decoder is understood. For phase 7, WDOSX's packer
  must still give the same packed bytes, and the junk after the WFSE name (docs/HANDOFF.md) must be
  reproduced or copied.
- **ak.cc's line numbers (found in phase 1).** A whole-COFF match includes ak.cc's 316 line
  numbers, so its source must keep the original line layout. The other four objects have none.

## 6. First session checklist

1. Phase 0: repository, .gitignore, README.md, HANDOFF.md skeleton, sha1 list. Ask Karl about the
   branch name and about getting the archives in §4.
2. Phase 1: get WDOSX 0.97 (with Karl's OK), write `tools/wdosx.py`, unpack `WdosxMain`, write
   `tools/coff.py`, produce docs/MAP.md, and answer: is `-g` there, how big is the game code,
   which objects, and in what link order?
3. Check that AK.DAT reads with Allegro's LZSS and list its objects.
4. Update this plan with what phase 1 found (as U3's PLAN.md was updated), especially the size of
   phase 4 and the gate for the COFF symbol table.
