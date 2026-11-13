// akchar.h: the character and its save record (layout from ak.cc's debug symbols, docs/MAP.md).
// The member names are those of Garriott's Applesoft program.

#ifndef AKCHAR_H
#define AKCHAR_H

class charsave
{
public:
    struct savestruct               // written to PLAYER.U0 as it is (124 bytes)
    {
        int character[6];           // hit points, strength, dexterity, stamina, wisdom, gold
        int owned[6];               // food, rapier, axe, shield, bow and arrow, magic amulet
        char name[24];
        char ChClass;               // 'F' fighter or 'M' mage
        int ln;                     // the lucky number (the random seed)
        int level;                  // level of play, 1-10
        int tx, ty;                 // position on the world map
        int px, py;                 // position in the dungeon
        int dx, dy;                 // facing in the dungeon
        int inout;                  // 0 on the surface, else the dungeon level
        int task;                   // Lord British's quest: the monster to kill (negative: done)
        int visit;                  // 1 once Lord British gave a quest (View Ending sets 2)
        int lk;                     // hit points earned by kills, paid out on leaving the dungeon
    } sav;
    int count;                      // turns, for food in the dungeon (not saved)

    charsave();
    ~charsave();
    void charcreate();
    int shop(int mode);
    int savegame();
    int loadgame();
    int lb();
    int checkstatus();
    void death();
};

#endif
