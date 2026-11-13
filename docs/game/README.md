# Akalabeth (PC, 1997/98): how the game works

Written in phase 6 from the C++ that rebuilds AK.EXE byte for byte (`src/`), so every rule here
is what the shipped program does. Names in `CAPITALS` are the constants of `src/akdefs.h`;
functions are named as in the source.

**Status: read from the code, not yet checked in play.** The generators (stats, world map,
dungeon levels) are reproduced by `tools/akmap.py`, whose random numbers were checked against
DJGPP 2.01's own libc compiled in DOSBox; the maps themselves have not been compared with the
running game yet. docs/FINDINGS.md lists the bugs, each to be marked "checked in play" once seen.

| page | what |
|---|---|
| [character.md](character.md) | character creation, the stats, fighters and mages, the level of play, the Adventure Shop |
| [world.md](world.md) | the world map: how it is made, moving, towns, the castle, dungeon entrances |
| [dungeon.md](dungeon.md) | the dungeon levels: the generator, the 3-D view, moving, traps, chests, ladders |
| [combat.md](combat.md) | the monsters, the player's attack, the weapons, the magic amulet, the monsters' turn |
| [lordbritish.md](lordbritish.md) | Lord British's quests, the ending, the menu's flag files |
| [random.md](random.md) | the two random number generators and what each one decides |
| [../FORMATS.md](../FORMATS.md) | PLAYER.U0, AK.DAT, SOUND.CFG, QUOTES.FLG and ENDGAME.FLG |

## The program's flow (ak.cc `main`)

1. Allegro starts, the sound system is set up from SOUND.CFG (failing that, the game tells the
   player to run SETUP.EXE and quits), the screen goes to 320x200 in 256 colours.
2. `akaux` checks that AK.DAT loads, then "Ready?": `N` quits.
3. **The menu** (`akmenu::runmenu`): Introduction (the instructions from AK.DAT), Create a
   Character, Acknowledgements (scrolling credits), Journey Onward (load PLAYER.U0), and, when
   their flag files exist, View Quotes and View Ending ([lordbritish.md](lordbritish.md)).
4. Create a Character runs character creation, then the shop, then makes a new world. Journey
   Onward loads PLAYER.U0 (without one it goes to character creation) and makes the world again
   from the saved lucky number.
5. **The game loop** alternates between the world map (`rungame`) and the dungeon
   (`rundungeon`). Each runs until the player changes place (enters or leaves a dungeon,
   changes level, loads a game), dies, or quits:
   - a death goes back to the menu; PLAYER.U0 is not touched, so Journey Onward goes on from
     the last save;
   - Ctrl-Q or Ctrl-E ends the program at once, without saving.
6. Every time the dungeon loop starts, the current level and its monsters are made anew
   ([dungeon.md](dungeon.md)).

## Turns

There is no clock. **Every key press is a turn**, whatever it did: a move, a blocked move, the
stats screen, an unknown key. After each key:

- on the world map: the key's action, then `checkstatus` (food, death, the status lines);
- in the dungeon: the key's action; if the player moved, the trap or chest under them; the
  compass; the monsters' turn (`movemonsters`); the view redrawn with the monsters; then
  `checkstatus`.

`checkstatus` also runs once each time `rungame` or `rundungeon` starts, so entering a dungeon,
changing level or coming back to the surface costs a turn too.

### Food and death (akchar.cc `checkstatus`)

- On the surface every turn eats one unit of food; in a dungeon every fifth turn does (a turn
  counter, `count`, that is not saved and restarts on the surface).
- The food is taken first, then checked: **food at 0 is starvation**, so the last unit can never
  be used. Hit points at 0 or below are death. Either way: "We mourn the passing of the Peasant
  and his Computer", a key, and back to the menu.
- The status lines show food, hit points and gold; food or hit points of 10 or less are red.

## Controls

| where | key | does |
|---|---|---|
| world | arrows | walk north, east, south, west; mountains block (with a beep) |
| world | `E` or `X` | enter what is under the player: a town (the shop), the castle (Lord British), a dungeon |
| world | `Q` or Ctrl-S | save to PLAYER.U0 |
| dungeon | Up | step forward |
| dungeon | Down | turn around |
| dungeon | Left / Right | turn left / right |
| dungeon | Alt+Left / Alt+Right | step sideways without turning |
| dungeon | Alt+Down | step back without turning |
| dungeon | `E`, `K` or `X` | climb: a ladder up or down, or down a hole |
| dungeon | `A` | attack, then the weapon: `R` `A` `S` `B` `M`, anything else is bare hands ([combat.md](combat.md)) |
| dungeon | space | wait (redraws the view) |
| dungeon | `Q` or Ctrl-S | "Can Not Save in Dungeons!" |
| both | `Z` | the stats and inventory screen (costs a turn) |
| both | Ctrl-R | load PLAYER.U0 (the world map stays the one in memory) |
| both | Ctrl-Q or Ctrl-E | quit to DOS without saving |
