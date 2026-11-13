# The character

Source: akchar.cc (`charcreate`, `shop`, `checkstatus`), akchar.h (`savestruct`). Read from
the code, not yet checked in play.

## Creation (`charcreate`)

1. **"Type thy Lucky Number"**: any number. It seeds `random()` (`srandom`). The same number
   later seeds the world map and, with the level, every dungeon level ([random.md](random.md)),
   so a lucky number is a whole game.
2. **"Level of play (1-10)"**: anything outside 1-10 is refused. It only changes three things:
   the monsters' hit points, the point at which they flee ([combat.md](combat.md)), and
   whether finishing the last quest unlocks the ending ([lordbritish.md](lordbritish.md)).
3. **The stats** are rolled and shown; any key but `Y` rolls six new ones from the same random
   sequence. `python tools/akmap.py stats LUCKY 5` lists the first five offers.
4. **"Fighter or a Mage?"**: `F` or `M`.

The character starts with nothing in the inventory, then goes straight to the shop.

Both lucky number and level are read with `cin` into a pointer that is never set (FINDINGS); in
practice the stack value it holds pointed somewhere harmless.

## The stats (`character[]`)

| index | stat | used for |
|---|---|---|
| `ST_HP` | Hit Points | death at 0; raised by leaving a dungeon with kills to show for it |
| `ST_STRENGTH` | Strength | damage: every hit adds strength / 5 |
| `ST_DEXTERITY` | Dexterity | the player's chance to hit |
| `ST_STAMINA` | Stamina | the monsters' chance to miss |
| `ST_WISDOM` | Wisdom | the first quest: monster wisdom / 3 |
| `ST_GOLD` | Gold | money |

Each roll is `floor(sqrt(random() % 100 * .01) * 21 + 4)`: Garriott's `INT(SQR(RND(1))*21+4)`
with only 100 steps. Values run from 4 to 24 with high values likelier (mean 17.4), and **5 can
never be rolled** (it would need `random() % 100` between 0.23 and 0.9):

| value | 4 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| chance % | 1 | 2 | 1 | 2 | 3 | 3 | 3 | 4 | 4 | 5 | 5 | 6 | 6 | 7 | 7 | 7 | 8 | 8 | 9 | 9 |

Stats change later only through Lord British (+1 to all six per quest step), the magic amulet
(toad, lizard man, backfire), damage, gold and the hit points paid out on leaving a dungeon.

## Fighters and mages

| | fighter | mage |
|---|---|---|
| rapier, bow | can fight with them | can buy them (the shop's refusal does not stop the sale, FINDINGS) but not use them: "Mages can't use Rapiers!" |
| magic amulet | the spell is random, including "Bad??" | chooses the spell (1-4, Esc goes back) |

Nothing else differs: same stats, same hit chances, same damage.

## The Adventure Shop (`shop`)

Reached from character creation and from every town (`LAND_TOWN`). Keys are the items'
initials; `Q` leaves. Prices and damage as the shop's table shows them, with what the code does:

| key | item | price | table says | damage in the code ([combat.md](combat.md)) |
|---|---|---|---|---|
| `F` | Food | 1 gold for 10 | N/A | |
| `R` | Rapier | 8 | 1-10 | 0-9, + strength / 5 |
| `A` | Axe | 5 | 1-5 | 0-4, + strength / 5; can be thrown (range 5, lost) |
| `S` | Shield | 6 | 1 | always 0, + strength / 5: the same as bare hands; it makes monsters miss more |
| `B` | Bow and Arrow | 3 | 1-4 | 0-3, + strength / 5; range 5, never used up |
| `M` | Magic Amulet | 15 | ????? | spells; each use has a 24% chance of being its last |

- Every item is a count: buying another axe or amulet adds one.
- Anything else: "I'm Sorry We Don't have that." Too little gold: "M'Lord thou can not afford
  that item."
- The shop screen with mode -1 (`Z`) only shows the stats and inventory.
- Numbers above 9999 are not printed (the right-aligned columns stop there).
