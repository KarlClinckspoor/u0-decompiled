// akdefs.h: named constants for the readable pass (phase 5). Not part of the original source.
//
// Only #defines: they produce neither code nor debug information, so every file can include
// this header without changing a byte (a new enum or type in a header that ak.cc includes would
// add debug symbols). Each name stands for the same int literal as before.

#ifndef AKDEFS_H
#define AKDEFS_H

// The world map, akworld::world[x][y]
#define LAND_PLAINS         0
#define LAND_MOUNTAINS      1       // impassable; the map's border
#define LAND_TREE           2
#define LAND_TOWN           3       // the Adventure Shop
#define LAND_DUNGEON        4
#define LAND_CASTLE         5       // Lord British

// A dungeon square, akworld::dng[x][y]. A monster of type m adds m * DG_MONSTER to the square
// it stands on, so dng / DG_MONSTER is the monster and dng % DG_MONSTER what lies under it.
#define DG_FLOOR            0
#define DG_WALL             1
#define DG_TRAP             2       // never cleared: falls to the next level every time
#define DG_HIDDEN_DOOR      3       // drawn as a plain wall, passable
#define DG_DOOR             4
#define DG_CHEST            5       // gold and an item, cleared when taken
#define DG_LADDER_DOWN      7       // drawn as a hole with a ladder
#define DG_LADDER_UP        8       // the exit on level 1
#define DG_HOLE             9       // a hole in the floor without a ladder: leads down too
#define DG_MONSTER          10

// The monsters (the index of akworld::mz, akworld::ml and every monster[] name table)
#define MON_SKELETON        1
#define MON_THIEF           2       // steals an item 10% of the time
#define MON_GIANT_RAT       3
#define MON_ORC             4
#define MON_VIPER           5
#define MON_CARRION_CRAWLER 6
#define MON_GREMLIN         7       // steals half the food 10% of the time
#define MON_MIMIC           8       // never moves, named "Chest!" in the view
#define MON_DAEMON          9
#define MON_BALROG          10

// The items, charsave::savestruct::owned[]
#define IT_FOOD             0
#define IT_RAPIER           1
#define IT_AXE              2
#define IT_SHIELD           3
#define IT_BOW              4       // "Bow and Arrow"
#define IT_AMULET           5       // "Magic Amulet"

// The stats, charsave::savestruct::character[]
#define ST_HP               0
#define ST_STRENGTH         1
#define ST_DEXTERITY        2
#define ST_STAMINA          3
#define ST_WISDOM           4
#define ST_GOLD             5

// Colours of the VGA's default palette, as textout() uses them
#define CLR_BLUE            1       // once, to blank a line with spaces
#define CLR_LIGHTBLUE      9       // values: numbers, item and monster names
#define CLR_LIGHTGREEN      10      // thefts
#define CLR_LIGHTCYAN       11      // ordinary text
#define CLR_LIGHTRED        12      // alarms: hits, traps, low food or hit points, the menu cursor
#define CLR_LIGHTMAGENTA    13      // headings, the monster's name in the view
#define CLR_WHITE           15      // menu entries

// The results of rungame() and rundungeon(); checkstatus() returns GAME_DEAD, or 1 while the
// character lives
#define GAME_NEXT           0       // the player changed places (dungeon, level) or loaded a game
#define GAME_OVER           1       // Ctrl-Q or Ctrl-E
#define GAME_DEAD           2       // starved or killed: back to the menu

// The main menu's entries, akmenu::runmenu()'s result and main()'s item[]
#define MENU_INTRO          1       // "Introduction": the instructions
#define MENU_CREATE         2       // "Create a Character"
#define MENU_THANKS         3       // "Acknowledgements": the credits
#define MENU_JOURNEY        4       // "Journey Onward": load PLAYER.U0
#define MENU_QUOTES         5       // "View Quotes", with QUOTES.FLG
#define MENU_ENDING         6       // "View Ending", with ENDGAME.FLG

// Keys. readkey() returns the scan code in the high byte, the ASCII code in the low byte.
// Allegro's KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT and KEY_ENTER are the plain scan codes.
#define SC_ALT_LEFT         155     // the BIOS codes of Alt with an arrow key
#define SC_ALT_RIGHT        157
#define SC_ALT_DOWN         160
#define CTRL_E              5
#define CTRL_Q              17
#define CTRL_R              18
#define CTRL_S              19
#define ASCII_ESC           27

#endif
