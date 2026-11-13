# What the code revealed

Bugs, oddities and mechanics found while writing the game in C++ (phase 4). Addresses are
AK.COF's; the functions are in `src/`. Everything here is **read from the code, not yet seen in
play**, unless marked otherwise.

## Bugs

- **Mages can buy the rapier and the bow** (akchar.cc `shop`). For R and B, a mage is told "I'm
  sorry Mages can't use that!" and a flag (`ok`) is cleared, but nothing reads that flag: the test
  after the switch is `if (key != 0)`, which always holds, so the purchase goes through and the
  gold is taken. The flag looks like the variable that test was meant to read.
- **Character creation reads into an uninitialised pointer** (akchar.cc `charcreate`). The lucky
  number and the level are read with `cin >> input`, where `input` is a `char *` local that is
  never set: the text lands wherever the stale stack value points.
- **Buffer overflow in the stat display** (akchar.cc `charcreate`). `itoa` writes the stat into a
  4-byte buffer and `strcat` adds two spaces: a two-digit stat needs 5 bytes.
- **Lord British leaks AK.DAT** (akchar.cc `lb`). On the first visit, the second speech returns
  as soon as it reaches its `~` page break, before `unload_datafile`. Every first visit leaves a
  copy of AK.DAT loaded.
- **The backtick marker does nothing** (akchar.cc `lb`). Lines holding `` ` `` are skipped; for
  the others `if (skip == 0) print; else skip = 1;` sets a flag that is already set.
- **Bitwise `&` and `|` for logic** (akmenu.cc, akaux.cc, akchar.cc). The tests are written
  `a == 1 & b != 1`. Each side is 0 or 1, so this works, but it always evaluates both sides.
- **The menu leaks MIDIs** (ak.cc `main`). Every pass of the menu loop loads intro.mid into
  `w.backmid`, but only Acknowledgements and View Quotes destroy it first. Introduction, View
  Ending (which also leaves lordbrit.mid behind) and Create a Character each leave one loaded.
- **Gold always comes with an item** (ak.cc `rundungeon`). The test `if (gold > 0)` can't fail:
  the amount is `rand() % (5 * level) + level` with a dungeon level of at least 1.
- Oddities in ak.cc: `rungame` counts moves in `i` and declares `char string[10]`, and uses
  neither; in the dungeon, `y` is a case of its own that does nothing.
- **Towns are never thinned out** (akworld.cc `createworld`). After the terrain roll comes
  `if (world[x][y] == 3 & rand() % 100 > 50) world[x][y] = 3;`, which sets the square to the
  value it already has. Probably meant to turn half of the towns back into plains.
- **More uninitialised buffers** (akworld.cc `movemonsters`). The damage a monster does is
  written by `itoa` into `char *string`, a pointer that is never set: the text lands wherever
  the stale stack value points, then is printed from there.
- **The ranged attack's edge check is mistyped** (akworld.cc `attack`). The fourth test is
  `player.py * player.dy * y < 0` where `player.py + player.dy * y < 1` was meant, so a shot
  north is not stopped at the edge and reads `dng` outside the row. A thrown axe, an arrow or a
  magic attack that finds nothing within 5 squares then hits whatever is on the square next to
  the player.
- **Three monster strokes do not shrink with distance** (akworld.cc `drawmonsters`). The daemon's
  `x + 17`, `x - 17` and `y - 7` lack the `/ dist` of every other offset.
- **The intro demonstration is dead code** (akworld.cc `viewintro`): nothing calls it. It also
  sets the akworld's own unused `sav` instead of the player's record.
- `lb()` and `death()` are reached as `int` functions that fall off their end (`lb`) or return
  nothing (`death` is void); `lb`'s return value is garbage when it falls off the end.

## Mechanics

- **Stats**: `floor(sqrt(random() % 100 * 0.01) * 21 + 4)`, the Apple original's
  `INT(SQR(RND(1))*21+4)` with a coarser random number (100 steps). The lucky number seeds
  `srandom`. Level of play 1-10.
- **Shop prices** (food is bought 10 at a time): food 1, rapier 8, axe 5, shield 6, bow 3, amulet
  15 gold. `shop(-1)` only shows the stats and waits for a key.
- **Food**: one unit a turn on the surface, one every 5th turn in a dungeon (`checkstatus`).
  Food or hit points at 0 or below means death. Food and hit points of 10 or less are shown in
  red (colour 12).
- **Lord British** (`lb`): the first quest is monster number wisdom / 3, at most 10 (1 skeleton
  ... 10 balrog). Every visit that ends a quest adds 1 to all six stats. A finished quest has a
  negative `task`; the next is `abs(task) + 1`. After the 10th, at level of play 10, he writes
  QUOTES.FLG and ENDGAME.FLG, which unlock the menu's extra entries (akmenu.cc).
- **Save file** PLAYER.U0: the 124-byte `charsave::savestruct` as it is (src/akchar.h).
- **AK.DAT texts**: one line per `;`, a page break at a line holding `~`. In Lord British's
  speeches a line holding `)` is where the monster's name is printed, and `` ` `` marks a line
  to skip.
