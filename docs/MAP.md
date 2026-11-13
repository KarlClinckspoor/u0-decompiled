# Map of AK.EXE

What is inside the GOG AK.EXE, read with `tools/wdosx.py` and `tools/coff.py`. Addresses are
virtual addresses of the COFF program (`build/AK.COF`). Regenerate the tables with
`python tools/coff.py files build/AK.COF --md`, `funcs` and `types`.

## The file

| part | file bytes | what |
|---|---|---|
| WDOSX 0.97 kernel | 0000-25F8 | MZ program. Its 16-bit image is LZ-packed and unpacks itself in place (`wdosx.py` uses the same decoder) |
| stub | 25F8-2A21 | WDOSX's `go32stub, v 2.00T` (inner MZ, 1065 bytes) |
| WFSE `WdosxMain` | 2A21-3A572 | the DJGPP COFF program, in 140 independently packed 4096-byte chunks |

AK.EXE and SETUP.EXE carry byte-identical kernels and stubs. The program unpacks to
`build/AK.COF`: 570462 bytes, sha1 `3badf300cef0032f57b12468b51b448c3926bcab`. SETUP's is
`build/SETUP.COF`: 326144 bytes, sha1 `4061d87c9ac76867b3df5ff623edc6709ba0b06e`. Two of
SETUP's chunks are stored unpacked.

## The COFF program

- i386 COFF, flags 0x103, **timestamp 0** (no link time to reproduce), no relocations, a.out
  header magic 0x10B, entry 0x10A8 (`start` in crt0.s).
- `.text` 0x10A8-0x52000 (331608 bytes), `.data` 0x52000-0x58A00 (27136), `.bss`
  0x58A00-0x61600 (35840).
