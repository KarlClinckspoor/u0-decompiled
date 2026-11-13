# What the code revealed

Bugs, oddities and surprises found while writing the game in C++ (phase 4) and documenting it
(phases 5 and 6). The rules of the game are in docs/game/; this file keeps what is wrong or
unexpected. Functions are those of `src/`, constants those of `src/akdefs.h`.

Each item has a status:
- **read**: read from the code only;
- **sim**: also reproduced with `tools/akmap.py`, whose random numbers match DJGPP's libc
  (checked by compiling a test program in DOSBox), but not yet seen in the game;
- **played**: seen in the running game (GOG's DOSBox with AK.EXE). Nothing is yet.

## Bugs

| # | what | where | status |
|---|---|---|---|
| 1 | **Mages can buy the rapier and the bow.** "I'm sorry Mages can't use that!" clears a flag (`ok`) that nothing reads; the test after the switch is `if (key != 0)`, always true, so the sale goes through and the gold is taken. | akchar.cc `shop` | read |
| 2 | **Ranged attacks facing `dy = -1` always miss.** The edge check's fourth test is `py * dy * i < 0` where `py + dy * i < 1` was meant. With `dy = -1` it is true at once, so the bow, a thrown axe and the magic attack give "Missed!" even point-blank when the compass says "East". (Phase 4's note that the shot "is not stopped at the edge" was wrong: the other three tests and the border walls keep it inside.) A thrown axe is lost all the same. | akworld.cc `attack` | read |
| 3 | **Ranged attacks pass through walls and doors.** The scan only stops at the map's edge or a monster. | akworld.cc `attack` | read |
| 4 | **The credits may end on a null pointer.** `credit[]` has 37 entries, the last a 0 that marks the end, but the scroller draws entries up to index 36, so when the credits are left to run, `textout_centre` gets a NULL string. Whether that shows garbage or crashes depends on whether the DPMI host protects page 0. The quotes' bound (93 of 94) is right. | akaux.cc `shoutouts` | read; check in play first |
| 5 | **Character creation reads into an uninitialised pointer.** The lucky number and the level are read with `cin >> input`, `input` being a `char *` that is never set: the text lands wherever the stale stack value points. | akchar.cc `charcreate` | read |
| 6 | **Buffer overflow in the stat display.** `itoa` writes the stat into a 4-byte buffer and `strcat` adds two spaces: a two-digit stat needs 5 bytes. | akchar.cc `charcreate` | read |
| 7 | **The monster's damage goes through an uninitialised pointer**: `itoa(damage, num, 10)` with `char *num` never set, then printed from there. | akworld.cc `movemonsters` | read |
| 8 | **The name is read unchecked**: `cin >> sav.name` into 24 bytes. | akchar.cc `lb` | read |
| 9 | **Lord British leaks AK.DAT.** On the first visit the second speech returns as soon as it reaches its `~`, before `unload_datafile`: every first visit leaves a copy of AK.DAT loaded. `lb` also falls off its end without a return value. | akchar.cc `lb` | read |
| 10 | **The menu leaks MIDIs.** Every pass of the menu loop loads intro.mid into `w.backmid`, but only Acknowledgements and View Quotes destroy it first. Introduction, View Ending (which also leaves lordbrit.mid behind) and Create a Character each leave one loaded. | ak.cc `main` | read |
| 11 | **With only ENDGAME.FLG, "View Ending" shows the quotes.** The menu then draws item 6 in fifth place, but returns the position, 5, which `main` takes for View Quotes. Both files are always written together, so it takes a deleted file to see it. | akmenu.cc, ak.cc `main` | read |
| 12 | **Towns are never thinned out.** `if (world[x][y] == 3 & rand() % 100 > 50) world[x][y] = 3;` sets a town to a town; it was probably meant to turn half the towns back into plains. (It calls `rand()`, so it doesn't disturb the map's `random()` sequence.) | akworld.cc `createworld` | sim (5% towns) |
| 13 | **Three strokes of the daemon do not shrink with distance**: `x + 17`, `x - 17` and `y - 7` lack the `/ dist` of every other offset. | akworld.cc `drawmonsters` | read |
| 14 | **The quest monster's name is printed for the wrong lines.** It should go on the line holding `)` ("killing a(n)"); the test is inverted, so it is printed at the same fixed place for each of the other eleven lines. Harmless on screen. | akchar.cc `lb` | read |
| 15 | **The backtick marker's else does nothing**: `if (skip == 0) print; else skip = 1;` sets a flag that is already set. | akchar.cc `lb` | read |
| 16 | **Falling can land the player inside a wall.** A trap or hole drops the player onto the same x, y one level down, a wall gap there, and about 26% of gaps are walls. Moving out works (only the target square is checked). | ak.cc `rundungeon`, akworld.cc `createdungeon` | sim (26%) |
| 17 | **A thief can steal the last food**, and the player starves at the end of that turn. | akworld.cc `movemonsters` | read |

## Oddities

- **`rand()` is never seeded.** Only `srandom()` is called (three times); no code calls `srand()`
  (checked over the whole of AK.COF). All combat, traps and chests follow the same sequence
  from every launch, and its first value is 0. (docs/game/random.md) — read
- **Levels are made anew on every entry**, so chests refill and killed monsters return, at full
  strength, whenever the player changes level. (docs/game/dungeon.md) — read
- **Endless stat gains after the last quest.** After the balrog the quest stays done, and every
  visit to the castle adds 1 to all six stats again. (docs/game/lordbritish.md) — read
- **The shield hits like bare hands**: its damage is `rand() % 1`, always 0, plus strength / 5.
  Every weapon's damage is one less than the shop's table says (0-9 for "1-10"). — read
- **The "bad" amulet spell is often good**: one outcome in three (Lizard Man) multiplies hit
  points and four stats by 2.5. — read
- **A chest's food is one unit**, where the shop sells ten for a gold piece. — read
- **Gold always comes with an item**: `if (gold > 0)` can't fail, the amount being
  `rand() % (5 * level) + level` with a level of at least 1. — read
- **A stat of 5 can never be rolled** (4, then 6 and up). — sim
- **A loaded game gets exactly its new game's world**: its "extra" town falls on the starting
  town, the random numbers coming in the same order. — sim
- **Ctrl-R keeps the world in memory**: loading another character's save mid-game puts them on
  the current world. — read
- **The compass in the dungeon** calls `dx = -1` "North" and `dy = -1` "East"; the dungeon has
  no tie to the world's directions. — read
- **Monsters are named only from two squares away**; the one right ahead is drawn unnamed. — read
- **Bitwise `&` and `|` for logic** throughout (`a == 1 & b != 1`). Each side is 0 or 1, so it
  works, but both sides are always evaluated (which is why the town test always calls
  `rand()`). — read
- `rungame` counts moves in `i` and declares `char string[10]`, and uses neither; in the
  dungeon, `y` is a case of its own that does nothing; `createmonsters` counts in `nm` and
  doesn't use it. — read
- **The intro demonstration is dead code**: nothing calls `akworld::viewintro`, and it sets the
  akworld's own unused `sav` rather than the player's record. — read
- AK.DAT carries two fonts the game never uses (everything is drawn in Allegro's 8x8 font).
  — read
- Typos in the texts: "Lord Brititsh" (AKLB1), "Kinght", "ressurection", "foolhearty". — read

## Still to compare with the Apple II original

The Applesoft listing would show how the original seeds `RND`, the original monster and hit
formulas, and whether bugs 12-16 and the oddities above are the port's or Garriott's. Not done
(the listing is not in the repository).
