// akchar.cc: the character: creation, the Adventure Shop, saving and loading, Lord British,
// the per-turn upkeep and death. Compiled -O0 (docs/MATCHING.md).
//
// The function order matters beyond each function: gcc 2.7.2.1 decides whether a function
// gets two dead jumps before its epilogue from a global that the previous functions leave
// behind (docs/MATCHING.md, "Dead jumps at the end of a function").

#include <fstream.h>
#include <allegro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <conio.h>
#include "ak.h"
#include "akchar.h"

charsave::charsave()                                    // 004F9C
{
    count = 0;
}

charsave::~charsave()                                   // 004FB4
{
}

// Character creation: the lucky number seeds the random numbers, the stats are rolled as in the
// Apple original, INT(SQR(RND(1))*21+4).
void charsave::charcreate()                             // 005134
{
    int i = 0;
    int key = 0;
    char num[4];
    char *input;            // never set: cin writes through an uninitialised pointer
    char *stat[] = {"Hit Points.....", "Strength.......", "Dexterity......",
                    "Stamina........", "Wisdom.........", "Gold..........."};
    remove_keyboard();
    clear(screen);
    textout(screen, font, "Type thy Lucky Number.....", 8, 40, 11);
    gotoxy(28, 6);
    cin >> input;
    sav.ln = atoi(input);
    srandom(sav.ln);
    textout(screen, font, "Level of play (1-10)......", 8, 56, 11);
    gotoxy(28, 8);
    cin >> input;
    sav.level = atoi(input);
    while (sav.level <= 0 | sav.level > 10)
    {
        textout(screen, font, "Invalid Choice!", 8, 64, 11);
        rest(1500);
        textout(screen, font, "                 ", 0, 64, 1);
        textout(screen, font, "Level of play (1-10)......       ", 8, 56, 11);
        gotoxy(28, 8);
        cin >> input;
        sav.level = atoi(input);
    }
    install_keyboard();
    while (key != 'y' & key != 'Y')
    {
        for (i = 0; i <= 5; i++)
        {
            sav.character[i] = floor(sqrt(random() % 100 * .01) * 21 + 4);
            textout(screen, font, stat[i], 8, i * 8 + 80, 11);
            itoa(sav.character[i], num, 10);
            strcat(num, "  ");
            textout(screen, font, num, 128, i * 8 + 80, 11);
        }
        textout(screen, font, "Shallt thou play with these qualities?", 0, 136, 11);
        gotoxy(20, 18);
        key = readkey() & 0xff;
    }
    key = 0;
    while (key != 'f' & key != 'F' & key != 'm' & key != 'M')
    {
        textout(screen, font, "And Shallt thou be a Fighter or a Mage?", 0, 154, 11);
        gotoxy(20, 19);
        key = readkey() & 0xff;
        if (key <= 'Z') key += 32;
        if (key == 'f') sav.ChClass = 'F';
        if (key == 'm') sav.ChClass = 'M';
    }
    for (i = 0; i <= 5; i++)
        sav.owned[i] = 0;
    sav.inout = 0;
    sav.task = 0;
    sav.visit = 0;
    sav.px = 1;
    sav.py = 1;
    sav.dx = 1;
    sav.dy = 0;
}