- 9382 symbols and a 40778-byte string table: **the program was not stripped**. Each object's
  `.file` symbol gives the **link order**, and its `.text`/`.data`/`.bss` section symbols give its
  exact address and size. Every static and global function and variable is named, and so is
  every local label (`L123` for gcc's code labels, `LC12` for literals). GNU ld moved the global
  symbols away from their objects; `coff.py` puts them back by address.
- **ak.cc was compiled with `-g`** (COFF debug symbols). That gives:
  - struct and class layouts with member names, including the game's own classes (below);
  - the arguments and locals of its 4 functions, with their frame offsets;
  - block scopes;
  - **316 line numbers**: each function's start line in the source, and a line for every
    statement.

  The other game objects and the libraries have no debug information.

| group | objects (#) | count | .text | .data | .bss |
|---|---|---|---|---|---|
| startup (crt0.s) | 0 | 1 | 1192 | 152 | 16 |
| **game** | 1-5 | 5 | **100132** | **0** | **0** |
| Allegro 2.2 | 6-45 | 40 | 113304 | 20656 | 14628 |
| libgpp / libio (C++ iostreams) | 46-67 | 22 | 57244 | 856 | 4 |
| libgcc (operator new/delete, 64-bit division) | 68-74 | 7 | 1324 | 4 | 0 |
| DJGPP 2.01 libc (with crt1, exception handling) | 75-267 | 193 | 58052 | 5248 | 16500 |

The objects named `fake` are assembly sources without a `.file` directive (Allegro's `.s` files,
some libc math). Objects named `*.stub` (`open.stub`, `findfirst.stub`, ...) are DJGPP 2.01's
libc stubs. The SVGA chipset drivers (ati.c, cirrus.c, paradise.c, s3.c, trident.c, tseng.c,
video7.c, vbeaf.c) are Allegro 2.2's.

## The game

Five C++ objects with 36 functions and **no `.data` or `.bss` of their own**. All game state sits
in the classes (their instances are locals of `main`). Every string and floating-point constant
sits in `.text`, after or between the functions (the `LC` labels).

| object | .text | functions | code labels | literal labels |
|---|---|---|---|---|
| ak.cc | 001550 +10104 | 4 | 129 | 51 |
| akaux.cc | 003CC8 +4820 | 6 | 60 | 90 |
| akchar.cc | 004F9C +8776 | 9 | 137 | 101 |
| akmenu.cc | 0071E4 +2052 | 5 | 44 | 3 |
| akworld.cc | 0079E8 +74380 | 12 | 282 | 84 |

| address | bytes | function |
|---|---|---|
| 001600 | 2260 | `rungame(charsave &, akworld &)` |
| 001ED4 | 6496 | `rundungeon(charsave &, akworld &)` |
| 003834 | 1128 | `main` |
| 003C9C | 44 | `static textout(BITMAP *, FONT *, char *, int, int, int)`, an inline from a header (`.bf` line 927) |
| 003D08 | 84 | `akaux::akaux()` |
| 003D5C | 88 | `akaux::~akaux()` |
| 003DB4 | 212 | `akaux::hello()` |
| 003E88 | 2492 | `akaux::instruct()` |
| 004844 | 1260 | `akaux::quotes()` |
| 004D30 | 620 | `akaux::shoutouts()` |
| 004F9C | 24 | `charsave::charsave()` |
| 004FB4 | 384 | `charsave::~charsave()` |
| 005134 | 1412 | `charsave::charcreate()` |
| 0056B8 | 2644 | `charsave::shop(int)` |
| 00610C | 184 | `charsave::savegame()` |
| 0061C4 | 776 | `charsave::loadgame()` |
| 0064CC | 1996 | `charsave::lb()` (Lord British) |
| 006C98 | 1092 | `charsave::checkstatus()` |
| 0070DC | 264 | `charsave::death()` |
| 0071E4 | 12 | `akmenu::akmenu()` |
| 0071F0 | 80 | `akmenu::~akmenu()` |
| 007240 | 452 | `akmenu::runmenu(char **)` |
| 007404 | 332 | `akmenu::draw(char **)` |
| 007550 | 1176 | `akmenu::redraw(char **)` (its size includes akmenu.cc's copy of `textout` at 0079BC) |
| 007A48 | 3264 | `akworld::akworld()` |
| 008708 | 100 | `akworld::~akworld()` |
| 00876C | 1224 | `akworld::createworld(int, charsave::savestruct &)` |
| 008C34 | 1552 | `akworld::drawworld(charsave::savestruct &)` |
| 009244 | 1752 | `akworld::createdungeon(charsave::savestruct &)` |
| 00991C | 13828 | `akworld::drawdungeon(charsave::savestruct &)` |
| 00CF20 | 804 | `akworld::createmonsters(charsave::savestruct &)` |
| 00D244 | 39396 | `akworld::drawmonsters(int, int)` |
| 016C28 | 5484 | `akworld::attack(charsave::savestruct &)` |
| 018194 | 5484 | `akworld::movemonsters(charsave::savestruct &)` |
| 019700 | 212 | `akworld::sgn(int)` |
| 0197D4 | 1184 | `akworld::viewintro(charsave::savestruct &)` |

Sizes run to the next function symbol and include the literals between functions. akmenu.cc's
`textout` is a local label in the symbol table, not a STAT symbol, which is why `coff.py funcs`
does not list it separately.

### Classes (from ak.cc's debug symbols)

The member names are Garriott's Applesoft variables (TX, TY, PX, PY, DX, DY, INOUT, TASK, LK,
PER, CD, LAD, LD, FT, XX, YY, MZ, ML, ...): the port kept the BASIC's names.

```
struct savestruct {            /* charsave::savestruct, 124 bytes */
    int  character[6];         /*   0 */
    int  owned[6];             /*  24 */
    char name[24];             /*  48 */
    char ChClass;              /*  72 */
    int  ln;                   /*  76 */
    int  level;                /*  80 */
    int  tx, ty;               /*  84, 88 */
    int  px, py;               /*  92, 96 */
    int  dx, dy;               /* 100, 104 */
    int  inout;                /* 108 */
    int  task;                 /* 112 */
    int  visit;                /* 116 */
    int  lk;                   /* 120 */
};
class charsave {               /* 128 bytes */
    savestruct sav;            /*   0 */
    int count;                 /* 124 */
};
class akworld : public charsave {  /* 3716 bytes; the base is the member named "charsave" at 0 */
    int world[21][21];         /*  128 */
    int dng[11][11];           /* 1892 */
    int mdraw, color, run1;    /* 2376, 2380, 2384 */
    BITMAP *worldbmp;          /* 2388 */
    MIDI *backmid;             /* 2392 */
    int per[11][4];            /* 2396 */
    int cd[11][4];             /* 2572 */
    int lad[11][4];            /* 2748 */
    int ld[11][6];             /* 2924 */
    int ft[11][6];             /* 3188 */
    int xx[11], yy[11];        /* 3452, 3496 */
    int mz[11][2], ml[11][2];  /* 3540, 3628 */
};
/* akworld's own charsave base is used only by viewintro (which is never called); the player's
   record is always passed in as a savestruct reference. */
class akmenu { int t1, t2, pos, max; };   /* 16 bytes */
class akaux { };                          /* 1 byte: no members */
```

### ak.cc's functions (from the debug symbols)

| function | source lines | arguments | locals (frame offset) |
|---|---|---|---|
| rungame | from 22 | player (charsave *, +8), w (akworld *, +C) | key -4, ky1 -8, ky2 -C, i -10, result -14, string char[10] -20 |
| rundungeon | from 169 | player +8, w +C | i -4, gold -8, key -C, ky1 -10, ky2 -14, result -18, string char[4] -1C, item char*[6] -34 |
| main | from 405 | argc +8, argv +C | result -4, r2 -8, item char*[7] -24, aux (akaux) -28, player (charsave) -A8, w (akworld) -F2C, menu (akmenu) -F3C |

The debug information shows references as pointers (C++ `&` has no COFF type). The array
dimensions above are as ak.cc's debug symbols store them, outermost first (an earlier version of
`coff.py types` printed them reversed; `src/ak.cc`'s debug symbols match only with
`per[11][4]` and so on).

`src/ak.cc` rebuilds this object completely, debug information included. Its line numbers fix
where every statement stands in the file, so the original's layout is known down to the line.
`rundungeon` spills every temporary to the frame (`[ebp-0x40]` and below) and `main` spills one,
while `rungame` keeps them in registers. gcc does this by itself; nothing in the source asks for
it.

## Code fingerprints (for choosing compiler flags)

Settled in phase 2; docs/MATCHING.md has the proof.

- **Game**: `-O0` (unoptimized), `-g` for ak.cc, no `-m486`, assembled with binutils 2.8.1. Signs:
  a frame pointer, every `return` jumping to a shared epilogue (even with a zero displacement,
  `eb 00`), `mov eax,eax` after calls, locals reloaded from memory. Functions are aligned to 4
  relative to their object, with 2.8.1's fillers (`90`, `89 f6`, `8d 76 00`). (A first reading
  here said "optimized"; the probes in phase 2 showed `-O0`.)
