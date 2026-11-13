# Random numbers

The game uses two unrelated generators from DJGPP 2.01's libc. Which one a roll uses decides
whether it can be predicted from the lucky number.

## `random()`: the world, seeded by the lucky number

`srandom()`/`random()` (libc `compat/stdlib/random.c`) is the BSD additive generator (degree
31, separation 3). The game seeds it three ways:

| seed | where | then rolls |
|---|---|---|
| `lucky` | `charcreate` | the stats, six per offer |
| `lucky` | `createworld` | the 361 inner squares, the castle, the start town |
| `lucky * 3 + level` | `createdungeon` | the wall gaps, then (`createmonsters`) the monsters' squares |

So the character's stat offers, the world map and every dungeon level are fixed by the lucky
number alone (and a level's monsters by where the player stands). `tools/akmap.py` reproduces
all of it. Its `random()` was checked against the real libc: a test program compiled with
gcc 2.7.2.1 in DOSBox printed the same sequence, the same stat rolls and the same terrain rolls
(2026-11-13).

The rolls are written `random() % 100 * .01`, i.e. hundredths, where the Apple's `RND(1)` was a
fraction with many more steps. The floating-point formulas on top (`pow`, `sqrt`, `floor`) never
land close enough to a whole number for the x87's extended precision to matter: for the terrain,
the boundaries `(k / 4.5)^(1/5)` lie between hundredths, and for positions `k * 0.19` is a whole
number only at k = 0.

## `rand()`: combat and luck, never seeded

`rand()` (libc `ansi/stdlib/rand.c`) is a 48-bit linear congruential generator,
`next = next * 0x5DEECE66D + 11`, returning bits 16-46. **The game never calls `srand()`**, so
`rand()` starts from the same state at every launch (its very first value is 0). It decides:

- in combat: hits and misses, damage, stealing, the fighter's amulet spell, the amulet's last
  charge, the bad spell's kind;
- traps' damage, a chest's gold and item;
- and it is called once per inner square by `createworld`'s town test, so making a world moves
  it 361 steps.

The sequence depends only on how many `rand()` calls came before since the program started. A
game replayed with the same keys from the launch (for example in a recorded DOSBox session)
gets the same fights. Two launches that differ only in their history before a fight get
different results.

## Where the Apple original differs (not yet checked)

Applesoft BASIC has a single generator, `RND`, so Garriott's program must draw everything from
one sequence; the port splits the rolls between two generators and leaves the combat half
unseeded. How the original seeds `RND` from the lucky number, and how its rolls, rounding and
order compare with the port's, is still open: it needs the Applesoft listing (docs/HANDOFF.md).
