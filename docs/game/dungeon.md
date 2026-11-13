# The dungeons

Source: akworld.cc (`createdungeon`, `createmonsters`, `drawdungeon`), ak.cc (`rundungeon`,
`main`). Read from the code, not yet checked in play; the generator is reproduced by
`python tools/akmap.py dungeon LUCKY LEVEL [X Y]`.

## A level is made every time it is entered

`main` calls `createdungeon` and `createmonsters` each time the dungeon loop starts: on entering
from the surface, after every ladder, hole or trap, and after loading. Nothing about a level is
remembered: **chests come back, killed monsters come back at full strength and on their
starting squares**. Since a level's seed is `lucky number * 3 + level`, the same level always
looks the same, so going down and back up refills the chests of both levels.

There is no bottom: every level has a way down, and the generator works for any level number.

## The generator (`createdungeon`)

The level is 11 x 11, `dng[x][y]`.

1. `srandom(lucky * 3 + level)`; everything floor.
2. Walls on the border and on every even row and every even column. What is left is a grid of
   5 x 5 one-square rooms at odd x and odd y. The squares where two even lines cross stay walls.
3. Each wall square between two rooms then rolls, in this order, each roll overwriting the one
   before: trap (`DG_TRAP`, 4%), hidden door (`DG_HIDDEN_DOOR`, 29%), door (`DG_DOOR`, 59%),
   hole (`DG_HOLE`, 2%), chest (`DG_CHEST`, 5%). What a gap ends up as:

   | chest | hole | door | hidden door | trap | wall |
   |---|---|---|---|---|---|
   | 5.0% | 1.9% | 54.9% | 11.1% | 1.1% | 26.0% |

4. (2,1), the gap east of the entrance, is always open.
5. The ladders, in rooms: on even levels a ladder down at (7,3) and up at (3,7), on odd levels
   the other way round. A ladder therefore always arrives on the matching ladder of the next
   level. On level 1, (1,1) is the ladder up (the exit) and (7,3) is plain floor.

Example, lucky number 1234, level 2, arriving by the ladder at (3,7):

```
# # # # # # # # # # #
# . . . h . # . + 2 #
# # # h # # # + # h #
# . $ . + 3 + > + . #
# + # + # # # + # + #
# . + . + . h . + . #
# + # + # + # h # + #
# . + @ + . # . + 1 #
# # # + # h # + # # #
# . + . + . $ . + . #
# # # # # # # # # # #
```

`#` wall, `.` floor, `h` hidden door, `+` door, `$` chest, `>` ladder down, `@` the player (on
the ladder up), digits the monsters (1 skeleton, 2 thief). Rows are y, columns x.

## The monsters' placement (`createmonsters`)

Monster *m* (1 skeleton ... 10 balrog, [combat.md](combat.md)) lives on levels deeper than
*m* - 2: skeleton and thief from level 1, giant rat from 2, and so on to the balrog from level 9.
There is one of each kind at most. Each goes on a random floor square that is not the player's
(`random() % 9 + 1` for x and y until one fits) and is written into the map as `m * 10` added to
the square. `random()` continues from the level's seed, so the monsters' squares are fixed too
(for a given square of the player). Their hit points are `2m + 2 * level + level of play`.

## The view (`drawdungeon`)

- A wire-frame corridor drawn from the player's square outwards, one square at a time, until a
  wall, a hidden door or a door ahead ends it. The perspective tables (the constructor) are the
  Apple's 280 x 160 view scaled to 320 x 166.
- A hidden door looks exactly like a wall; a door shows a door frame, ahead or in a side wall.
- A ladder down is a hole in the floor with a ladder, a ladder up a hole in the ceiling with a
  ladder, a hole just the hole in the floor. A chest is a box, with "Chest!" written beside the
  view.
- Monsters are drawn after the monsters' turn; one more than a square away is also named (the
  mimic as "Chest!"). The daemon's picture has three strokes that do not shrink with distance
  (FINDINGS).
- The lines are drawn in palette colour 30 + level, so each level has its own colour.
- The compass word is printed from the facing, oddly assigned: `dx = -1` "North", `dx = 1`
  "South", `dy = -1` "East", `dy = 1` "West". It is only a label; the dungeon has no tie to the
  world map's directions.

## Moving (`rundungeon`)

- Up steps forward, Alt+Left / Alt+Right step sideways, Alt+Down steps back. Only walls and
  squares holding a monster block; doors, **hidden doors**, traps, chests, ladders and holes can
  all be walked onto. Down turns around, Left and Right turn.
- A blocked step just redraws the view (and is still a turn).

After a step (and only after one of the four steps):

- **Trap** (`DG_TRAP`): a falling whistle, `rand() % level + 3` damage, and the player falls to
  the next level, onto the same x, y. The trap is never removed, so stepping on it again drops
  the player again. The square below is a wall gap of the next level, and about one time in four
  that is a wall: the player then stands inside a wall (stepping out works, since moves only
  check the target square).
- **Chest** (`DG_CHEST`): "Gold!", `rand() % (5 * level) + level` gold, and always one item,
  `rand() % 6`: one unit of food (not ten), a rapier, an axe, a shield, a bow or an amulet. The
  chest is removed until the level is made again.

## Climbing (`E`, `K` or `X`)

| square | effect |
|---|---|
| `DG_LADDER_DOWN` or `DG_HOLE` | down one level, same square |
| `DG_LADDER_UP` on level 1 | "Exit Dungeon": back to the surface on the dungeon entrance; the hit points earned in this trip (`lk`, from kills, [combat.md](combat.md)) are added to the hit points |
| `DG_LADDER_UP` | up one level, same square |

The magic amulet's ladder spells put a ladder under the player, which works the same way: a
ladder up on level 1 is an exit ([combat.md](combat.md)).

Saving is refused in a dungeon; Ctrl-R loads PLAYER.U0, which was saved on the surface, so the
player is back on the world map.