- **Allegro**: its makefile's `-m486 -O3 -ffast-math -fomit-frame-pointer` (arguments at
  `[esp+N]`, functions 16-aligned within each object), assembled with binutils 2.7.
- The doubles a function uses sit in `.text` just before it (e.g. 0.8 = `9a 99 99 99 99 99 e9 3f`).

## All objects in link order

| # | object | .text | size | .data | size | .bss | size |
|---|---|---|---|---|---|---|---|
| 0 | crt0.s | 0010A8 | 1192 | 05200C | 152 | 058A00 | 16 |
| 1 | ak.cc | 001550 | 10104 | 0520A4 | 0 | 058A10 | 0 |
| 2 | akaux.cc | 003CC8 | 4820 | 0520A4 | 0 | 058A10 | 0 |
| 3 | akchar.cc | 004F9C | 8776 | 0520A4 | 0 | 058A10 | 0 |
| 4 | akmenu.cc | 0071E4 | 2052 | 0520A4 | 0 | 058A10 | 0 |
| 5 | akworld.cc | 0079E8 | 74380 | 0520A4 | 0 | 058A10 | 0 |
| 6 | midi.c | 019C74 | 7876 | 0520A4 | 20 | 058A10 | 11928 |
| 7 | keyboard.c | 01BB38 | 3124 | 0520B8 | 944 | 05B8A8 | 1028 |
| 8 | text.c | 01C76C | 892 | 052468 | 1808 | 05BCAC | 0 |
| 9 | allegro.c | 01CAE8 | 1468 | 052B78 | 1284 | 05BCAC | 0 |
| 10 | timer.c | 01D0A4 | 2388 | 05307C | 12 | 05BCAC | 116 |
| 11 | gfx.c | 01D9F8 | 3900 | 053088 | 0 | 05BD20 | 0 |
| 12 | sound.c | 01E934 | 6804 | 053088 | 368 | 05BD20 | 304 |
| 13 | graphics.c | 0203C8 | 3560 | 0531F8 | 356 | 05BE50 | 0 |
| 14 | datafile.c | 0211B0 | 6276 | 05335C | 388 | 05BE50 | 0 |
| 15 | file.c | 022A34 | 10296 | 0534E0 | 8 | 05BE50 | 0 |
| 16 | dpmi.c | 02526C | 508 | 0534E8 | 0 | 05BE50 | 0 |
| 17 | adlib.c | 025468 | 8724 | 0534E8 | 2968 | 05BE50 | 360 |
| 18 | irq.c | 02767C | 528 | 054080 | 4 | 05BFB8 | 0 |
| 19 | blit.c | 02788C | 3852 | 054084 | 0 | 05BFB8 | 0 |
| 20 | fake | 028798 | 700 | 054084 | 0 | 05BFB8 | 0 |
| 21 | gus.c | 028A54 | 468 | 054084 | 176 | 05BFB8 | 0 |
| 22 | sb.c | 028C28 | 5652 | 054134 | 344 | 05BFB8 | 36 |
| 23 | mpu.c | 02A23C | 532 | 05428C | 136 | 05BFDC | 0 |
| 24 | digmid.c | 02A450 | 580 | 054314 | 1156 | 05BFDC | 0 |
| 25 | dma.c | 02A694 | 604 | 054798 | 128 | 05BFDC | 0 |
| 26 | vga.c | 02A8F0 | 200 | 054818 | 56 | 05BFDC | 0 |
| 27 | modex.c | 02A9B8 | 6148 | 054850 | 2560 | 05BFDC | 0 |
| 28 | vbeaf.c | 02C1BC | 3000 | 055250 | 236 | 05BFDC | 0 |
| 29 | vesa.c | 02CD74 | 4216 | 05533C | 296 | 05BFDC | 768 |
| 30 | ati.c | 02DDEC | 2800 | 055464 | 340 | 05C2DC | 4 |
| 31 | cirrus.c | 02E8DC | 1344 | 0555B8 | 240 | 05C2E0 | 0 |
| 32 | paradise.c | 02EE1C | 1268 | 0556A8 | 124 | 05C2E0 | 0 |
| 33 | s3.c | 02F310 | 836 | 055724 | 136 | 05C2E0 | 0 |
| 34 | trident.c | 02F654 | 1252 | 0557AC | 196 | 05C2E0 | 0 |
| 35 | tseng.c | 02FB38 | 1184 | 055870 | 276 | 05C2E0 | 0 |
| 36 | video7.c | 02FFD8 | 1100 | 055984 | 152 | 05C2E0 | 0 |
| 37 | fake | 030424 | 1112 | 055A1C | 0 | 05C2E0 | 0 |
| 38 | fake | 03087C | 5016 | 055A1C | 0 | 05C2E0 | 0 |
| 39 | fake | 031C14 | 712 | 055A1C | 0 | 05C2E0 | 0 |
| 40 | mouse.c | 031EDC | 3524 | 055A1C | 308 | 05C2E0 | 84 |
| 41 | sprite.c | 032CA0 | 4248 | 055B50 | 0 | 05C334 | 0 |
| 42 | fake | 033D38 | 516 | 055B50 | 0 | 05C334 | 0 |
| 43 | fake | 033F3C | 3140 | 055B50 | 0 | 05C334 | 0 |
| 44 | fake | 034B80 | 2372 | 055B50 | 0 | 05C334 | 0 |
| 45 | math.c | 0354C4 | 584 | 055B50 | 5636 | 05C334 | 0 |
| 46 | stdstreams.cc | 03570C | 120 | 057154 | 172 | 05C334 | 0 |
| 47 | iostream.cc | 035784 | 11148 | 057200 | 0 | 05C334 | 0 |
| 48 | fstream.cc | 038310 | 7400 | 057200 | 0 | 05C334 | 0 |
| 49 | streambuf.cc | 039FF8 | 3684 | 057200 | 4 | 05C334 | 0 |
| 50 | stdstrbufs.cc | 03AE5C | 0 | 057204 | 472 | 05C334 | 0 |
| 51 | genops.c | 03AE5C | 3772 | 0573DC | 0 | 05C334 | 0 |
| 52 | isgetline.cc | 03BD18 | 1104 | 0573DC | 0 | 05C334 | 0 |
| 53 | ioseekpos.c | 03C168 | 44 | 0573DC | 0 | 05C334 | 0 |
| 54 | ioseekoff.c | 03C194 | 72 | 0573DC | 0 | 05C334 | 0 |
| 55 | isscan.cc | 03C1DC | 188 | 0573DC | 0 | 05C334 | 0 |
| 56 | iopadn.c | 03C298 | 164 | 0573DC | 0 | 05C334 | 0 |
| 57 | outfloat.c | 03C33C | 1348 | 0573DC | 0 | 05C334 | 0 |
| 58 | filebuf.cc | 03C880 | 840 | 0573DC | 0 | 05C334 | 0 |
| 59 | fileops.c | 03CBC8 | 3120 | 0573DC | 144 | 05C334 | 0 |
| 60 | stdiostream.cc | 03D7F8 | 2552 | 05746C | 0 | 05C334 | 0 |
| 61 | iogetline.c | 03E1F0 | 172 | 05746C | 0 | 05C334 | 0 |
| 62 | sbscan.cc | 03E29C | 80 | 05746C | 0 | 05C334 | 0 |
| 63 | floatconv.c | 03E2EC | 16904 | 05746C | 28 | 05C334 | 4 |
| 64 | filedoalloc.c | 0424F4 | 172 | 057488 | 0 | 05C338 | 0 |
| 65 | ioprims.c | 0425A0 | 92 | 057488 | 0 | 05C338 | 0 |
| 66 | iovfscanf.c | 0425FC | 4232 | 057488 | 36 | 05C338 | 0 |
| 67 | ioungetc.c | 043684 | 36 | 0574AC | 0 | 05C338 | 0 |
| 68 | _op_delete.c | 0436A8 | 20 | 0574AC | 0 | 05C338 | 0 |
| 69 | _op_vdel.c | 0436BC | 16 | 0574AC | 0 | 05C338 | 0 |
| 70 | _umoddi3.c | 0436CC | 604 | 0574AC | 0 | 05C338 | 0 |
| 71 | _udivdi3.c | 043928 | 520 | 0574AC | 0 | 05C338 | 0 |
| 72 | _op_vnew.c | 043B30 | 16 | 0574AC | 0 | 05C338 | 0 |
| 73 | _op_new.c | 043B40 | 60 | 0574AC | 0 | 05C338 | 0 |
| 74 | _new_handler.c | 043B7C | 88 | 0574AC | 4 | 05C338 | 0 |
| 75 | npxsetup.c | 043BD4 | 572 | 0574B0 | 0 | 05C338 | 4 |
| 76 | ident.c | 043E10 | 0 | 0574B0 | 120 | 05C33C | 0 |
| 77 | crt1.c | 043E10 | 796 | 057528 | 16 | 05C33C | 0 |
| 78 | fake | 04412C | 904 | 057538 | 0 | 05C33C | 8000 |
| 79 | infostrc.c | 0444B4 | 0 | 057538 | 0 | 05E27C | 0 |
| 80 | sound.c | 0444B4 | 80 | 057538 | 0 | 05E27C | 0 |
| 81 | itoa.c | 044504 | 176 | 057538 | 0 | 05E27C | 0 |
| 82 | rand.c | 0445B4 | 140 | 057538 | 8 | 05E27C | 0 |
| 83 | fake | 044640 | 44 | 057540 | 0 | 05E27C | 0 |
| 84 | strlen.c | 04466C | 36 | 057540 | 0 | 05E27C | 0 |
| 85 | _main.c | 044690 | 68 | 057540 | 4 | 05E27C | 0 |
| 86 | exit.c | 0446D4 | 104 | 057544 | 4 | 05E27C | 0 |
| 87 | printf.c | 04473C | 40 | 057548 | 0 | 05E27C | 0 |
| 88 | strsep.c | 044764 | 80 | 057548 | 0 | 05E27C | 0 |
| 89 | strchr.c | 0447B4 | 44 | 057548 | 0 | 05E27C | 0 |
| 90 | conio.c | 0447E0 | 4392 | 057548 | 36 | 05E27C | 16 |
| 91 | atoi.c | 045908 | 20 | 05756C | 0 | 05E28C | 0 |
| 92 | random.c | 04591C | 948 | 05756C | 196 | 05E28C | 0 |
| 93 | fake | 045CD0 | 8 | 057630 | 0 | 05E28C | 0 |
| 94 | strcat.c | 045CD8 | 48 | 057630 | 0 | 05E28C | 0 |
| 95 | fopen.c | 045D08 | 312 | 057630 | 0 | 05E28C | 0 |
| 96 | stderr.c | 045E40 | 4 | 057630 | 28 | 05E28C | 0 |
| 97 | fprintf.c | 045E44 | 132 | 05764C | 0 | 05E28C | 0 |
| 98 | fwrite.c | 045EC8 | 532 | 05764C | 0 | 05E28C | 0 |
| 99 | fclose.c | 0460DC | 172 | 05764C | 0 | 05E28C | 0 |
| 100 | fseek.c | 046188 | 240 | 05764C | 0 | 05E28C | 0 |
| 101 | fread.c | 046278 | 300 | 05764C | 0 | 05E28C | 0 |
| 102 | abs.c | 0463A4 | 16 | 05764C | 0 | 05E28C | 0 |
| 103 | fake | 0463B4 | 12 | 05764C | 0 | 05E28C | 0 |
| 104 | fake | 0463C0 | 184 | 05764C | 8 | 05E28C | 0 |
| 105 | gopint.c | 046478 | 1184 | 057654 | 104 | 05E28C | 0 |
| 106 | malloc.c | 046918 | 564 | 0576BC | 0 | 05E28C | 128 |
| 107 | dmgw.c | 046B4C | 32 | 0576BC | 0 | 05E30C | 0 |
| 108 | dmpw.c | 046B6C | 32 | 0576BC | 0 | 05E30C | 0 |
| 109 | d0300_z.S | 046B8C | 128 | 0576BC | 0 | 05E30C | 4 |
| 110 | getdos_v.c | 046C0C | 448 | 0576BC | 188 | 05E310 | 0 |
| 111 | atexit.c | 046DCC | 56 | 057778 | 0 | 05E310 | 0 |
| 112 | dpmiexcp.c | 046E04 | 3080 | 057778 | 100 | 05E310 | 1516 |
| 113 | _write.c | 047A0C | 236 | 0577DC | 0 | 05E8FC | 0 |
| 114 | strcpy.c | 047AF8 | 40 | 0577DC | 0 | 05E8FC | 0 |
| 115 | getenv.c | 047B20 | 92 | 0577DC | 0 | 05E8FC | 0 |
| 116 | stricmp.stub | 047B7C | 8 | 0577DC | 0 | 05E8FC | 0 |
| 117 | strtol.c | 047B84 | 384 | 0577DC | 0 | 05E8FC | 0 |
| 118 | errno.c | 047D04 | 0 | 0577DC | 0 | 05E8FC | 0 |
| 119 | sprintf.c | 047D04 | 56 | 0577DC | 0 | 05E8FC | 0 |
| 120 | ct_lower.c | 047D3C | 0 | 0577DC | 260 | 05E8FC | 0 |
| 121 | findfirst.stub | 047D3C | 8 | 0578E0 | 0 | 05E8FC | 0 |
| 122 | fake | 047D44 | 8 | 0578E0 | 0 | 05E8FC | 0 |
| 123 | findnext.stub | 047D4C | 8 | 0578E0 | 0 | 05E8FC | 0 |
| 124 | open.stub | 047D54 | 8 | 0578E0 | 0 | 05E8FC | 0 |
| 125 | close.stub | 047D5C | 8 | 0578E0 | 0 | 05E8FC | 0 |
| 126 | lseek.stub | 047D64 | 8 | 0578E0 | 0 | 05E8FC | 0 |
| 127 | tmpnam.c | 047D6C | 388 | 0578E0 | 20 | 05E8FC | 532 |
| 128 | read.stub | 047EF0 | 8 | 0578F4 | 0 | 05EB10 | 0 |
| 129 | write.stub | 047EF8 | 8 | 0578F4 | 0 | 05EB10 | 0 |
| 130 | d0800.S | 047F00 | 68 | 0578F4 | 0 | 05EB10 | 0 |
| 131 | d0600.S | 047F44 | 56 | 0578F4 | 0 | 05EB10 | 0 |
| 132 | d0801.S | 047F7C | 44 | 0578F4 | 0 | 05EB10 | 0 |
| 133 | d0000.S | 047FA8 | 36 | 0578F4 | 0 | 05EB10 | 0 |
| 134 | d0007.S | 047FCC | 44 | 0578F4 | 0 | 05EB10 | 0 |
| 135 | d0008.S | 047FF8 | 44 | 0578F4 | 0 | 05EB10 | 0 |
| 136 | d0001.S | 048024 | 36 | 0578F4 | 0 | 05EB10 | 0 |
| 137 | d0006.S | 048048 | 44 | 0578F4 | 0 | 05EB10 | 0 |
| 138 | d0601.S | 048074 | 56 | 0578F4 | 0 | 05EB10 | 0 |
| 139 | d0204.S | 0480AC | 44 | 0578F4 | 0 | 05EB10 | 0 |
| 140 | d0205.S | 0480D8 | 44 | 0578F4 | 0 | 05EB10 | 0 |
| 141 | d0101.S | 048104 | 36 | 0578F4 | 0 | 05EB10 | 0 |
| 142 | d0100.S | 048128 | 60 | 0578F4 | 0 | 05EB10 | 0 |
| 143 | nearptr.c | 048164 | 180 | 0578F4 | 0 | 05EB10 | 0 |
| 144 | dosmemput.stub | 048218 | 8 | 0578F4 | 0 | 05EB10 | 0 |
| 145 | dosmemget.stub | 048220 | 8 | 0578F4 | 0 | 05EB10 | 0 |
| 146 | strncmp.c | 048228 | 60 | 0578F4 | 0 | 05EB10 | 0 |
| 147 | gormcb.c | 048264 | 752 | 0578F4 | 112 | 05EB10 | 0 |
| 148 | memcpy.s | 048554 | 28 | 057964 | 0 | 05EB10 | 0 |
| 149 | ct_flags.c | 048570 | 0 | 057964 | 516 | 05EB10 | 0 |
| 150 | stdout.c | 048570 | 4 | 057B68 | 28 | 05EB10 | 0 |
| 151 | fflush.c | 048574 | 128 | 057B84 | 0 | 05EB10 | 0 |
| 152 | stdin.c | 0485F4 | 4 | 057B84 | 28 | 05EB10 | 0 |
| 153 | memmove.s | 0485F8 | 40 | 057BA0 | 0 | 05EB10 | 0 |
| 154 | fileno.c | 048620 | 20 | 057BA0 | 0 | 05EB10 | 0 |
| 155 | getc.c | 048634 | 120 | 057BA0 | 0 | 05EB10 | 0 |
| 156 | memchr.c | 0486AC | 48 | 057BA0 | 0 | 05EB10 | 0 |
| 157 | huge_val.c | 0486DC | 0 | 057BA0 | 8 | 05EB10 | 0 |
| 158 | isatty.stub | 0486DC | 8 | 057BA8 | 0 | 05EB10 | 0 |
| 159 | fstat.c | 0486E4 | 2520 | 057BA8 | 44 | 05EB10 | 104 |
| 160 | strtoul.c | 0490BC | 344 | 057BD4 | 0 | 05EB78 | 0 |
| 161 | d0e01.S | 049214 | 40 | 057BD4 | 0 | 05EB78 | 0 |
| 162 | fake | 04923C | 76 | 057BD4 | 0 | 05EB78 | 0 |
| 163 | cntrl87.S | 049288 | 40 | 057BD4 | 0 | 05EB78 | 0 |
| 164 | dxeload.c | 0492B0 | 196 | 057BD4 | 0 | 05EB78 | 0 |
| 165 | fake | 049374 | 116 | 057BD4 | 0 | 05EB78 | 0 |
| 166 | d0400.S | 0493E8 | 56 | 057BD4 | 0 | 05EB78 | 0 |
| 167 | md.S | 049420 | 44 | 057BD4 | 0 | 05EB78 | 0 |
| 168 | abort.c | 04944C | 24 | 057BD4 | 12 | 05EB78 | 0 |
| 169 | putenv.c | 049464 | 612 | 057BE0 | 20 | 05EB78 | 0 |
| 170 | _use_lfn.c | 0496C8 | 708 | 057BF4 | 12 | 05EB78 | 4 |
| 171 | c1args.c | 04998C | 2572 | 057C00 | 0 | 05EB7C | 0 |
| 172 | c1loadef.c | 04A398 | 1412 | 057C00 | 0 | 05EB7C | 0 |
| 173 | stdiohk.c | 04A91C | 52 | 057C00 | 4 | 05EB7C | 0 |
| 174 | setmode.c | 04A950 | 228 | 057C04 | 0 | 05EB7C | 0 |
| 175 | doprnt.c | 04AA34 | 5848 | 057C04 | 352 | 05EB7C | 0 |
| 176 | dmp.c | 04C10C | 32 | 057D64 | 0 | 05EB7C | 0 |
| 177 | dmg.c | 04C12C | 32 | 057D64 | 0 | 05EB7C | 0 |
| 178 | scsetc.c | 04C14C | 40 | 057D64 | 0 | 05EB7C | 0 |
| 179 | scgetc.c | 04C174 | 52 | 057D64 | 0 | 05EB7C | 0 |
| 180 | scattrib.c | 04C1A8 | 0 | 057D64 | 4 | 05EB7C | 0 |
| 181 | scputc.c | 04C1A8 | 104 | 057D68 | 0 | 05EB7C | 0 |
| 182 | kbhit.c | 04C210 | 80 | 057D68 | 0 | 05EB7C | 0 |
| 183 | vsprintf.c | 04C260 | 56 | 057D68 | 0 | 05EB7C | 0 |
| 184 | doscan.c | 04C298 | 2320 | 057D68 | 260 | 05EB7C | 0 |
| 185 | sccols.c | 04CBA8 | 28 | 057E6C | 0 | 05EB7C | 0 |
| 186 | scrows.c | 04CBC4 | 28 | 057E6C | 0 | 05EB7C | 0 |
| 187 | scmode.c | 04CBE0 | 28 | 057E6C | 0 | 05EB7C | 0 |
| 188 | allocfil.c | 04CBFC | 168 | 057E6C | 0 | 05EB7C | 0 |
| 189 | fmode.c | 04CCA4 | 0 | 057E6C | 4 | 05EB7C | 0 |
| 190 | open.c | 04CCA4 | 216 | 057E70 | 0 | 05EB7C | 0 |
| 191 | lseek.c | 04CD7C | 100 | 057E70 | 0 | 05EB7C | 0 |
| 192 | dosio.c | 04CDE0 | 188 | 057E70 | 32 | 05EB7C | 0 |
| 193 | flsbuf.c | 04CE9C | 304 | 057E90 | 0 | 05EB7C | 0 |
| 194 | close.c | 04CFCC | 16 | 057E90 | 0 | 05EB7C | 0 |
| 195 | remove.c | 04CFDC | 188 | 057E90 | 0 | 05EB7C | 0 |
| 196 | ftell.c | 04D098 | 96 | 057E90 | 0 | 05EB7C | 0 |
| 197 | filbuf.c | 04D0F8 | 384 | 057E90 | 0 | 05EB7C | 0 |
| 198 | memset.s | 04D278 | 64 | 057E90 | 0 | 05EB7C | 0 |
| 199 | mdw.S | 04D2B8 | 44 | 057E90 | 0 | 05EB7C | 0 |
| 200 | d0200.S | 04D2E4 | 48 | 057E90 | 0 | 05EB7C | 0 |
| 201 | d0201.S | 04D314 | 48 | 057E90 | 0 | 05EB7C | 0 |
| 202 | d0202.S | 04D344 | 44 | 057E90 | 0 | 05EB7C | 0 |
| 203 | d0203.S | 04D370 | 44 | 057E90 | 0 | 05EB7C | 0 |
| 204 | d0303.S | 04D39C | 68 | 057E90 | 0 | 05EB7C | 0 |
| 205 | d0304.S | 04D3E0 | 40 | 057E90 | 0 | 05EB7C | 0 |
| 206 | dpmi_lsl.S |  |  |  |  |  |  |
| 207 | dpmi_lsl.s | 04D408 | 20 | 057E90 | 0 | 05EB7C | 0 |
| 208 | osmajor.c | 04D41C | 0 | 057E90 | 0 | 05EB7C | 0 |
| 209 | osminor.c | 04D41C | 0 | 057E90 | 0 | 05EB7C | 0 |
| 210 | osflavor.c | 04D41C | 0 | 057E90 | 0 | 05EB7C | 0 |
| 211 | write.c | 04D41C | 336 | 057E90 | 4 | 05EB7C | 0 |
| 212 | fsext.c | 04D56C | 372 | 057E94 | 4 | 05EB7C | 8 |
| 213 | doserr2e.c | 04D6E0 | 32 | 057E98 | 112 | 05EB84 | 0 |
| 214 | stricmp.c | 04D700 | 80 | 057F08 | 0 | 05EB84 | 0 |
| 215 | findfirs.c | 04D750 | 512 | 057F08 | 0 | 05EB84 | 0 |
| 216 | findnext.c | 04D950 | 440 | 057F08 | 0 | 05EB84 | 0 |
| 217 | access.c | 04DB08 | 208 | 057F08 | 0 | 05EB84 | 0 |
| 218 | read.c | 04DBD8 | 168 | 057F08 | 4 | 05EB84 | 0 |
| 219 | djmd.s | 04DC80 | 40 | 057F0C | 0 | 05EB84 | 0 |
| 220 | isatty.c | 04DCA8 | 56 | 057F0C | 0 | 05EB84 | 0 |
| 221 | xstat.c | 04DCE0 | 592 | 057F0C | 12 | 05EB84 | 1024 |
| 222 | lfnftime.c | 04DF30 | 120 | 057F18 | 0 | 05EF84 | 0 |
| 223 | filelen.c | 04DFA8 | 236 | 057F18 | 0 | 05EF84 | 0 |
| 224 | getuid.c | 04E094 | 12 | 057F18 | 0 | 05EF84 | 0 |
| 225 | getgid.c | 04E0A0 | 12 | 057F18 | 0 | 05EF84 | 0 |
| 226 | time.c | 04E0AC | 52 | 057F18 | 0 | 05EF84 | 0 |
| 227 | is_exec.c | 04E0E0 | 772 | 057F18 | 448 | 05EF84 | 0 |
| 228 | getdinfo.c | 04E3E4 | 72 | 0580D8 | 0 | 05EF84 | 0 |
| 229 | _open.c | 04E42C | 220 | 0580D8 | 0 | 05EF84 | 0 |
| 230 | _read.c | 04E508 | 224 | 0580D8 | 0 | 05EF84 | 0 |
| 231 | _close.c | 04E5E8 | 100 | 0580D8 | 0 | 05EF84 | 0 |
| 232 | strcmp.c | 04E64C | 40 | 0580D8 | 0 | 05EF84 | 0 |
| 233 | d_getdrv.c | 04E674 | 40 | 0580D8 | 0 | 05EF84 | 0 |
| 234 | ct_upper.c | 04E69C | 0 | 0580D8 | 260 | 05EF84 | 0 |
| 235 | c1pglob.c | 04E69C | 36 | 0581DC | 0 | 05EF84 | 0 |
| 236 | calloc.c | 04E6C0 | 48 | 0581DC | 0 | 05EF84 | 0 |
| 237 | strncpy.c | 04E6F0 | 60 | 0581DC | 0 | 05EF84 | 0 |
| 238 | strncat.c | 04E72C | 64 | 0581DC | 0 | 05EF84 | 0 |
| 239 | fwalk.c | 04E76C | 64 | 0581DC | 0 | 05EF84 | 0 |
| 240 | lconv.c | 04E7AC | 12 | 0581DC | 48 | 05EF84 | 0 |
| 241 | putc.c | 04E7B8 | 128 | 05820C | 0 | 05EF84 | 0 |
| 242 | fake | 04E838 | 44 | 05820C | 0 | 05EF84 | 0 |
| 243 | ungetc.c | 04E864 | 100 | 05820C | 0 | 05EF84 | 0 |
| 244 | fgetc.c | 04E8C8 | 120 | 05820C | 0 | 05EF84 | 0 |
| 245 | atof.c | 04E940 | 16 | 05820C | 0 | 05EF84 | 0 |
| 246 | atold.c | 04E950 | 16 | 05820C | 0 | 05EF84 | 0 |
| 247 | frlist.c | 04E960 | 0 | 05820C | 92 | 05EF84 | 0 |
| 248 | fexistp.c | 04E960 | 48 | 058268 | 0 | 05EF84 | 0 |
| 249 | _creat.c | 04E990 | 216 | 058268 | 0 | 05EF84 | 0 |
| 250 | _chmod.c | 04EA68 | 140 | 058268 | 0 | 05EF84 | 0 |
| 251 | putpath.c | 04EAF4 | 240 | 058268 | 0 | 05EF84 | 0 |
| 252 | fixpath.c | 04EBE4 | 912 | 058268 | 0 | 05EF84 | 4 |
| 253 | crlf2nl.c | 04EF74 | 48 | 058268 | 0 | 05EF88 | 0 |
| 254 | ctime.c | 04EFA4 | 6180 | 058268 | 1520 | 05EF88 | 5104 |
| 255 | gettimeo.c | 0507C8 | 216 | 058858 | 0 | 060378 | 0 |
| 256 | strstr.c | 0508A0 | 92 | 058858 | 0 | 060378 | 0 |
| 257 | fse_open.c | 0508FC | 116 | 058858 | 4 | 060378 | 0 |
| 258 | glob.c | 050970 | 2644 | 05885C | 0 | 060378 | 36 |
| 259 | strtod.c | 0513C4 | 412 | 05885C | 0 | 06039C | 0 |
| 260 | strtold.c | 051560 | 432 | 05885C | 156 | 06039C | 0 |
| 261 | stdprn.c | 051710 | 4 | 0588F8 | 28 | 06039C | 0 |
| 262 | stdaux.c | 051714 | 4 | 058914 | 28 | 06039C | 0 |
| 263 | fncase.c | 051718 | 64 | 058930 | 0 | 06039C | 0 |
| 264 | lfnshort.c | 051758 | 336 | 058930 | 0 | 06039C | 0 |
| 265 | strrchr.c | 0518A8 | 40 | 058930 | 0 | 06039C | 0 |
| 266 | fnmatch.c | 0518D0 | 752 | 058930 | 0 | 06039C | 0 |
| 267 | qsort.c | 051BC0 | 728 | 058930 | 0 | 06039C | 16 |