// The Adventure Shop. With mode -1 it only shows the stats and items and waits for a key.
// A mage is told he cannot use the rapier or the bow, but the purchase goes through anyway:
// the flag `ok` is cleared and never looked at (the test after the switch reads `key`).
int charsave::shop(int mode)                            // 0056B8
{
    int i;
    int key = mode;
    int ok = 0;
    char num[24];
    int price = 0;
    int item = 0;
    char *stat[] = {"Hit Points.....", "Strength.......", "Dexterity......",
                    "Stamina........", "Wisdom.........", "Gold..........."};
    char *weapon[] = {"Food", "Rapier", "Axe", "Shield", "Bow and Arrow", "Magic Amulet"};
    clear(screen);
    textout(screen, font, "Stat's", 48, 16, 13);
    textout(screen, font, "Weapons", 168, 16, 13);
    for (i = 0; i <= 5; i++)
    {
        textout(screen, font, stat[i], 0, i * 8 + 32, 11);
        itoa(sav.character[i], num, 10);
        textout(screen, font, num, 120, i * 8 + 32, 9);
        itoa(sav.owned[i], num, 10);
        if (sav.owned[i] <= 9)
            textout(screen, font, num, 192, i * 8 + 32, 9);
        if (sav.owned[i] > 9 & sav.owned[i] <= 99)
            textout(screen, font, num, 184, i * 8 + 32, 9);
        if (sav.owned[i] > 99 & sav.owned[i] <= 999)
            textout(screen, font, num, 176, i * 8 + 32, 9);
        if (sav.owned[i] > 999 & sav.owned[i] <= 9999)
            textout(screen, font, num, 168, i * 8 + 32, 9);
        textout(screen, font, "-", 200, i * 8 + 32, 9);
        textout(screen, font, weapon[i], 208, i * 8 + 32, 11);
    }
    textout(screen, font, "Q-Quit", 144, 80, 9);
    textout(screen, font, "Price", 40, 128, 13);
    textout(screen, font, "Damage", 120, 128, 13);
    textout(screen, font, "Item", 200, 128, 13);
    textcolor(11);
    for (i = 0; i <= 5; i++)
        textout(screen, font, weapon[i], 200, i * 8 + 144, 11);
    textout(screen, font, "1 For 10", 40, 144, 11);
    textout(screen, font, "N/A", 120, 144, 11);
    textout(screen, font, "8", 40, 152, 11);
    textout(screen, font, "1-10", 120, 152, 11);
    textout(screen, font, "5", 40, 160, 11);
    textout(screen, font, "1-5", 120, 160, 11);
    textout(screen, font, "6", 40, 168, 11);
    textout(screen, font, "1", 120, 168, 11);
    textout(screen, font, "3", 40, 176, 11);
    textout(screen, font, "1-4", 120, 176, 11);
    textout(screen, font, "15", 40, 184, 11);
    textout(screen, font, "?????", 120, 184, 11);
    gotoxy(1, 13);
    if (mode != -1)
    {
        textout(screen, font, "Welcome to the Adventure Shop", 8, 96, 11);
        textout(screen, font, "Which item shallt thou buy", 8, 104, 11);
    }
    else
        readkey();
again:
    while (key != -1)
    {
        key = readkey() & 0xff;
        switch (key)
        {
        case 'F': case 'f':
            item = 0;
            price = 1;
            break;
        case 'R': case 'r':
            item = 1;
            price = 8;
            if (sav.ChClass == 'M')
            {
                textout(screen, font, "I'm sorry Mages can't use that!", 8, 96, 11);
                ok = 0;
            }
            break;
        case 'A': case 'a':
            item = 2;
            price = 5;
            break;
        case 'S': case 's':
            item = 3;
            price = 6;
            break;
        case 'B': case 'b':
            item = 4;
            price = 3;
            if (sav.ChClass == 'M')
            {
                textout(screen, font, "I'm sorry Mages can't use that!", 8, 96, 11);
                ok = 0;
            }
            break;
        case 'M': case 'm':
            item = 5;
            price = 15;
            break;
        case 'Q': case 'q':
            return 0;
            break;
        default:
            item = 0;
            price = 0;
            textout(screen, font, "I'm Sorry We Don't have that.   ", 8, 96, 11);
            goto again;
        }
        if (sav.character[5] - price >= 0)
        {
            if (key != 0)
            {
                if (item == 0)
                    sav.owned[item] = sav.owned[item] + 10;
                else
                    sav.owned[item] = sav.owned[item] + 1;
                sav.character[5] -= price;
                itoa(sav.owned[item], num, 10);
                if (sav.owned[item] <= 9)
                    textout(screen, font, num, 192, item * 8 + 32, 9);
                if (sav.owned[item] > 9 & sav.owned[item] <= 99)
                    textout(screen, font, num, 184, item * 8 + 32, 9);
                if (sav.owned[item] > 99 & sav.owned[item] <= 999)
                    textout(screen, font, num, 176, item * 8 + 32, 9);
                if (sav.owned[item] > 999 & sav.owned[item] <= 9999)
                    textout(screen, font, num, 168, item * 8 + 32, 9);
                itoa(sav.character[5], num, 10);
                textout(screen, font, "   ", 120, 72, 9);
                textout(screen, font, num, 120, 72, 9);
                textout(screen, font, "Thank you m'lord.               ", 8, 96, 11);
            }
        }
        else
            textout(screen, font, "M'Lord thou can not afford that item.", 8, 96, 11);
    }
    return 0;
}

