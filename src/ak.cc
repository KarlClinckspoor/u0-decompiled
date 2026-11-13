// ak.cc: the game's loops: the world map (rungame), the dungeon (rundungeon) and main with the
// menu. Compiled -O0 -g (docs/MATCHING.md): the debug information records the line of every
// statement, so the line layout of this file is the original's and must stay as it is.

#include <stdlib.h>
#include <string.h>
#include <iostream.h>
#include <math.h>
#include <allegro.h>
#include "akaux.h"
#include "akchar.h"
#include "akmenu.h"
#include "akworld.h"

// One move on the world map per key: the arrows walk (mountains block), E enters the town,
// castle or dungeon under the player, Q or Ctrl-S saves, Ctrl-R loads, Z shows the stats.
// Returns 0 when the player enters a dungeon or loads a game, 1 when Ctrl-Q or Ctrl-E ends
// the game and 2 when the character has died.
// player is the character, w the world (an akworld holds a second charsave as its base, which
// the game never uses).
int rungame(charsave &player, akworld &w)
{
    int key = 0;
    int ky1 = 0, ky2 = 0;
    int i = 0;
    int result;
    char string[10];
    if (player.checkstatus() == 2) return 2;
    w.backmid = load_midi("over.mid");
    play_midi(w.backmid, -1);

    // Ctrl-Q and Ctrl-E end the game
    while (ky2 != 17 & ky2 != 5)
    {
        i++;
        key = readkey();
        ky1 = key >> 8;
        ky2 = key & 0xff;

        textout(screen, font, "                           ", 8, 175, 9);
        textout(screen, font, "                    ", 8, 183, 9);

        switch (ky1) {
        case 72:                                // up
            if (w.world[player.sav.tx][player.sav.ty - 1] == 1)
            {
                sound(100);
                rest(50);
                sound(0);
                textout(screen, font, "You cant pass the Mountains!", 8, 175, 11);
            }
            else
            {
                player.sav.ty = player.sav.ty - 1;
                w.drawworld(player.sav);
                textout(screen, font, "North       ", 8, 167, 11);
            }
            break;
        case 77:                                // right
            if (w.world[player.sav.tx + 1][player.sav.ty] == 1)
            {
                sound(100);
                rest(50);
                sound(0);
                textout(screen, font, "You cant pass the Mountains!", 8, 175, 11);
            }
            else
            {
                player.sav.tx = player.sav.tx + 1;
                w.drawworld(player.sav);
                textout(screen, font, "East        ", 8, 167, 11);
            }
            break;
        case 80:                                // down
            if (w.world[player.sav.tx][player.sav.ty + 1] == 1)
            {
                sound(100);
                rest(50);
                sound(0);
                textout(screen, font, "You cant pass the Mountains!", 8, 175, 11);
            }
            else
            {
                player.sav.ty = player.sav.ty + 1;
                w.drawworld(player.sav);
                textout(screen, font, "South       ", 8, 167, 11);
            }
            break;
        case 75:                                // left
            if (w.world[player.sav.tx - 1][player.sav.ty] == 1)
            {
                sound(100);
                rest(50);
                sound(0);
                textout(screen, font, "You cant pass the Mountains!", 8, 175, 11);
            }
            else
            {
                player.sav.tx = player.sav.tx - 1;
                w.drawworld(player.sav);
                textout(screen, font, "West        ", 8, 167, 11);
            }
            break;
        }
        switch (ky2) {
        case 'X':
        case 'x':
        case 'E':
        case 'e':
            if (w.world[player.sav.tx][player.sav.ty] == 3)     // a town: the shop
            {
                destroy_midi(w.backmid);
                w.backmid = load_midi("shop.mid");
                play_midi(w.backmid, -1);
                player.shop(0);
                clear(screen);
                destroy_midi(w.backmid);
                w.backmid = load_midi("over.mid");
                play_midi(w.backmid, -1);
                w.drawworld(player.sav);
            }
            if (w.world[player.sav.tx][player.sav.ty] == 5)     // Lord British's castle
            {
                destroy_midi(w.backmid);
                w.backmid = load_midi("lordbrit.mid");
                play_midi(w.backmid, -1);
                player.lb();
                destroy_midi(w.backmid);
                w.backmid = load_midi("over.mid");
                play_midi(w.backmid, -1);
                w.drawworld(player.sav);
            }
            if (w.world[player.sav.tx][player.sav.ty] == 4)     // a dungeon
            {
                textout(screen, font, "Enter Dungeon", 8, 167, 11);
                player.sav.inout = 1;
                player.sav.dx = 1;
                player.sav.dy = 0;
                player.sav.px = 1;
                player.sav.py = 1;
                player.sav.lk = 0;
                return 0;
            }
            break;
        case 'Q':
        case 'q':
        case 19:                                // Ctrl-S
            player.savegame();
            break;
        case 18:                                // Ctrl-R
            player.loadgame();
            return 0;
        case 'Z':
        case 'z':
            player.shop(-1);
            clear(screen);
            w.drawworld(player.sav);
            break;
        }
        result = player.checkstatus();
        if (result == 2) return 2;
    }
    return 1;
}

