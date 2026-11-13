// akmenu.h: the main menu (class layout from ak.cc's debug symbols, docs/MAP.md)

#ifndef AKMENU_H
#define AKMENU_H

class akmenu
{
public:
    int t1;     // 1 when QUOTES.FLG exists: the menu offers "View Quotes"
    int t2;     // 1 when ENDGAME.FLG exists: the menu offers "View Ending"
    int pos;    // highlighted entry, 1 = first
    int max;    // number of entries: 4, plus one for each flag file

    akmenu();
    ~akmenu();
    int runmenu(char **item);
    int draw(char **item);
    int redraw(char **item);
};

#endif
