# Lord British, the quests and the ending

Source: akchar.cc (`lb`), akmenu.cc, ak.cc (`main`). Read from the code, not yet checked in
play. The speeches are AK.DAT's objects AKLB1-3 ([../FORMATS.md](../FORMATS.md)).

## The quest chain

The castle (`LAND_CASTLE`, `E` on the world map) is the only place quests come from. The quest
is the number of a monster, `task` (1 skeleton ... 10 balrog, [combat.md](combat.md)); killing
that kind of monster anywhere makes it negative ("done").

**First visit** (`visit` 0):
1. The welcome speech (AKLB1), "What is thy name peasant?" (typed with `cin`, up to 23
   characters fit), "Doest thou wish for grand adventure?".
2. Anything but `Y`: "Then Leave and Begone!", and the next visit is a first visit again.
3. `Y`: the quest becomes wisdom / 3 (at most 10), **then** all six stats go up by 1, and the
   second speech (AKLB2) names the monster. With rolled wisdom (4-24) the first quest is the
   skeleton to the mimic:

   | wisdom | 4-5 | 6-8 | 9-11 | 12-14 | 15-17 | 18-20 | 21-23 | 24 |
   |---|---|---|---|---|---|---|---|---|
   | first quest | skeleton | thief | giant rat | orc | viper | carrion crawler | gremlin | mimic |

**Later visits**:
- Quest not done: "Why hast thou returned? Thou must kill a(n) ...".
- Quest done: all six stats go up by 1, "Aahh..... *name* Thou hast accomplished thy quest!",
  then:
  - below the balrog: "Unfortunately, this is not enough to become a Kinght." and the next
    quest is the next monster up (`abs(task) + 1`);
  - after the balrog: the knighthood speech (AKLB3). At level of play 10, "Report thy Amazing
    feat to Lord British today!" and the game writes the empty files QUOTES.FLG and ENDGAME.FLG
    (in the current directory). Below level 10, Lord British suggests trying the next level of
    play, which means a new character.

So a character with wisdom *w* has `11 - w / 3` quests to do, each paid with +1 to every stat
(hit points and gold included). After the balrog the quest stays "done" for good, so **every
later visit to the castle adds 1 to all six stats again**, as often as the player likes (each
visit costs only the turns to walk out and back in).

Hit points, the chain's only real limit, come from leaving the dungeon with kills
([combat.md](combat.md)) and from these visits.

## The menu's extra entries (akmenu.cc)

The menu checks for the two flag files each time it is shown:

| file | entry | what it does |
|---|---|---|
| QUOTES.FLG | View Quotes | the team's quotes scroll by (akaux.cc `quotes`), any key stops them |
| ENDGAME.FLG | View Ending | the character in memory is given level of play 10 and a finished balrog quest, and Lord British's last visit plays (stats +1, AKLB3, the flag files written again) |

Both files are written together, so normally both entries appear. With only ENDGAME.FLG (the
other deleted by hand), the fifth entry reads "View Ending" but choosing it shows the quotes:
`runmenu` returns the position, 5, and `main` treats 5 as View Quotes (FINDINGS).

## Texts in AK.DAT

The speeches are plain text, one line per `;`. A line holding `~` ends a page (wait for a key),
a line holding `` ` `` is left out. In the second speech the monster's name goes after "killing
a(n)": the code prints it at that fixed place for every line *without* a `)`, which skips just
the line holding "a(n)" and draws the name eleven times on the same spot instead of once.