// One move in the dungeon per key: arrows, Alt+Left/Right strafe, Alt+Down retreats. Returns 0
// when the player leaves the level (ladder, trap) or loads a game, 1 at the end, 2 on death.
int rundungeon(charsave &player, akworld &w)
{
    int i = 0;
    int gold;
    int key = 0;
    int ky1 = 0, ky2 = 0;
    int result;
    char string[4];

    char *item[] = {"Food", "Rapier", "Axe", "Shield", "Bow and Arrow", "Magic Amulet"};


    if (player.checkstatus() == 2) return 2;
    w.backmid = load_midi("dungeon.mid");
    play_midi(w.backmid, -1);
    w.mdraw = 0;

    while (ky2 != 17 & ky2 != 5)
    {
        i++;
        key = readkey();
        ky1 = key >> 8;
        ky2 = key & 0xff;

        textout(screen, font, "                           ", 8, 175, 9);
        textout(screen, font, "                           ", 8, 183, 9);
        textout(screen, font, "                           ", 8, 191, 9);


        switch (ky1) {
        case 72:                                // forward
            if (w.dng[player.sav.px + player.sav.dx][player.sav.py + player.sav.dy] != 1 & w.dng[player.sav.px + player.sav.dx][player.sav.py + player.sav.dy] < 10)
            {
                player.sav.px = player.sav.px + player.sav.dx;
                player.sav.py = player.sav.py + player.sav.dy;
                w.drawdungeon(player.sav);
                textout(screen, font, "Forward      ", 8, 167, 11);
            }
            else
                w.drawdungeon(player.sav);
            break;
        case 80: player.sav.dx = -player.sav.dx;        // turn around
            player.sav.dy = -player.sav.dy;
            w.drawdungeon(player.sav);
            textout(screen, font, "Turn Around  ", 8, 167, 11);
            break;
        case 75: if (player.sav.dx != 0)                // turn left
            {
                player.sav.dy = -player.sav.dx;
                player.sav.dx = 0;
            }
            else
            {
                player.sav.dx = player.sav.dy;
                player.sav.dy = 0;
            }
            w.drawdungeon(player.sav);
            textout(screen, font, "Turn Left    ", 8, 167, 11);
            break;
        case 77: if (player.sav.dx != 0)                // turn right
            {
                player.sav.dy = player.sav.dx;
                player.sav.dx = 0;
            }
            else
            {
                player.sav.dx = -player.sav.dy;
                player.sav.dy = 0;
            }
            w.drawdungeon(player.sav);
            textout(screen, font, "Turn Right   ", 8, 167, 11);
            break;
        case 155:                               // Alt+Left: strafe left
            if (w.dng[player.sav.px + player.sav.dy][player.sav.py - player.sav.dx] != 1 & w.dng[player.sav.px + player.sav.dy][player.sav.py - player.sav.dx] < 10)
            {
                player.sav.px = player.sav.px + player.sav.dy;
                player.sav.py = player.sav.py - player.sav.dx;
                w.drawdungeon(player.sav);
                textout(screen, font, "Strafe Left ", 8, 167, 11);
            }
            else
                w.drawdungeon(player.sav);
            break;
        case 157:                               // Alt+Right: strafe right
            if (w.dng[player.sav.px - player.sav.dy][player.sav.py + player.sav.dx] != 1 & w.dng[player.sav.px - player.sav.dy][player.sav.py + player.sav.dx] < 10)
            {
                player.sav.px = player.sav.px - player.sav.dy;
                player.sav.py = player.sav.py + player.sav.dx;
                w.drawdungeon(player.sav);
                textout(screen, font, "Strafe Right", 8, 167, 11);
            }
            else
                w.drawdungeon(player.sav);
            break;
        case 160:                               // Alt+Down: retreat
            if (w.dng[player.sav.px - player.sav.dx][player.sav.py - player.sav.dy] != 1 & w.dng[player.sav.px - player.sav.dx][player.sav.py - player.sav.dy] < 10)
            {
                player.sav.px = player.sav.px - player.sav.dx;
                player.sav.py = player.sav.py - player.sav.dy;
                w.drawdungeon(player.sav);
                textout(screen, font, "Retreat     ", 8, 167, 11);
            }
            else
                w.drawdungeon(player.sav);
            break;
        }
        switch (ky2) {
        case 'X':
        case 'x':
        case 'K':
        case 'k':
        case 'E':
        case 'e':
            if (w.dng[player.sav.px][player.sav.py] == 7 | w.dng[player.sav.px][player.sav.py] == 9)
            {
                player.sav.inout++;
                textout(screen, font, "Ladder Down  ", 8, 167, 11);
                textout(screen, font, "You are on Level ", 8, 175, 11);
                itoa(player.sav.inout, string, 10);
                textout(screen, font, string, 144, 175, 11);
                return 0;
            }
            if (w.dng[player.sav.px][player.sav.py] == 8 & player.sav.inout == 1)
            {
                player.sav.inout = 0;
                player.sav.character[0] = player.sav.character[0] + player.sav.lk;

                textout(screen, font, "Exit Dungeon ", 8, 167, 11);
                return 0;
            }
            if (w.dng[player.sav.px][player.sav.py] == 8)
            {
                player.sav.inout--;
                textout(screen, font, "Ladder Up    ", 8, 167, 11);
                textout(screen, font, "You are on Level ", 8, 175, 11);
                itoa(player.sav.inout, string, 10);
                textout(screen, font, string, 144, 175, 11);
                return 0;
            }
            break;
        case 'A':
        case 'a':
            w.attack(player.sav);
            break;
        case 'Z':
        case 'z':
            player.shop(-1);
            clear(screen);
            w.drawdungeon(player.sav);
            break;
        case 'Q':
        case 'q':
        case 19:                                // Ctrl-S
            textout(screen, font, "Can Not Save in Dungeons!", 8, 175, 9);
            break;
        case 18:                                // Ctrl-R
            player.loadgame();
            return 0;
        // The debug information has no statement on the 13 lines from here to case 'y'.












        case 'y': break;
        case ' ': w.drawdungeon(player.sav);
            break;
        }
        if (ky1 == 72 | ky1 == 157 | ky1 == 155 | ky1 == 160)   // the player moved
        {
            if (w.dng[player.sav.px][player.sav.py] == 2)       // a trap
            {
                for (i = 2000; i > 999; i -= 5)
                {
                    sound(i);
                    rest(4);
                }
                sound(0);
                player.sav.character[0] = player.sav.character[0] - (rand() % player.sav.inout + 3);
                player.sav.inout++;
                w.drawdungeon(player.sav);
                textout(screen, font, "AAARRRGGGHHH!!!! A Trap!", 8, 175, 12);
                textout(screen, font, "Falling to Level ", 8, 183, 11);
                itoa(player.sav.inout, string, 10);
                textout(screen, font, string, 144, 183, 9);
                return 0;
            }
            if (w.dng[player.sav.px][player.sav.py] == 5)       // gold
            {
                w.dng[player.sav.px][player.sav.py] = 0;
                w.drawdungeon(player.sav);
                sound(2000);
                rest(500);
                sound(0);
                textout(screen, font, "Gold!         ", 8, 167, 11);
                gold = floor(rand() % (player.sav.inout * 5) + player.sav.inout);
                itoa(gold, string, 10);
                textout(screen, font, string, 8, 175, 11);
                textout(screen, font, "-Pieces of Eight", strlen(string) * 8 + 8, 175, 11);
                player.sav.character[5] = player.sav.character[5] + gold;
                if (gold > 0)
                {
                    gold = floor(rand() % 6);
                    textout(screen, font, "and a ", 8, 183, 11);
                    textout(screen, font, item[gold], 56, 183, 9);
                    player.sav.owned[gold]++;
                }
            }
        }
        if (player.sav.dx == -1)
            textout(screen, font, "North", 140, 167, 11);
        if (player.sav.dx == 1)
            textout(screen, font, "South", 140, 167, 11);
        if (player.sav.dy == -1)
            textout(screen, font, "East ", 140, 167, 11);
        if (player.sav.dy == 1)
            textout(screen, font, "West ", 140, 167, 11);
        w.movemonsters(player.sav);
        w.mdraw = 1;
        w.drawdungeon(player.sav);
        w.mdraw = 0;
        result = player.checkstatus();
        if (result == 2) return 2;
    }
    return 1;
}

