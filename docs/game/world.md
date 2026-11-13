# The world map

Source: akworld.cc (`createworld`, `drawworld`), ak.cc (`rungame`). Read from the code, not yet
checked in play; the generator is reproduced by `python tools/akmap.py world LUCKY`.

## How it is made (`createworld`)

The map is 21 x 21 squares, `world[x][y]`, x growing east and y growing south (the up arrow is
"North" and decrements y).

1. `srandom(lucky number)`.
2. The border (x or y = 0 or 20) is mountains.
3. Each inner square, column by column (x outer, y inner), is `floor(RND^5 * 4.5)` with
   `RND = random() % 100 / 100`:

   | value | land | chance |
   |---|---|---|
   | 0 | `LAND_PLAINS` | 75% |
   | 1 | `LAND_MOUNTAINS` | 11% |
   | 2 | `LAND_TREE` | 7% |
   | 3 | `LAND_TOWN` | 5% |
   | 4 | `LAND_DUNGEON` | 2% |

   A second test after it was meant to thin out the towns but sets a town to a town (FINDINGS);
   it calls `rand()`, not `random()`, so it doesn't change the map.
4. One `LAND_CASTLE` at a random square, x then y, each `floor(RND * 19 + 1)`.
5. A new game puts the player at a random square and makes it a town. A loaded game puts a town
   at a random square without moving the player; the random numbers come in the same order, so
   it is **the same square**. A loaded game therefore has exactly the map of its new game.

Each of the 361 inner squares is rolled independently, so nothing guarantees a castle or a
dungeon can be reached: mountains may wall them off, and the castle or the start town may land
on (and replace) anything, even each other (then there is no castle).

Example, lucky number 1234 (`tools/akmap.py world 1234`): 269 plains, 34 mountains, 19 trees,
26 towns, 12 dungeons and the castle; the player starts at x=1, y=19.

## Moving (`rungame`)

- The view (`drawworld`) shows only the 3 x 3 squares around the player, each 58 x 52 pixels,
  with a cross on the player. There is no overview map.
- The arrows move one square. Mountains block: "You cant pass the Mountains!" with a beep. The
  border is all mountains, so the player can never leave the map.
- Trees and plains are just ground; there are no encounters on the surface.
- Every key is a turn and eats a unit of food ([README.md](README.md)).

## Places

`E` or `X` acts on the square under the player:

| land | what happens |
|---|---|
| `LAND_TOWN` | the Adventure Shop ([character.md](character.md)), with shop.mid |
| `LAND_CASTLE` | Lord British ([lordbritish.md](lordbritish.md)), with lordbrit.mid |
| `LAND_DUNGEON` | "Enter Dungeon": level 1 at square (1,1), facing `dx = 1` (shown as "South"); the dungeon's hit point account `lk` starts at 0 ([dungeon.md](dungeon.md)) |

Saving (`Q`, Ctrl-S) works anywhere on the surface. Ctrl-R loads PLAYER.U0 but keeps the
world map in memory, so loading another character's save mid-game puts them on this world.
