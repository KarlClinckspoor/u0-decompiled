# Monsters and combat

Source: akworld.cc (`attack`, `movemonsters`, `createmonsters`). Read from the code, not yet
checked in play. *L* is the dungeon level, *P* the level of play, *m* the monster's number.
All combat rolls use `rand()` ([random.md](random.md)).

## The monsters

| *m* | monster | from level | hit points | damage per hit | special |
|---|---|---|---|---|---|
| 1 | Skeleton | 1 | 2 + 2*L* + *P* | *L* | |
| 2 | Thief | 1 | 4 + 2*L* + *P* | 0-1, + *L* | 10% of its attacks steal an item instead |
| 3 | Giant Rat | 2 | 6 + 2*L* + *P* | 0-2, + *L* | |
| 4 | Orc | 3 | 8 + 2*L* + *P* | 0-3, + *L* | |
| 5 | Viper | 4 | 10 + 2*L* + *P* | 0-4, + *L* | |
| 6 | Carrion Crawler | 5 | 12 + 2*L* + *P* | 0-5, + *L* | drawn hanging from the ceiling |
| 7 | Gremlin | 6 | 14 + 2*L* + *P* | 0-6, + *L* | 10% of its attacks halve the food instead |
| 8 | Mimic | 7 | 16 + 2*L* + *P* | 0-7, + *L* | never moves; looks like a chest and is named "Chest!" |
| 9 | Daemon | 8 | 18 + 2*L* + *P* | 0-8, + *L* | |
| 10 | Balrog | 9 | 20 + 2*L* + *P* | 0-9, + *L* | |

One of each kind at most per level, placed when the level is made ([dungeon.md](dungeon.md)).
The level of play only adds to the hit points (and so moves the fleeing point, below).

## The player's attack (`A`)

"Attack! Which Weapon?", then:

| key | weapon | needs | damage | reach |
|---|---|---|---|---|
| `R` | Rapier | a rapier, a fighter | `rand() % 10` + strength / 5 | the square ahead |
| `A` | Axe | an axe | `rand() % 5` + strength / 5 | swing: the square ahead; `T` throws it: range 5, one axe used up |
| `S` | Shield | a shield | `rand() % 1` (always 0) + strength / 5 | the square ahead |
| `B` | Bow and Arrow | a bow, a fighter | `rand() % 4` + strength / 5 | range 5; arrows are not counted |
| `M` | Magic Amulet | an amulet | see below | |
| other | Hands | | strength / 5 | the square ahead |

A weapon the player doesn't own ("Not Owned!") or can't use as a mage asks again. The shield
does exactly what bare hands do; its use is defensive (below). The shop's damage column
(1-10, 1-5, 1, 1-4) is one higher than the code's `rand()` range.

**To hit**: the attack misses when there is no monster, or when
`dexterity - rand() % 25 < m + L`. So the chance is `(dexterity - m - L + 1) / 25`, between 0
and 1: with dexterity 20, an orc on level 3 is hit 56% of the time.

**Ranged attacks** (bow, thrown axe, magic attack) look 1 to 5 squares ahead and take the first
monster found. They **pass through walls and doors**: only the map's edge stops them. The edge
test has a typo (FINDINGS): facing `dy = -1` (the compass says "East") it fires at once, so a
ranged attack in that direction always misses, however close the monster.

**A hit** takes the damage off the monster's hit points and shows what is left. At 0 or below
the monster dies: it is removed from the map, "Thou hast killed a ...", and the player gets
`m + L` gold at once and `m * L / 2` (rounded down) hit points in the dungeon account `lk`, paid
out when leaving the dungeon by the level 1 exit. Dying in the dungeon loses the account. If
the monster was the quest's, the quest is done (two rising tones, [lordbritish.md](lordbritish.md)).

## The magic amulet (`A`, then `M`)

A fighter gets a random spell (1-4, each 25%); a mage picks one ("1-Ladder-Up", "2-Ladder-Dn",
"3-Kill", "4-Bad??"; Esc goes back). Before the spell, 24% of the time it is the amulet's last
charge: one amulet is used up (the spell still works).

| spell | effect |
|---|---|
| 1 Ladder Up | the square under the player becomes a ladder up; on level 1 that is an exit |
| 2 Ladder Down | the square becomes a ladder down |
| 3 Magic Attack | a ranged attack doing `rand() % (L + 10)` + strength / 5 |
| 4 Bad?? | one of three, equally likely: **Toad**: strength, dexterity, stamina and wisdom become 3. **Lizard Man**: hit points, strength, dexterity, stamina and wisdom are multiplied by 2.5 (a windfall). **Backfire**: the hit points are halved |

The amulet is the only way to change level without finding a ladder, and the Lizard Man is the
biggest stat gain in the game.

## The monsters' turn (`movemonsters`)

After every key in the dungeon (attacks included), each living monster in order 1 to 10:

1. **Fleeing**: a monster whose hit points are below `0.3 * (2m + 2L + P)`, i.e. 30% of what it
   started with, does not attack.
2. **Attack**: a monster that doesn't flee and stands next to the player (north, south, east or
   west, not diagonally) attacks: "You are being attacked by a ...". It hits when
   `rand() % 20 - shield - stamina + m + L >= 0` (shield: 1 if the player owns one), so the
   chance is `(20 - stamina - shield + m + L) / 20`, between 0 and 1. A hit does
   `rand() % m + L` damage. With stamina 18 and a shield, a skeleton on level 1 hits 15% of the
   time; a balrog on level 9 always hits.
   - A thief or gremlin attacks like this 90% of the time. Otherwise the gremlin steals half
     the food (rounded down), and the thief one item of a random kind the player has (one food
     unit counts: a thief taking the last food starves the player at the end of the turn).
3. **Moving**: any other monster (not next to the player, or fleeing) steps one square towards
   the player, or away when fleeing (the view is then redrawn at once). It tries along y first
   and moves there if that square holds no wall, trap or monster; otherwise it tries along x.
   Monsters never move diagonally, walk through doors and hidden doors, onto chests, ladders and
   holes, and never into the player. **The mimic never moves**, but attacks when the player
   comes next to it.

A monster that comes next to the player attacks only on the following turn.
