# u0-decompiled

Work towards source code for the PC port of **Akalabeth: World of Doom** (Richard Garriott 1979;
MS-DOS port by Corey Roth with Nathan Tucker, 1997/98, made for the Ultima IX vault), meant to
rebuild with the original tools into files identical to the GOG release, byte for byte. The
sister projects `../u1-decompiled`, `../u2-decompiled` and `../u3-decompiled` are the models.

## Status

| file | what | status |
|---|---|---|
| AK.EXE | the game: a DJGPP 2.01 / gcc 2.7.2.1 C++ program with Allegro 2.2, inside a WDOSX 0.97 wrapper | **links with identical code and data** (phase 3): every library object rebuilt or shipped identical, the game in C++ under way: akmenu.cc and akaux.cc match (code and symbols), the other three objects are still generated assembly stand-ins (`src/standin/`) |
| AK.DAT | Allegro datafile: instruction text, Lord British's speeches, two fonts | read by `tools/akdat.py` |
| SETUP.EXE | Allegro 2.2's stock sound setup, same wrapper (stripped) | unpacked, not looked at further |

The plan is docs/PLAN.md; the current state and next steps are in docs/HANDOFF.md.

## Tools

The repository contains neither the game nor the tools. Put the GOG files in `gamedata/`. Then:

```sh
python tools/wdosx.py unpack gamedata/AK.EXE      # -> build/AK.COF (+ the kernel and stub)
python tools/coff.py files build/AK.COF           # objects in link order
python tools/coff.py funcs build/AK.COF akworld.cc
python tools/coff.py types build/AK.COF ak.cc     # classes from ak.cc's -g information
python tools/akdat.py list                        # AK.DAT's objects
```

With the toolchain archives in `tools/` (docs/HANDOFF.md lists them):

```sh
python tools/setup.py            # unpack the toolchain into build/tc
python tools/buildalleg.py       # build Allegro 2.2's liballeg.a
python tools/libmatch.py --quiet # all library objects identical
python tools/build.py            # link, compare with the original COFF
```

None of them needs anything beyond Python 3. capstone (`../u1-decompiled/build/venv`) is useful
for disassembly. To compile, put the toolchain archives in `tools/` (docs/HANDOFF.md lists them)
and run `python tools/setup.py`. `tools/dos.py` then runs gcc 2.7.2.1 in headless DOSBox, and
`python tools/gxx.py FILE.cc` compiles one file and compares its functions with the original
(docs/MATCHING.md). `tools/u1/` (ignored by git) holds the rest of u1's Lattice-era tools that
were in this folder.
