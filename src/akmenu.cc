// akmenu.cc: the main menu. Compiled -O0 (docs/MATCHING.md).
//
// item[1..4] are the four fixed entries, item[5] "View Quotes" and item[6] "View Ending"; the
// two extra entries appear only when QUOTES.FLG / ENDGAME.FLG exist (WHATS.NEW). The entries
// are drawn from y = 128 (row 16) in steps of 8, the highlighted one in colour 12, the others
// in colour 15. Note the bitwise & between comparisons: that is how the original was written.
//
// fstream.h comes before allegro.h: gcc numbers labels across the whole file, headers included,
// and only this order gives the original's label numbers (L332 for the textout copy).

#include <fstream.h>
#include <allegro.h>
#include "akmenu.h"

akmenu::akmenu()                                        // 0071E4
{
}

akmenu::~akmenu()                                       // 0071F0
{
}

// Shows the menu and lets the player move with the arrow keys until Enter; returns the entry.
int akmenu::runmenu(char **item)                        // 007240
{
    t1 = 0;
    t2 = 0;
    clear(screen);
    textout(screen, font, "Ultima 0 - Akalabeth", 72, 64, 11);
    ifstream quotes("quotes.flg");
    ifstream endgame("endgame.flg");
    if (quotes) t1 = 1;
    if (endgame) t2 = 1;
    pos = 1;
    int key = 0;
    max = 4 + t1 + t2;
    draw(item);
    redraw(item);
    while (key != KEY_ENTER)
    {
        key = readkey() >> 8;
        switch (key)
        {
        case KEY_UP:
            if (pos != 1) { pos--; redraw(item); }
            break;
        case KEY_DOWN:
            if (pos != max) { pos++; redraw(item); }
            break;
        }
    }
    return pos;
}

// Draws every entry in the normal colour.
int akmenu::draw(char **item)                           // 007404
{
    int i;
    for (i = 1; i <= 4; i++)
        textout(screen, font, item[i], 80, (i + 15) * 8, 15);
    if (t1 == 1 & t2 != 1)
        textout(screen, font, item[5], 80, 160, 15);
    if (t1 != 1 & t2 == 1)
        textout(screen, font, item[6], 80, 160, 15);
    if (t1 == 1 & t2 == 1)
    {
        textout(screen, font, item[5], 80, 160, 15);
        textout(screen, font, item[6], 80, 168, 15);
    }
    return 0;
}

// Redraws the highlighted entry and its neighbours. When only ENDGAME.FLG exists, entry 5 on
// screen is item[6].
int akmenu::redraw(char **item)                         // 007550
{
    if (pos == 1)
    {
        textout(screen, font, item[pos + 1], 80, (pos + 16) * 8, 15);
        textout(screen, font, item[pos], 80, (pos + 15) * 8, 12);
    }
    else if (pos == max)
    {
        if (max != 5)
        {
            textout(screen, font, item[pos - 1], 80, (pos + 14) * 8, 15);
            textout(screen, font, item[pos], 80, (pos + 15) * 8, 12);
        }
        else
        {
            textout(screen, font, item[pos - 1], 80, (pos + 14) * 8, 15);
            if (t1 != 1)
                textout(screen, font, item[6], 80, (pos + 15) * 8, 12);
            else
                textout(screen, font, item[pos], 80, (pos + 15) * 8, 12);
        }
    }
    else
    {
        if (t1 == 0 & t2 != 1)
        {
            textout(screen, font, item[pos + 1], 80, (pos + 16) * 8, 15);
            textout(screen, font, item[pos - 1], 80, (pos + 14) * 8, 15);
            textout(screen, font, item[pos], 80, (pos + 15) * 8, 12);
        }
        else
        {
            if (pos + 1 == 5 & t1 != 1)
                textout(screen, font, item[6], 80, (pos + 16) * 8, 15);
            else
                textout(screen, font, item[pos + 1], 80, (pos + 16) * 8, 15);
            if (pos - 1 == 5 & t1 != 1)
                textout(screen, font, item[6], 80, (pos + 14) * 8, 15);
            else
                textout(screen, font, item[pos - 1], 80, (pos + 14) * 8, 15);
            if (pos == 5 & t1 != 1)
                textout(screen, font, item[6], 80, (pos + 15) * 8, 12);
            else
                textout(screen, font, item[pos], 80, (pos + 15) * 8, 12);
        }
    }
    return 0;
}
