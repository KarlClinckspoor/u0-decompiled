// akworld.h: the world map, the dungeon and its monsters (class layout from ak.cc's debug
// symbols, docs/MAP.md). The base class shows in the debug information as a member named
// "charsave" at offset 0.

#ifndef AKWORLD_H
#define AKWORLD_H

class akworld : public charsave
{
public:
    int world[21][21];              // the world map: 1 mountain, 3 town, 4 dungeon, 5 castle
    int dng[11][11];                // the dungeon level: 1 wall, 2 trap, 5 gold, 7/9 ladder down, 8 up
    int mdraw;                      // 1 while drawdungeon should draw the monsters
    int color;
    int run1;
    BITMAP *worldbmp;
    MIDI *backmid;                  // the music playing
    int per[11][4];
    int cd[11][4];
    int lad[11][4];
    int ld[11][6];
    int ft[11][6];
    int xx[11], yy[11];
    int mz[11][2], ml[11][2];

    akworld();
    ~akworld();
    void createworld(int mode, charsave::savestruct &sav);
    void drawworld(charsave::savestruct &sav);
    void createdungeon(charsave::savestruct &sav);
    void drawdungeon(charsave::savestruct &sav);
    void createmonsters(charsave::savestruct &sav);
    void drawmonsters(int a, int b);
    void attack(charsave::savestruct &sav);
    void movemonsters(charsave::savestruct &sav);
    int sgn(int n);
    void viewintro(charsave::savestruct &sav);
};

#endif