- **Keys on the world map** (ak.cc `rungame`): the arrows walk, and mountains (map value 1) block
  with a beep. `E` or `X` acts on the square: 3 a town, i.e. the shop; 5 the castle, i.e. Lord
  British; 4 a dungeon. `Q` or Ctrl-S saves, Ctrl-R loads, `Z` shows the stats. Ctrl-Q or
  Ctrl-E ends the game without saving. Every key is a turn (`checkstatus`).
- **Keys in the dungeon** (ak.cc `rundungeon`): up moves forward and down turns around. Left and
  right turn, Alt+Left and Alt+Right strafe, Alt+Down retreats (BIOS codes 155, 157, 160). A wall
  is map value 1, and values of 10 and up block too. `E`, `K` or `X` climbs: 7 or 9 is a ladder
  down, 8 a ladder up, and 8 on level 1 is the exit. `A` attacks, `Z` shows the stats, space
  waits. Ctrl-R loads; `Q`, `q` and Ctrl-S answer "Can Not Save in Dungeons!".
- **Entering a dungeon** puts the player on level 1 at (1,1), facing `dx = 1` (shown as
  "South"), and sets `lk` to 0. **Leaving** adds `lk` to the hit points (`lk` is presumably
  raised in combat, in akworld.cc).
- **Traps** (map value 2, never cleared): the player takes `rand() % level + 3` hit points of
  damage and falls one level.
- **Gold** (map value 5, cleared when found): `rand() % (5 * level) + level` pieces, and always
  one item, `rand() % 6`: one unit of food, a rapier, an axe, a shield, a bow or an amulet.
- **Death** (`checkstatus` returning 2) goes back to the menu. Choosing "Journey Onward" without
  a PLAYER.U0 leads to character creation (`loadgame` returns 1). "View Ending" sets the level
  of play to 10 and the quest to finished, then plays Lord British's last speech.
- **The world map** (akworld.cc `createworld`): 21x21 with a border of mountains. Each square
  is `floor(RND^5 * 4.5)`: 0 plains (the most common), 1 mountains, 2 a tree, 3 a town, 4 a
  dungeon entrance (rare). One castle (5) at a random place. A new game starts on a town at a
  random place; a loaded game gets an extra town at a random place. The map comes from the
  lucky number (`srandom(ln)`), so the same character always gets the same world.
- **Dungeon levels** (akworld.cc `createdungeon`): seeded with `ln * 3 + level`, so each level
  of a game is always the same. A grid of walls with, per wall gap, 4% traps, 29% hidden doors,
  59% doors, 2% ladders down and 5% chests (later rolls overwrite earlier ones). The ladders sit
  at (7,3) and (3,7), up and down swapping with the level's parity. Level 1 has the exit at
  (1,1).
- **Monsters** (akworld.cc `createmonsters`): monster x (1 skeleton ... 10 balrog) appears on
  levels deeper than x - 2, on a free square, with `x * 2 + level * 2 + level of play` hit points.
- **The player's attack** (akworld.cc `attack`): weapon damage rapier 10, axe 5 (or throw it,
  range 5, and lose it), shield 1, bow 4 (range 5), bare hands 0, magic attack level + 10. A hit
  does `rand() % weapon + strength / 5` (strength / 5 bare-handed). It misses when
  `dexterity - rand() % 25 < monster + level`. A kill adds `monster * level / 2` to `lk` (the
  hit points granted on leaving the dungeon) and gives `monster + level` gold. Killing the quest
  monster finishes the quest (two tones).
- **The magic amulet**: a mage picks the spell, a fighter gets a random one: 1 ladder up and 2
  ladder down (a ladder appears under the player), 3 magic attack, 4 a random bad effect: turned
  into a toad (strength to wisdom set to 3), into a lizard man (hit points to wisdom * 2.5) or a
  backfire (half the hit points). Each use has a 24% chance of being the amulet's last.
- **The monsters' turn** (akworld.cc `movemonsters`): a monster weaker than
  `((x + level) * 2 + level of play) * 0.3` flees, the others close in, along y if that square is free, else along x. Next to the player it
  attacks: it hits when `rand() % 20 - (shield ? 1 : 0) - stamina + x + level >= 0`, doing
  `rand() % x + level` damage. The thief and the gremlin steal instead 10% of the time: the
  gremlin half the food, the thief one item. The mimic never moves.
- **The quotes and credits** (akaux.cc) scroll one pixel per 100 ms until a key is pressed.
