// akworld.h: the world map, the dungeon and its monsters (class layout from ak.cc's debug
// symbols, docs/MAP.md). The base class shows in the debug information as a member named
// "charsave" at offset 0.

#ifndef AKWORLD_H
#define AKWORLD_H

class akworld : public charsave
{
public:
    int world[21][21];              // the world map [x][y], LAND_* (akdefs.h)
    int dng[11][11];                // the dungeon level [x][y], DG_* plus DG_MONSTER * monster
    int mdraw;                      // 1 while drawdungeon should draw the monsters
    int color;                      // the dungeon's drawing colour: level + 30
    int run1;                       // 1 when a monster fled this turn: redraw the view
    BITMAP *worldbmp;               // the view: the top 320x166 pixels of the screen
    MIDI *backmid;                  // the music playing
    int per[11][4];                 // the perspective tables, see the constructor
    int cd[11][4];
    int lad[11][4];
    int ld[11][6];
    int ft[11][6];
    int xx[11], yy[11];
    int mz[11][2], ml[11][2];       // per monster: alive, hit points; its square x, y

    akworld();
    ~akworld();
    int createworld(int mode, charsave::savestruct &player);
    int drawworld(charsave::savestruct &player);
    int createdungeon(charsave::savestruct &player);
    int drawdungeon(charsave::savestruct &player);
    int createmonsters(charsave::savestruct &player);
    int drawmonsters(int type, int dist);
    int attack(charsave::savestruct &player);
    int movemonsters(charsave::savestruct &player);
    int sgn(int n);
    int viewintro(charsave::savestruct &player);
};

#endif
