# File formats

The files AK.EXE reads and writes besides itself. Read from the code (and, for AK.DAT, from the
GOG file with `tools/akdat.py`).

## PLAYER.U0: the save file

`charsave::savegame` writes `charsave::sav` (akchar.h, `struct savestruct`) as it is: 124 bytes,
little-endian 32-bit integers, gcc's natural alignment. `loadgame` reads them back. There is no
header, version or checksum. Saving is only possible on the world map, so `inout` is always 0
in a saved file.

| offset | size | field | contents |
|---|---|---|---|
| 0 | 6 x 4 | `character[6]` | hit points, strength, dexterity, stamina, wisdom, gold (`ST_*`) |
| 24 | 6 x 4 | `owned[6]` | food, rapier, axe, shield, bow and arrow, magic amulet (`IT_*`) |
| 48 | 24 | `name` | Lord British asks it on the first visit (`cin`, NUL-terminated; garbage before that) |
| 72 | 1 | `ChClass` | `F` fighter or `M` mage |
| 73 | 3 | | padding |
| 76 | 4 | `ln` | the lucky number: the world and the dungeons are made from it |
| 80 | 4 | `level` | level of play, 1-10 |
| 84 | 4 + 4 | `tx`, `ty` | position on the world map |
| 92 | 4 + 4 | `px`, `py` | position in the dungeon |
| 100 | 4 + 4 | `dx`, `dy` | facing in the dungeon |
| 108 | 4 | `inout` | 0 on the surface, else the dungeon level |
| 112 | 4 | `task` | Lord British's quest: the monster's number, negative once done |
| 116 | 4 | `visit` | 1 once Lord British gave a quest |
| 120 | 4 | `lk` | hit points earned by kills in the current dungeon trip |

The world map is not saved: "Journey Onward" makes it again from `ln` (docs/game/world.md),
and the dungeon turn counter `charsave::count` is not part of the record.

## AK.DAT: texts and fonts

An Allegro 2.2 datafile packed as a whole (`slh!` magic, LZSS), holding `ALL.` and seven
objects, made with Allegro's grabber in `c:\djgpp\bin`, 13-20 November 1997:

| # | name (src/ak.h) | type | contents |
|---|---|---|---|
| 0 | `AKINS` | DATA | the instructions (`akaux::instruct`) |
| 1 | `AKLB1` | DATA | Lord British's welcome |
| 2 | `AKLB2` | DATA | the first quest |
| 3 | `AKLB3` | DATA | the knighthood speech |
| 4 | `ARIAL_BLACK` | FONT | not used by the game |
| 5 | `NEAT_FONT` | FONT | not used by the game |
| 6 | GrabberInfo | info | the grabber's settings |

The texts are one line per `;`. A line holding `~` is a page break, a line holding `` ` `` is
skipped (the speeches end with one). The game draws everything in Allegro's built-in 8x8 font,
so the two fonts are dead weight. AK.DAT is loaded at start (only to check it is there), and
again for each instruction screen and each visit to Lord British.

## SOUND.CFG

Allegro 2.2's sound configuration, written by SETUP.EXE (Allegro's stock setup program) and read
by `install_sound(DIGI_NONE, MIDI_AUTODETECT, argv[0])`: the game never plays digital sound,
only the MIDI music. GOG's file sets `midi_card = 2` and an SB-compatible port 260, IRQ 7.

| file | when |
|---|---|
| intro.mid | the menu |
| over.mid | the world map |
| shop.mid | the shop |
| lordbrit.mid | Lord British, View Ending |
| dungeon.mid | the dungeons |

## QUOTES.FLG and ENDGAME.FLG

Empty files, created in the current directory when the balrog quest is reported at level of
play 10 (and again by View Ending). The menu only checks that they open; each adds its entry
(docs/game/lordbritish.md).