// Q: writes the save record to PLAYER.U0.
int charsave::savegame()                                // 00610C
{
    FILE *f;
    if ((f = fopen("player.u0", "wb")) == NULL)
    {
        textout(screen, font, "Save Game Failed!", 8, 175, 9);
        fprintf(stderr, "Error: Can not open player.u0\n");
        return 1;
    }
    fwrite(&sav, sizeof(sav), 1, f);
    fclose(f);
    textout(screen, font, "Save Game Complete!", 8, 175, 9);
    return 0;
}

// ^R: reads it back.
int charsave::loadgame()                                // 0061C4
{
    FILE *f;
    if ((f = fopen("player.u0", "rb")) == NULL)
    {
        fprintf(stderr, "Error: Can not open player.u0\n");
        return 1;
    }
    fseek(f, 0, 0);
    fread(&sav, sizeof(sav), 1, f);
    fclose(f);
    return 0;
}

// Lord British. The speeches come from AK.DAT (AKLB1-3): one line per ';', a line holding '`' is
// left out (or would be: the else only sets a flag that is already set), '~' ends a page and
// ')' marks where the quest's monster is named. On the first visit he asks the name and gives
// a quest (wisdom / 3, at most 10); later he checks it. Returning at the first '~' leaves the
// datafile loaded.
int charsave::lb()                                      // 0064CC
{
    int row = 0;
    int skip = 0;
    int key;
    char num[4];
    char *line;
    char *monster[] = {"", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper",
                       "Carrion Crawler", "Gremlin", "Mimic", "Daemon", "Balrog"};
    DATAFILE *dat;
    clear(screen);
    dat = load_datafile("ak.dat");
    if (dat == NULL)
    {
        allegro_exit();
        printf("Error loading ak.dat!\r\n");
        printf("Please reinstall Akalabeth\r\n\n");
        exit(1);
    }
    if (sav.visit == 0)
    {
        char *text = (char *)dat[AKLB1].dat;
        char **rest = &text;
        sav.visit = 1;
        skip = 0;
        while ((line = strsep(rest, ";")) != NULL)
        {
            if (strchr(line, '`') == NULL)
            {
                if (skip == 0)
                    textout(screen, font, line, 0, row++ * 8, 11);
                else
                    skip = 1;
            }
        }
        remove_keyboard();
        textout(screen, font, "What is thy name peasant? ", 0, 64, 11);
        gotoxy(27, 9);
        cin >> sav.name;
        textout(screen, font, "Doest thou wish for grand adventure? ", 0, 80, 11);
        install_keyboard();
        key = readkey() & 0xff;
        if (key != 'y' & key != 'Y')
        {
            textout(screen, font, "Then Leave and Begone!", 0, 96, 11);
            sav.visit = 0;
        }
        else
        {
            sav.task = floor(sav.character[4] / 3);
            if (sav.task > 10) sav.task = 10;
            for (row = 0; row <= 5; row++)
                sav.character[row] = sav.character[row] + 1;
            char *text = (char *)dat[AKLB2].dat;
            char **rest = &text;
            skip = 0;
            row = 0;
            while ((line = strsep(rest, ";")) != NULL)
            {
                if (strchr(line, '~') != NULL)
                {
                    textout_centre(screen, font, "Press any Key to Continue", 160, (row + 1) * 8 + 80, 11);
                    readkey();
                    clear(screen);
                    row = 0;
                    return 0;
                }
                if (strchr(line, ')') == NULL)
                    textout(screen, font, monster[sav.task], 104, 136, 9);
                if (strchr(line, '`') == NULL)
                {
                    if (skip == 0)
                        textout(screen, font, line, 0, row++ * 8 + 80, 11);
                    else
                        skip = 1;
                }
            }
        }
    }
    else
    {
        if (sav.task > 0)
        {
            textout(screen, font, "Why hast thou returned?", 0, 24, 11);
            textout(screen, font, "Thou must kill a(n) ", 0, 32, 11);
            textout(screen, font, monster[sav.task], 160, 32, 9);
            textout(screen, font, "Go now and complete thy quest!", 0, 48, 11);
        }
        else
        {
            for (row = 0; row <= 5; row++)
                sav.character[row] = sav.character[row] + 1;
            textout(screen, font, "Aahh.....", 0, 24, 11);
            textout(screen, font, sav.name, 72, 24, 11);
            textout(screen, font, "Thou hast accomplished thy quest!", 0, 40, 11);
            if (abs(sav.task) == 10)
            {
                char *text = (char *)dat[AKLB3].dat;
                char **rest = &text;
                row = 0;
                while ((line = strsep(rest, ";")) != NULL)
                {
                    if (strchr(line, '`') == NULL)
                    {
                        if (skip == 0)
                            textout(screen, font, line, 0, row++ * 8 + 48, 11);
                        else
                            skip = 1;
                    }
                }
                if (sav.level > 9)
                {
                    textout(screen, font, "...Report thy Amazing feat to  ", 0, 96, 11);
                    textout(screen, font, "Lord British today!", 0, 104, 11);
                    ofstream quotes("quotes.flg");
                    ofstream endgame("endgame.flg");
                }
                else
                {
                    textout(screen, font, "   Now maybe thou art foolhearty", 0, 96, 11);
                    textout(screen, font, "enough to try difficulty level ", 0, 104, 11);
                    itoa(sav.level + 1, num, 10);
                    textout(screen, font, num, 256, 96, 9);
                }
            }
            else
            {
                textout(screen, font, "Unfortunately, this is not enough to", 0, 56, 11);
                textout(screen, font, "become a Kinght.", 0, 64, 11);
                sav.task = abs(sav.task) + 1;
                textout(screen, font, "Now Thou Must Kill a(n) ", 0, 72, 11);
                textout(screen, font, monster[sav.task], 192, 72, 9);
            }
        }
    }
    readkey();
    unload_datafile(dat);
}