// The menu, then the world map and the dungeons in turn until the game ends; a death goes back
// to the menu. argv[0] tells Allegro where to find SETUP.EXE's sound configuration.
int main(int argc, char *argv[])
{
    int result = 0, r2 = 0;
    char *item[] = {"", "   Introduction", "Create a Character", " Acknowledgements", "  Journey Onward", "    View Quotes", "    View Ending"};


    akaux aux;
    charsave player;

    allegro_init();
    install_keyboard();

    install_timer();


    if (install_sound(DIGI_NONE, MIDI_AUTODETECT, argv[0]) != 0)
    {
        allegro_exit();
        cout << "Error Initializing Sound System!\n";
        cout << "Please Run SETUP.EXE to Reconfigure your Sound Card\n";
        exit(1);
    }

    set_gfx_mode(GFX_VGA, 320, 200, 0, 0);
    akworld w;

    aux.hello();

    akmenu menu;

start:
    player.sav.px = 1;
    player.sav.py = 1;
    player.sav.dx = 1;
    player.sav.dy = 0;

    result = 0;
    r2 = 0;

    while (result != 2 & result != 4)
    {
        w.backmid = load_midi("intro.mid");
        play_midi(w.backmid, -1);
        result = menu.runmenu(item);
        switch (result) {
        case 1:
            aux.instruct();
            break;
        case 3:
            destroy_midi(w.backmid);
            aux.shoutouts();
            break;
        case 4:
            r2 = player.loadgame();

            if (r2 == 1) result = 2;
            break;
        case 5:
            destroy_midi(w.backmid);
            aux.quotes();
            break;
        case 6:                                 // View Ending: Lord British's last speech
            clear(screen);
            player.sav.visit = 2;
            player.sav.task = -10;
            player.sav.level = 10;
            w.backmid = load_midi("lordbrit.mid");
            play_midi(w.backmid, -1);
            player.lb();
            break;
        }
    }
    if (result == 2)                            // Create a Character
    {
        player.charcreate();
        w.backmid = load_midi("shop.mid");
        play_midi(w.backmid, -1);
        player.shop(0);
        w.createworld(0, player.sav);
    }
    else
        w.createworld(1, player.sav);
    clear(screen);

    result = 0;
    while (result == 0)
    {
        if (player.sav.inout == 0)
        {
            destroy_midi(w.backmid);
            w.drawworld(player.sav);
            result = rungame(player, w);
            destroy_midi(w.backmid);
        }
        else
        {
            w.createdungeon(player.sav);
            w.createmonsters(player.sav);
            w.mdraw = 0;
            w.drawdungeon(player.sav);
            result = rundungeon(player, w);
            destroy_midi(w.backmid);
        }
        if (result == 2) goto start;
    }
    text_mode(0);
}