// Called every turn: food (every turn on the surface, every 5th in a dungeon), starvation and
// death, and the status lines. Returns 2 when the player died, else 1.
int charsave::checkstatus()                             // 006C98
{
    char num[8];
    count++;
    if (sav.inout == 0)
    {
        sav.owned[0]--;
        count = 0;
    }
    if (sav.inout > 0 & count % 5 == 0)
    {
        sav.owned[0]--;
        count = 0;
    }
    if (sav.owned[0] <= 0)
    {
        textout(screen, font, "Thou Hast Starved!!!!!", 8, 175, 12);
        death();
        return 2;
    }
    if (sav.character[0] <= 0)
    {
        death();
        return 2;
    }
    textout(screen, font, "             ", 225, 167, 9);
    textout(screen, font, "Food=", 225, 175, 11);
    itoa(sav.owned[0], num, 10);
    textout(screen, font, "    ", 265, 175, 9);
    if (sav.owned[0] <= 10)
        textout(screen, font, num, 265, 175, 12);
    else
        textout(screen, font, num, 265, 175, 9);
    textout(screen, font, "H.P.=", 225, 183, 11);
    textout(screen, font, "    ", 265, 183, 9);
    itoa(sav.character[0], num, 10);
    if (sav.character[0] <= 10)
        textout(screen, font, num, 265, 183, 12);
    else
        textout(screen, font, num, 265, 183, 9);
    textout(screen, font, "Gold=", 225, 191, 11);
    itoa(sav.character[5], num, 10);
    textout(screen, font, "    ", 265, 191, 9);
    textout(screen, font, num, 265, 191, 9);
    return 1;
}

void charsave::death()                                  // 0070DC
{
    textout_centre(screen, font, "         We mourn the passing of         ", 159, 167, 11);
    textout_centre(screen, font, "       the Peasant and his Computer      ", 159, 175, 11);
    textout_centre(screen, font, "   To invoke a miracle of ressurection   ", 159, 183, 11);
    textout_centre(screen, font, "                Press a Key              ", 159, 191, 11);
    readkey();
}
