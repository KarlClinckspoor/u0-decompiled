// akworld.cc: the world map, the dungeon, its monsters and combat. Compiled -O0
// (docs/MATCHING.md). Most of it follows Garriott's Applesoft program line by line: the
// perspective tables of the constructor, the map and dungeon generators, the monster pictures.

#include <stdlib.h>
#include <string.h>
#include <iostream.h>
#include <math.h>
#include <allegro.h>
#include "akchar.h"
#include "akworld.h"

// The perspective tables for the 3-D dungeon view. xx/yy are the half-width and half-height
// of the corridor at each distance (the Apple's 280x160 view scaled to 320x166); per, cd, ld,
// ft and lad are the corners of walls, doors, ladders and so on at each distance.
akworld::akworld()
{
    int w;
    int x;
    int y;

    worldbmp = create_sub_bitmap(screen, 0, 0, 320, 166);
    xx[0] = 159;
    yy[0] = 83;
    for (x = 1; x <= 10; x++)
    {
        xx[x] = floor(atan(1 / (x * 2.0)) / .785398 * 140 + .5);
        yy[x] = floor(xx[x] * 4.0 / 7);
        xx[x] = floor(xx[x] / 279.0 * 320);
        yy[x] = floor(yy[x] / 159.0 * 166);
        per[x][0] = 159 - xx[x];
        per[x][1] = 159 + xx[x];
        per[x][2] = 83 - yy[x];
        per[x][3] = 83 + yy[x];
    }
    per[0][0] = 0;
    per[0][1] = 319;
    per[0][2] = 0;
    per[0][3] = 166;
    for (x = 1; x <= 10; x++)
    {
        cd[x][0] = 159 - (int)(xx[x] / 3.0);
        cd[x][1] = 159 + (int)(xx[x] / 3.0);
        cd[x][2] = 83 - (int)(yy[x] * .7);
        cd[x][3] = 83 + yy[x];
    }
    for (x = 0; x <= 9; x++)
    {
        ld[x][0] = (per[x][0] * 2 + per[x + 1][0]) / 3;
        ld[x][1] = (per[x][0] + 2 * per[x + 1][0]) / 3;
        w = ld[x][0] - per[x][0];
        ld[x][2] = per[x][2] + w * 4 / 7;
        ld[x][3] = per[x][2] + 2 * w * 4 / 7;
        ld[x][4] = (per[x][3] * 2 + per[x + 1][3]) / 3;
        ld[x][5] = (per[x][3] + 2 * per[x + 1][3]) / 3;
        ld[x][2] = ld[x][4] - (ld[x][4] - ld[x][2]) * .8;
        ld[x][3] = ld[x][5] - (ld[x][5] - ld[x][3]) * .8;
        if (ld[x][3] == ld[x][4]) ld[x][3] = ld[x][3] - 1;
    }
    for (x = 0; x <= 9; x++)
    {
        ft[x][0] = 159 - xx[x] / 3;
        ft[x][1] = 159 + xx[x] / 3;
        ft[x][2] = 159 - xx[x + 1] / 3;
        ft[x][3] = 159 + xx[x + 1] / 3;
        ft[x][4] = 83 + (yy[x] * 2 + yy[x + 1]) / 3;
        ft[x][5] = 83 + (yy[x] + 2 * yy[x + 1]) / 3;
    }
    for (x = 0; x <= 9; x++)
    {
        lad[x][0] = (ft[x][0] * 2 + ft[x][1]) / 3;
        lad[x][1] = (ft[x][0] + 2 * ft[x][1]) / 3;
        lad[x][3] = ft[x][4];
        lad[x][2] = 166 - lad[x][3];
    }
}

akworld::~akworld()
{
}

// The world map: a wall of mountains, then random terrain, 0 plains to 4 (rarely) a dungeon;
// one castle. mode 0 is a new game: the player starts on a town at a random place.
int akworld::createworld(int mode, charsave::savestruct &player)
{
    int x;
    int y;

    clear(screen);
    srandom(player.ln);
    textout(screen, font, "Welcome to Akalabeth, World of Doom!", 16, 96, 11);
    textout(screen, font, "(Please Wait)", 24, 184, 11);
    for (x = 0; x <= 20; x++)
    {
        world[x][0] = 1;
        world[0][x] = 1;
        world[x][20] = 1;
        world[20][x] = 1;
    }
    for (x = 1; x <= 19; x++)
        for (y = 1; y <= 19; y++)
        {
            world[x][y] = floor(pow(random() % 100 * .01, 5) * 4.5);
            if (world[x][y] == 3 & rand() % 100 > 50) world[x][y] = 3;
        }
    world[(int)floor(random() % 100 * .01 * 19 + 1)][(int)floor(random() % 100 * .01 * 19 + 1)] = 5;
    if (mode == 0)
    {
        player.tx = floor(random() % 100 * .01 * 19 + 1);
        player.ty = floor(random() % 100 * .01 * 19 + 1);
        world[player.tx][player.ty] = 3;
    }
    else
        world[(int)floor(random() % 100 * .01 * 19 + 1)][(int)floor(random() % 100 * .01 * 19 + 1)] = 3;
    return 0;
}

// The 3x3 squares around the player, 58x52 pixels each: 1 mountains, 2 a tree, 3 a town,
// 4 a dungeon entrance, 5 the castle. On the surface a cross marks the player.
int akworld::drawworld(charsave::savestruct &player)
{
    int x;
    int y;
    int m;
    int x1;
    int y1;

    clear(worldbmp);
    for (y = -1; y <= 1; y++)
        for (x = -1; x <= 1; x++)
        {
            if (player.inout == 0)
            {
                line(worldbmp, 140, 78, 144, 78, 11);
                line(worldbmp, 142, 76, 142, 80, 11);
            }
            m = world[player.tx + x][player.ty + y];
            x1 = (x + 1) * 58 + 55;
            y1 = (y + 1) * 52;
            switch (m) {
            case 1:
                line(worldbmp, x1 + 12, y1 + 53, x1 + 12, y1 + 42, 11);
                line(worldbmp, x1 + 12, y1 + 42, x1 + 23, y1 + 31, 11);
                line(worldbmp, x1 + 23, y1 + 31, x1 + 46, y1 + 31, 11);
                line(worldbmp, x1 + 46, y1 + 31, x1 + 46, y1 + 53, 11);
                line(worldbmp, x1, y1 + 10, x1 + 12, y1 + 10, 11);
                line(worldbmp, x1 + 58, y1 + 10, x1 + 46, y1 + 10, 11);
                line(worldbmp, x1, y1 + 42, x1 + 12, y1 + 42, 11);
                line(worldbmp, x1 + 46, y1 + 42, x1 + 58, y1 + 42, 11);
                line(worldbmp, x1 + 12, y1, x1 + 12, y1 + 21, 11);
                line(worldbmp, x1 + 12, y1 + 21, x1 + 23, y1 + 21, 11);
                line(worldbmp, x1 + 23, y1 + 21, x1 + 23, y1 + 31, 11);
                line(worldbmp, x1 + 23, y1 + 31, x1 + 35, y1 + 31, 11);
                line(worldbmp, x1 + 35, y1 + 31, x1 + 35, y1 + 11, 11);
                line(worldbmp, x1 + 35, y1 + 10, x1 + 46, y1 + 10, 11);
                line(worldbmp, x1 + 46, y1 + 10, x1 + 46, y1, 11);
                break;
            case 2:
                rect(worldbmp, x1 + 23, y1 + 21, x1 + 35, y1 + 31, 11);
                break;
            case 3:
                rect(worldbmp, x1 + 11, y1 + 10, x1 + 23, y1 + 21, 11);
                rect(worldbmp, x1 + 35, y1 + 10, x1 + 46, y1 + 21, 11);
                rect(worldbmp, x1 + 23, y1 + 21, x1 + 35, y1 + 31, 11);
                rect(worldbmp, x1 + 11, y1 + 31, x1 + 23, y1 + 43, 11);
                rect(worldbmp, x1 + 35, y1 + 31, x1 + 46, y1 + 43, 11);
                break;
            case 4:
                line(worldbmp, x1 + 23, y1 + 21, x1 + 34, y1 + 31, 11);
                line(worldbmp, x1 + 23, y1 + 31, x1 + 34, y1 + 21, 11);
                break;
            case 5:
                rect(worldbmp, x1, y1, x1 + 58, y1 + 53, 11);
                rect(worldbmp, x1 + 12, y1 + 10, x1 + 46, y1 + 42, 11);
                line(worldbmp, x1 + 12, y1 + 10, x1 + 46, y1 + 42, 11);
                line(worldbmp, x1 + 46, y1 + 10, x1 + 12, y1 + 42, 11);
                break;
            }
        }
    return 0;
}

// A dungeon level: the same level of the same game is always the same (the seed is the lucky
// number and the level). 1 wall, 2 trap, 3 hidden door, 4 door, 5 chest of gold, 7/9 ladder
// down, 8 ladder up.
int akworld::createdungeon(charsave::savestruct &player)
{
    int x;
    int y;

    srandom(player.ln * 3 + player.inout);
    for (x = 0; x <= 10; x++)
        for (y = 0; y <= 10; y++)
            dng[x][y] = 0;
    for (x = 0; x <= 10; x++)
    {
        dng[x][0] = 1;
        dng[x][10] = 1;
        dng[0][x] = 1;
        dng[10][x] = 1;
    }
    for (x = 2; x <= 8; x += 2)
        for (y = 1; y <= 9; y++)
        {
            dng[x][y] = 1;
            dng[y][x] = 1;
        }
    for (x = 2; x <= 8; x += 2)
        for (y = 1; y <= 9; y += 2)
        {
            if (random() % 100 > 95) dng[x][y] = 2;
            if (random() % 100 > 95) dng[y][x] = 2;
            if (random() % 100 > 70) dng[x][y] = 3;
            if (random() % 100 > 70) dng[y][x] = 3;
            if (random() % 100 > 40) dng[x][y] = 4;
            if (random() % 100 > 40) dng[y][x] = 4;
            if (random() % 100 > 97) dng[y][x] = 9;
            if (random() % 100 > 97) dng[x][y] = 9;
            if (random() % 100 > 94) dng[x][y] = 5;
            if (random() % 100 > 94) dng[y][x] = 5;
        }
    dng[2][1] = 0;
    if (player.inout / 2.0 == floor(player.inout / 2.0))
    {
        dng[7][3] = 7;
        dng[3][7] = 8;
    }
    else
    {
        dng[7][3] = 8;
        dng[3][7] = 7;
    }
    if (player.inout == 1)
    {
        dng[1][1] = 8;
        dng[7][3] = 0;
    }
    return 0;
}

// The 3-D view of the dungeon, drawn from the player's square outwards, one distance l at a
// time, until a wall or a door ends it (en is never set: the returns end the loop). cent, left
// and right are the squares ahead and beside at distance l; l1/r1/t1/b1 the near corners of
// the corridor there and l2/r2/t2/b2 the far ones. With mdraw set the monsters are drawn too.
int akworld::drawdungeon(charsave::savestruct &player)
{
    int bl, tl, ll, rl;
    int y1, y2, y3, y4;
    int cent, left, right;
    int l1, l2, r1, r2, b1, b2, t1, t2;
    int mon;
    int en = 0;
    int l = 0;

    color = player.inout + 30;
    char *monster[] = {"", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper", "Carrion Crawler",
                       "Gremlin", "Mimic", "Daemon", "Balrog"};
    if (mdraw == 0) clear(worldbmp);
    while (en != 1)
    {
        cent = floor(dng[player.px + player.dx * l][player.py + player.dy * l]);
        left = floor(dng[player.px + player.dx * l + player.dy][player.py + player.dy * l - player.dx]);
        right = floor(dng[player.px + player.dx * l - player.dy][player.py + player.dy * l + player.dx]);
        mon = floor(cent / 10);
        cent = cent - mon * 10;
        l1 = per[l][0];
        r1 = per[l][1];
        t1 = per[l][2];
        b1 = per[l][3];
        l2 = per[l + 1][0];
        r2 = per[l + 1][1];
        t2 = per[l + 1][2];
        b2 = per[l + 1][3];
        if (mdraw == 1)
            if (mon > 0)
            {
                if (l > 1)
                {
                    if (mon != 8)
                        textout(screen, font, monster[mon], 225, 167, 13);
                    else
                        textout(screen, font, "Chest!      ", 226, 167, 13);
                }
                drawmonsters(mon, l);
            }
        if (cent != 1 & cent != 3 & cent != 4)
        {
            if (left == 1 | left == 3 | left == 4)
            {
                line(worldbmp, l1, t1, l2, t2, color);
                line(worldbmp, l1, b1, l2, b2, color);
            }
            if (right == 1 | right == 3 | right == 4)
            {
                line(worldbmp, r1, t1, r2, t2, color);
                line(worldbmp, r1, b1, r2, b2, color);
            }
        }
        if (l != 0)
        {
            if (cent == 1 | cent == 3 | cent == 4)
            {
                line(worldbmp, l1, t1, r1, t1, color);
                line(worldbmp, r1, t1, r1, b1, color);
                line(worldbmp, r1, b1, l1, b1, color);
                line(worldbmp, l1, b1, l1, t1, color);
                if (cent != 4) return 0;
            }
            if (cent == 4)
            {
                line(worldbmp, cd[l][0], cd[l][3], cd[l][0], cd[l][2], color);
                line(worldbmp, cd[l][0], cd[l][2], cd[l][1], cd[l][2], color);
                line(worldbmp, cd[l][1], cd[l][2], cd[l][1], cd[l][3], color);
                return 0;
            }
            if (left == 4)
            {
                line(worldbmp, ld[l][0], ld[l][4], ld[l][0], ld[l][2], color);
                line(worldbmp, ld[l][0], ld[l][2], ld[l][1], ld[l][3], color);
                line(worldbmp, ld[l][1], ld[l][3], ld[l][1], ld[l][5], color);
            }
            if (right == 4)
            {
                line(worldbmp, 319 - ld[l][0], ld[l][4], 319 - ld[l][0], ld[l][2], color);
                line(worldbmp, 319 - ld[l][0], ld[l][2], 319 - ld[l][1], ld[l][3], color);
                line(worldbmp, 319 - ld[l][1], ld[l][3], 319 - ld[l][1], ld[l][5], color);
            }
            if (left != 1 & left != 3 & left != 4)
            {
                line(worldbmp, l1, t1, l1, b1, color);
                line(worldbmp, l1, t2, l2, t2, color);
                line(worldbmp, l2, t2, l2, b2, color);
                line(worldbmp, l2, b2, l1, b2, color);
            }
            if (right != 1 & right != 3 & right != 4)
            {
                line(worldbmp, r1, t1, r1, b1, color);
                line(worldbmp, r1, t2, r2, t2, color);
                line(worldbmp, r2, t2, r2, b2, color);
                line(worldbmp, r2, b2, r1, b2, color);
            }
        }
        else
        {
            if (left == 4)
            {
                line(worldbmp, 0, ld[l][2] - 3, ld[l][1], ld[l][3], color);
                line(worldbmp, ld[l][1], ld[l][3], ld[l][1], ld[l][5], color);
            }
            if (right == 4)
            {
                line(worldbmp, 319, ld[l][2] - 3, 319 - ld[l][1], ld[l][3], color);
                line(worldbmp, 319 - ld[l][1], ld[l][3], 319 - ld[l][1], ld[l][5], color);
            }
            if (en != 1)
            {
                if (left != 1 & left != 3 & left != 4)
                {
                    line(worldbmp, l1, t2, l2, t2, color);
                    line(worldbmp, l2, t2, l2, b2, color);
                    line(worldbmp, l2, b2, l1, b2, color);
                }
                if (right != 1 & right != 3 & right != 4)
                {
                    line(worldbmp, r1, t2, r2, t2, color);
                    line(worldbmp, r2, t2, r2, b2, color);
                    line(worldbmp, r2, b2, r1, b2, color);
                }
            }
        }
        if (cent == 7 | cent == 9)
        {
            line(worldbmp, ft[l][0], ft[l][4], ft[l][2], ft[l][5], color);
            line(worldbmp, ft[l][2], ft[l][5], ft[l][3], ft[l][5], color);
            line(worldbmp, ft[l][3], ft[l][5], ft[l][1], ft[l][4], color);
            line(worldbmp, ft[l][1], ft[l][4], ft[l][0], ft[l][4], color);
        }
        if (cent == 8)
        {
            line(worldbmp, ft[l][0], 166 - ft[l][4], ft[l][2], 166 - ft[l][5], color);
            line(worldbmp, ft[l][2], 166 - ft[l][5], ft[l][3], 166 - ft[l][5], color);
            line(worldbmp, ft[l][3], 166 - ft[l][5], ft[l][1], 166 - ft[l][4], color);
            line(worldbmp, ft[l][1], 166 - ft[l][4], ft[l][0], 166 - ft[l][4], color);
        }
        if (cent == 7 | cent == 8)
        {
            bl = lad[l][3];
            tl = lad[l][2];
            ll = lad[l][0];
            rl = lad[l][1];
            line(worldbmp, ll, bl, ll, tl, color);
            line(worldbmp, rl, tl, rl, bl, color);
            y1 = (bl * 4 + tl) / 5;
            y2 = (bl * 3 + tl * 2) / 5;
            y3 = (bl * 2 + tl * 3) / 5;
            y4 = (bl + tl * 4) / 5;
            line(worldbmp, ll, y1, rl, y1, color);
            line(worldbmp, ll, y2, rl, y2, color);
            line(worldbmp, ll, y3, rl, y3, color);
            line(worldbmp, ll, y4, rl, y4, color);
        }
        if (cent == 5 & l != 0)
        {
            line(worldbmp, 159 - 11 / l, per[l][3], 159 - 11 / l, per[l][3] - 11 / l, color);
            line(worldbmp, 159 - 11 / l, per[l][3] - 11 / l, 159 + 11 / l, per[l][3] - 11 / l, color);
            line(worldbmp, 159 + 11 / l, per[l][3] - 11 / l, 159 + 11 / l, per[l][3], color);
            line(worldbmp, 159 + 11 / l, per[l][3], 159 - 11 / l, per[l][3], color);
            line(worldbmp, 159 - 11 / l, per[l][3] - 11 / l, 159 - 6 / l, per[l][3] - 15 / l, color);
            line(worldbmp, 159 - 6 / l, per[l][3] - 15 / l, 159 + 16 / l, per[l][3] - 15 / l, color);
            line(worldbmp, 159 + 16 / l, per[l][3] - 15 / l, 159 + 16 / l, per[l][3] - 6 / l, color);
            line(worldbmp, 159 + 16 / l, per[l][3] - 6 / l, 159 + 11 / l, per[l][3], color);
            line(worldbmp, 159 + 11 / l, per[l][3] - 11 / l, 159 + 16 / l, per[l][3] - 15 / l, color);
            textout(screen, font, "Chest!", 225, 167, 13);
        }
        l++;
    }
done:                                           // never jumped to
    return 0;
}

// The monsters of a level: monster x (1 skeleton ... 10 balrog) lives on levels down to x - 2,
// at a free square; its hit points (mz[x][1]) grow with the level of play and the depth.
int akworld::createmonsters(charsave::savestruct &player)
{
    int nm = 0;
    int x;

    for (x = 1; x <= 10; x++)
    {
        mz[x][0] = 0;
        mz[x][1] = x + 3 + player.inout;
        if (x - 2 < player.inout)
        {
            ml[x][0] = floor(random() % 9 + 1);
            ml[x][1] = floor(random() % 9 + 1);
            while (dng[ml[x][0]][ml[x][1]] != 0 | (ml[x][0] == player.px & ml[x][1] == player.py))
            {
                ml[x][0] = floor(random() % 9 + 1);
                ml[x][1] = floor(random() % 9 + 1);
            }
            dng[ml[x][0]][ml[x][1]] = x * 10;
            mz[x][0] = 1;
            nm++;
            mz[x][1] = x * 2 + player.inout * 2 + player.level;
        }
    }
    return 0;
}

// A monster in the 3-D view, as lines around the floor point (x, y) of distance dist, every
// offset divided by the distance. The Apple original's HPLOT pictures. Three of the daemon's
// offsets lack the "/ dist" (x + 17, x - 17, y - 7): those strokes do not shrink with the
// distance. The carrion crawler hangs from the ceiling (83 - yy[dist]); the mimic looks like
// a chest.
int akworld::drawmonsters(int type, int dist)
{
    int y;
    int x;

    y = yy[dist] + 83;
    x = 159;
    switch (type) {
    case 1:                                // skeleton
        line(worldbmp, x - 27 / dist, y, x - 17 / dist, y, color);
        line(worldbmp, x - 17 / dist, y, x - 17 / dist, y - 16 / dist, color);
        line(worldbmp, x - 17 / dist, y - 16 / dist, x - 9 / dist, y - 31 / dist, color);
        line(worldbmp, x - 9 / dist, y - 31 / dist, x + 9 / dist, y - 31 / dist, color);
        line(worldbmp, x + 9 / dist, y - 31 / dist, x + 17 / dist, y - 17 / dist, color);
        line(worldbmp, x + 17 / dist, y - 17 / dist, x + 17 / dist, y, color);
        line(worldbmp, x + 17 / dist, y, x + 27 / dist, y, color);
        line(worldbmp, x, y - 27 / dist, x, y - 68 / dist, color);
        line(worldbmp, x - 3 / dist, y - 40 / dist, x + 3 / dist, y - 40 / dist, color);
        line(worldbmp, x - 4 / dist, y - 47 / dist, x + 4 / dist, y - 47 / dist, color);
        line(worldbmp, x - 4 / dist, y - 56 / dist, x + 4 / dist, y - 56 / dist, color);
        line(worldbmp, x - 27 / dist, y - 59 / dist, x - 35 / dist, y - 56 / dist, color);
        line(worldbmp, x - 35 / dist, y - 56 / dist, x - 27 / dist, y - 47 / dist, color);
        line(worldbmp, x - 27 / dist, y - 47 / dist, x - 27 / dist, y - 56 / dist, color);
        line(worldbmp, x - 27 / dist, y - 56 / dist, x - 9 / dist, y - 40 / dist, color);
        line(worldbmp, x - 9 / dist, y - 40 / dist, x - 17 / dist, y - 47 / dist, color);
        line(worldbmp, x - 17 / dist, y - 47 / dist, x - 9 / dist, y - 63 / dist, color);
        line(worldbmp, x - 9 / dist, y - 63 / dist, x + 9 / dist, y - 63 / dist, color);
        line(worldbmp, x + 9 / dist, y - 63 / dist, x + 17 / dist, y - 47 / dist, color);
        line(worldbmp, x + 17 / dist, y - 44 / dist, x + 17 / dist, y - 60 / dist, color);
        line(worldbmp, x + 14 / dist, y - 47 / dist, x + 23 / dist, y - 47 / dist, color);
        line(worldbmp, x, y - 79 / dist, x - 6 / dist, y - 84 / dist, color);
        line(worldbmp, x - 6 / dist, y - 84 / dist, x - 9 / dist, y - 79 / dist, color);
        line(worldbmp, x - 9 / dist, y - 79 / dist, x - 6 / dist, y - 68 / dist, color);
        line(worldbmp, x - 6 / dist, y - 68 / dist, x + 6 / dist, y - 68 / dist, color);
        line(worldbmp, x + 6 / dist, y - 68 / dist, x + 6 / dist, y - 71 / dist, color);
        line(worldbmp, x + 6 / dist, y - 71 / dist, x - 6 / dist, y - 71 / dist, color);
        line(worldbmp, x - 6 / dist, y - 71 / dist, x - 6 / dist, y - 68 / dist, color);
        line(worldbmp, x - 6 / dist, y - 68 / dist, x + 6 / dist, y - 68 / dist, color);
        line(worldbmp, x + 6 / dist, y - 68 / dist, x + 9 / dist, y - 79 / dist, color);
        line(worldbmp, x + 9 / dist, y - 79 / dist, x + 6 / dist, y - 84 / dist, color);
        line(worldbmp, x + 6 / dist, y - 84 / dist, x - 6 / dist, y - 84 / dist, color);
        break;
    case 2:                                // thief
        line(worldbmp, x, y - 59 / dist, x, y - 8 / dist, color);
        line(worldbmp, x, y - 8 / dist, x + 12 / dist, y, color);
        line(worldbmp, x + 12 / dist, y, x + 35 / dist, y, color);
        line(worldbmp, x + 35 / dist, y, x + 35 / dist, y - 47 / dist, color);
        line(worldbmp, x + 35 / dist, y - 47 / dist, x + 12 / dist, y - 67 / dist, color);
        line(worldbmp, x + 12 / dist, y - 68 / dist, x, y - 59 / dist, color);
        line(worldbmp, x, y - 59 / dist, x - 12 / dist, y - 67 / dist, color);
        line(worldbmp, x - 12 / dist, y - 67 / dist, x - 35 / dist, y - 47 / dist, color);
        line(worldbmp, x - 35 / dist, y - 47 / dist, x - 35 / dist, y, color);
        line(worldbmp, x - 35 / dist, y, x - 12 / dist, y, color);
        line(worldbmp, x - 12 / dist, y, x, y - 8 / dist, color);
        line(worldbmp, x - 12 / dist, y - 67 / dist, x - 12 / dist, y - 79 / dist, color);
        line(worldbmp, x - 12 / dist, y - 79 / dist, x, y - 87 / dist, color);
        line(worldbmp, x, y - 87 / dist, x + 12 / dist, y - 79 / dist, color);
        line(worldbmp, x + 12 / dist, y - 79 / dist, x, y - 83 / dist, color);
        line(worldbmp, x, y - 83 / dist, x - 12 / dist, y - 79 / dist, color);
        line(worldbmp, x - 12 / dist, y - 79 / dist, x, y - 63 / dist, color);
        line(worldbmp, x, y - 63 / dist, x + 12 / dist, y - 79 / dist, color);
        line(worldbmp, x + 12 / dist, y - 79 / dist, x + 12 / dist, y - 67 / dist, color);
        break;
    case 3:                                // giant rat
        line(worldbmp, x + 6 / dist, y - 31 / dist, x, y - 26 / dist, color);
        line(worldbmp, x, y - 26 / dist, x - 6 / dist, y - 31 / dist, color);
        line(worldbmp, x - 6 / dist, y - 31 / dist, x - 17 / dist, y - 5 / dist, color);
        line(worldbmp, x - 17 / dist, y - 5 / dist, x - 12 / dist, y, color);
        line(worldbmp, x - 12 / dist, y, x + 12 / dist, y, color);
        line(worldbmp, x + 12 / dist, y, x + 17 / dist, y - 5 / dist, color);
        line(worldbmp, x + 23 / dist, y - 5 / dist, x + 12 / dist, y, color);
        line(worldbmp, x + 12 / dist, y, x + 17 / dist, y - 5 / dist, color);
        line(worldbmp, x + 17 / dist, y - 5 / dist, x + 6 / dist, y - 31 / dist, color);
        line(worldbmp, x + 6 / dist, y - 31 / dist, x + 12 / dist, y - 42 / dist, color);
        line(worldbmp, x + 12 / dist, y - 42 / dist, x + 4 / dist, y - 37 / dist, color);
        line(worldbmp, x + 4 / dist, y - 37 / dist, x - 4 / dist, y - 37 / dist, color);
        line(worldbmp, x - 4 / dist, y - 37 / dist, x - 12 / dist, y - 42 / dist, color);
        line(worldbmp, x - 12 / dist, y - 42 / dist, x - 6 / dist, y - 31 / dist, color);
        line(worldbmp, x - 6 / dist, y - 35 / dist, x - 4 / dist, y - 31 / dist, color);
        line(worldbmp, x + 6 / dist, y - 35 / dist, x + 4 / dist, y - 31 / dist, color);
        line(worldbmp, x + 6 / dist, y - 21 / dist, x - 6 / dist, y - 16 / dist, color);
        line(worldbmp, x + 6 / dist, y - 21 / dist, x + 6 / dist, y - 16 / dist, color);
        line(worldbmp, x - 8 / dist, y - 21 / dist, x - 8 / dist, y - 16 / dist, color);
        line(worldbmp, x + 8 / dist, y - 21 / dist, x + 8 / dist, y - 16 / dist, color);
        break;
    case 4:                                // orc
        line(worldbmp, x, y, x - 17 / dist, y, color);
        line(worldbmp, x - 17 / dist, y, x - 9 / dist, y - 8 / dist, color);
        line(worldbmp, x - 9 / dist, y - 8 / dist, x - 9 / dist, y - 16 / dist, color);
        line(worldbmp, x - 9 / dist, y - 16 / dist, x - 17 / dist, y - 24 / dist, color);
        line(worldbmp, x - 17 / dist, y - 24 / dist, x - 17 / dist, y - 16 / dist, color);
        line(worldbmp, x - 17 / dist, y - 16 / dist, x - 27 / dist, y - 24 / dist, color);
        line(worldbmp, x - 27 / dist, y - 24 / dist, x - 27 / dist, y - 47 / dist, color);
        line(worldbmp, x - 27 / dist, y - 47 / dist, x - 17 / dist, y - 56 / dist, color);
        line(worldbmp, x - 17 / dist, y - 56 / dist, x - 9 / dist, y - 56 / dist, color);
        line(worldbmp, x - 9 / dist, y - 56 / dist, x - 17 / dist, y - 71 / dist, color);
        line(worldbmp, x - 17 / dist, y - 71 / dist, x - 8 / dist, y - 79 / dist, color);
        line(worldbmp, x - 8 / dist, y - 79 / dist, x, y - 79 / dist, color);
        line(worldbmp, x, y, x + 17 / dist, y, color);
        line(worldbmp, x + 17 / dist, y, x + 9 / dist, y - 8 / dist, color);
        line(worldbmp, x + 9 / dist, y - 8 / dist, x + 9 / dist, y - 16 / dist, color);
        line(worldbmp, x + 9 / dist, y - 16 / dist, x + 17 / dist, y - 24 / dist, color);
        line(worldbmp, x + 17 / dist, y - 24 / dist, x + 17 / dist, y - 16 / dist, color);
        line(worldbmp, x + 17 / dist, y - 16 / dist, x + 27 / dist, y - 24 / dist, color);
        line(worldbmp, x + 27 / dist, y - 24 / dist, x + 27 / dist, y - 47 / dist, color);
        line(worldbmp, x + 27 / dist, y - 47 / dist, x + 17 / dist, y - 56 / dist, color);
        line(worldbmp, x + 17 / dist, y - 56 / dist, x + 9 / dist, y - 56 / dist, color);
        line(worldbmp, x + 9 / dist, y - 56 / dist, x + 17 / dist, y - 71 / dist, color);
        line(worldbmp, x + 17 / dist, y - 71 / dist, x + 9 / dist, y - 79 / dist, color);
        line(worldbmp, x + 9 / dist, y - 79 / dist, x, y - 79 / dist, color);
        line(worldbmp, x - 17 / dist, y - 71 / dist, x + 17 / dist, y - 71 / dist, color);
        line(worldbmp, x - 9 / dist, y - 56 / dist, x + 9 / dist, y - 56 / dist, color);
        line(worldbmp, x - 29 / dist, y - 16 / dist, x + 9 / dist, y - 47 / dist, color);
        line(worldbmp, x - 9 / dist, y - 71 / dist, x, y - 63 / dist, color);
        line(worldbmp, x, y - 63 / dist, x + 9 / dist, y - 71 / dist, color);
        line(worldbmp, x + 9 / dist, y - 71 / dist, x + 9 / dist, y - 63 / dist, color);
        line(worldbmp, x + 9 / dist, y - 63 / dist, x - 9 / dist, y - 63 / dist, color);
        line(worldbmp, x - 9 / dist, y - 63 / dist, x - 9 / dist, y - 71 / dist, color);
        line(worldbmp, x, y - 40 / dist, x - 9 / dist, y - 40 / dist, color);
        line(worldbmp, x - 9 / dist, y - 40 / dist, x + 9 / dist, y - 56 / dist, color);
        line(worldbmp, x + 9 / dist, y - 56 / dist, x + 9 / dist, y - 47 / dist, color);
        line(worldbmp, x + 9 / dist, y - 47 / dist, x + 17 / dist, y - 47 / dist, color);
        line(worldbmp, x + 17 / dist, y - 47 / dist, x, y - 31 / dist, color);
        line(worldbmp, x, y - 31 / dist, x, y - 40 / dist, color);
        break;
    case 5:                                // viper
        line(worldbmp, x - 16 / dist, y - 16 / dist, x - 12 / dist, y - 31 / dist, color);
        line(worldbmp, x - 12 / dist, y - 31 / dist, x - 17 / dist, y - 21 / dist, color);
        line(worldbmp, x - 17 / dist, y - 21 / dist, x - 17 / dist, y - 16 / dist, color);
        line(worldbmp, x - 17 / dist, y - 16 / dist, x - 17 / dist, y, color);
        line(worldbmp, x - 17 / dist, y, x + 17 / dist, y, color);
        line(worldbmp, x + 17 / dist, y, x + 17 / dist, y - 16 / dist, color);
        line(worldbmp, x + 17 / dist, y - 16 / dist, x - 17 / dist, y - 16 / dist, color);
        line(worldbmp, x - 17 / dist, y - 10 / dist, x + 17 / dist, y - 10 / dist, color);
        line(worldbmp, x - 17 / dist, y - 5 / dist, x + 17 / dist, y - 5 / dist, color);
        line(worldbmp, x, y - 16 / dist, x - 6 / dist, y - 21 / dist, color);
        line(worldbmp, x - 6 / dist, y - 21 / dist, x - 6 / dist, y - 37 / dist, color);
        line(worldbmp, x - 6 / dist, y - 37 / dist, x + 6 / dist, y - 37 / dist, color);
        line(worldbmp, x + 6 / dist, y - 37 / dist, x + 6 / dist, y - 21 / dist, color);
        line(worldbmp, x + 6 / dist, y - 21 / dist, x + 12 / dist, y - 16 / dist, color);
        line(worldbmp, x - 6 / dist, y - 21 / dist, x + 6 / dist, y - 21 / dist, color);
        line(worldbmp, x - 6 / dist, y - 26 / dist, x + 6 / dist, y - 26 / dist, color);
        line(worldbmp, x - 6 / dist, y - 31 / dist, x + 6 / dist, y - 31 / dist, color);
        line(worldbmp, x - 23 / dist, y - 37 / dist, x - 12 / dist, y - 42 / dist, color);
        line(worldbmp, x - 12 / dist, y - 42 / dist, x - 6 / dist, y - 47 / dist, color);
        line(worldbmp, x - 6 / dist, y - 47 / dist, x + 6 / dist, y - 47 / dist, color);
        line(worldbmp, x + 6 / dist, y - 47 / dist, x + 12 / dist, y - 42 / dist, color);
        line(worldbmp, x + 12 / dist, y - 42 / dist, x + 12 / dist, y - 37 / dist, color);
        line(worldbmp, x - 12 / dist, y - 42 / dist, x, y - 47 / dist, color);
        line(worldbmp, x, y - 47 / dist, x + 12 / dist, y - 42 / dist, color);
        line(worldbmp, x - 6 / dist, y - 42 / dist, x + 6 / dist, y - 42 / dist, color);
        line(worldbmp, x + 6 / dist, y - 42 / dist, x + 17 / dist, y - 31 / dist, color);
        line(worldbmp, x + 17 / dist, y - 31 / dist, x, y - 42 / dist, color);
        line(worldbmp, x, y - 42 / dist, x - 17 / dist, y - 31 / dist, color);
        line(worldbmp, x - 17 / dist, y - 31 / dist, x - 6 / dist, y - 42 / dist, color);
        break;
    case 6:                                // carrion crawler
        line(worldbmp, x - 23 / dist, 83 - yy[dist], x - 23 / dist, y - 92 / dist, color);
        line(worldbmp, x - 23 / dist, y - 92 / dist, x - 12 / dist, y - 87 / dist, color);
        line(worldbmp, x - 12 / dist, y - 87 / dist, x + 12 / dist, y - 87 / dist, color);
        line(worldbmp, x + 12 / dist, y - 87 / dist, x + 23 / dist, y - 92 / dist, color);
        line(worldbmp, x + 23 / dist, y - 92 / dist, x + 23 / dist, 83 - yy[dist], color);
        line(worldbmp, x + 23 / dist, 83 - yy[dist], x - 23 / dist, 83 - yy[dist], color);
        line(worldbmp, x - 23 / dist, y - 92 / dist, x - 35 / dist, y - 87 / dist, color);
        line(worldbmp, x - 35 / dist, y - 87 / dist, x - 35 / dist, y - 82 / dist, color);
        line(worldbmp, x + 23 / dist, y - 92 / dist, x + 35 / dist, y - 87 / dist, color);
        line(worldbmp, x + 35 / dist, y - 87 / dist, x + 46 / dist, y - 87 / dist, color);
        line(worldbmp, x - 20 / dist, y - 90 / dist, x - 23 / dist, y - 87 / dist, color);
        line(worldbmp, x - 23 / dist, y - 87 / dist, x - 23 / dist, y - 82 / dist, color);
        line(worldbmp, x - 23 / dist, y - 82 / dist, x - 35 / dist, y - 79 / dist, color);
        line(worldbmp, x - 35 / dist, y - 79 / dist, x - 35 / dist, y - 71 / dist, color);
        line(worldbmp, x - 35 / dist, y - 71 / dist, x - 23 / dist, y - 66 / dist, color);
        line(worldbmp, x - 12 / dist, y - 87 / dist, x - 12 / dist, y - 61 / dist, color);
        line(worldbmp, x - 12 / dist, y - 61 / dist, x, y - 51 / dist, color);
        line(worldbmp, x + 12 / dist, y - 87 / dist, x + 12 / dist, y - 82 / dist, color);
        line(worldbmp, x + 12 / dist, y - 82 / dist, x + 23 / dist, y - 66 / dist, color);
        line(worldbmp, x + 17 / dist, y - 89 / dist, x + 23 / dist, y - 82 / dist, color);
        line(worldbmp, x + 23 / dist, y - 82 / dist, x + 35 / dist, y - 80 / dist, color);
        line(worldbmp, x + 35 / dist, y - 80 / dist, x + 35 / dist, y - 63 / dist, color);
        line(worldbmp, x, y - 87 / dist, x, y - 77 / dist, color);
        line(worldbmp, x, y - 77 / dist, x + 12 / dist, y - 71 / dist, color);
        line(worldbmp, x + 12 / dist, y - 71 / dist, x + 12 / dist, y - 66 / dist, color);
        line(worldbmp, x + 12 / dist, y - 66 / dist, x, y - 61 / dist, color);
        break;
    case 7:                                // gremlin
        line(worldbmp, x + 6 / dist, y - 10 / dist, x - 6 / dist, y - 10 / dist, color);
        line(worldbmp, x - 6 / dist, y - 10 / dist, x, y - 15 / dist, color);
        line(worldbmp, x, y - 15 / dist, x + 12 / dist, y - 21 / dist, color);
        line(worldbmp, x + 12 / dist, y - 21 / dist, x + 6 / dist, y - 16 / dist, color);
        line(worldbmp, x + 6 / dist, y - 16 / dist, x + 6 / dist, y - 10 / dist, color);
        line(worldbmp, x + 6 / dist, y - 10 / dist, x + 8 / dist, y - 6 / dist, color);
        line(worldbmp, x + 8 / dist, y - 6 / dist, x + 6 / dist, y - 3 / dist, color);
        line(worldbmp, x + 6 / dist, y - 3 / dist, x - 6 / dist, y - 3 / dist, color);
        line(worldbmp, x - 6 / dist, y - 3 / dist, x - 8 / dist, y - 6 / dist, color);
        line(worldbmp, x - 8 / dist, y - 6 / dist, x - 6 / dist, y - 10 / dist, color);
        line(worldbmp, x + 3 / dist, y - 3 / dist, x + 6 / dist, y, color);
        line(worldbmp, x + 6 / dist, y, x + 9 / dist, y, color);
        line(worldbmp, x - 3 / dist, y - 3 / dist, x - 6 / dist, y, color);
        line(worldbmp, x - 6 / dist, y, x - 4 / dist, y, color);
        line(worldbmp, x + 4 / dist, y - 5 / dist, x - 4 / dist, y - 5 / dist, color);
        putpixel(worldbmp, x + 4 / dist, y - 8 / dist, 15);
        putpixel(worldbmp, x - 4 / dist, y - 8 / dist, 15);
        break;
    case 8:                                // mimic
        line(worldbmp, 159 - 11 / dist, per[dist][3], 159 - 11 / dist, per[dist][3] - 11 / dist, color);
        line(worldbmp, 159 - 11 / dist, per[dist][3] - 11 / dist, 159 + 11 / dist, per[dist][3] - 11 / dist, color);
        line(worldbmp, 159 + 11 / dist, per[dist][3] - 11 / dist, 159 + 11 / dist, per[dist][3], color);
        line(worldbmp, 159 + 11 / dist, per[dist][3], 159 - 11 / dist, per[dist][3], color);
        line(worldbmp, 159 - 11 / dist, per[dist][3] - 11 / dist, 159 - 6 / dist, per[dist][3] - 15 / dist, color);
        line(worldbmp, 159 - 6 / dist, per[dist][3] - 15 / dist, 159 + 16 / dist, per[dist][3] - 15 / dist, color);
        line(worldbmp, 159 + 16 / dist, per[dist][3] - 15 / dist, 159 + 16 / dist, per[dist][3] - 6 / dist, color);
        line(worldbmp, 159 + 16 / dist, per[dist][3] - 6 / dist, 159 + 11 / dist, per[dist][3], color);
        line(worldbmp, 159 + 11 / dist, per[dist][3] - 11 / dist, 159 + 16 / dist, per[dist][3] - 15 / dist, color);
        break;
    case 9:                                // daemon
        line(worldbmp, x - 16 / dist, y - 48 / dist, x - 14 / dist, y - 39 / dist, color);
        line(worldbmp, x - 14 / dist, y - 39 / dist, x - 23 / dist, y - 34 / dist, color);
        line(worldbmp, x - 23 / dist, y - 34 / dist, x - 35 / dist, y - 34 / dist, color);
        line(worldbmp, x - 35 / dist, y - 34 / dist, x - 25 / dist, y - 25 / dist, color);
        line(worldbmp, x - 25 / dist, y - 25 / dist, x - 46 / dist, y - 18 / dist, color);
        line(worldbmp, x - 46 / dist, y - 18 / dist, x - 46 / dist, y - 8 / dist, color);
        line(worldbmp, x - 46 / dist, y - 8 / dist, x - 44 / dist, y - 5 / dist, color);
        line(worldbmp, x - 44 / dist, y - 5 / dist, x - 46 / dist, y - 3 / dist, color);
        line(worldbmp, x - 46 / dist, y - 3 / dist, x - 46 / dist, y, color);
        line(worldbmp, x - 46 / dist, y, x - 42 / dist, y, color);
        line(worldbmp, x - 42 / dist, y, x - 39 / dist, y - 2 / dist, color);
        line(worldbmp, x - 39 / dist, y - 2 / dist, x - 37 / dist, y, color);
        line(worldbmp, x - 37 / dist, y, x - 32 / dist, y, color);
        line(worldbmp, x - 32 / dist, y, x - 32 / dist, y - 3 / dist, color);
        line(worldbmp, x - 32 / dist, y - 3 / dist, x - 35 / dist, y - 5 / dist, color);
        line(worldbmp, x - 35 / dist, y - 5 / dist, x - 32 / dist, y - 8 / dist, color);
        line(worldbmp, x - 32 / dist, y - 8 / dist, x - 32 / dist, y - 16 / dist, color);
        line(worldbmp, x - 32 / dist, y - 16 / dist, x, y - 28 / dist, color);
        line(worldbmp, x + 16 / dist, y - 48 / dist, x + 14 / dist, y - 39 / dist, color);
        line(worldbmp, x + 14 / dist, y - 39 / dist, x + 23 / dist, y - 34 / dist, color);
        line(worldbmp, x + 23 / dist, y - 34 / dist, x + 35 / dist, y - 34 / dist, color);
        line(worldbmp, x + 35 / dist, y - 34 / dist, x + 25 / dist, y - 25 / dist, color);
        line(worldbmp, x + 25 / dist, y - 25 / dist, x + 46 / dist, y - 18 / dist, color);
        line(worldbmp, x + 46 / dist, y - 18 / dist, x + 46 / dist, y - 8 / dist, color);
        line(worldbmp, x + 46 / dist, y - 8 / dist, x + 44 / dist, y - 5 / dist, color);
        line(worldbmp, x + 44 / dist, y - 5 / dist, x + 46 / dist, y - 3 / dist, color);
        line(worldbmp, x + 46 / dist, y - 3 / dist, x + 46 / dist, y, color);
        line(worldbmp, x + 46 / dist, y, x + 42 / dist, y, color);
        line(worldbmp, x + 42 / dist, y, x + 39 / dist, y - 2 / dist, color);
        line(worldbmp, x + 39 / dist, y - 2 / dist, x + 37 / dist, y, color);
        line(worldbmp, x + 37 / dist, y, x + 32 / dist, y, color);
        line(worldbmp, x + 32 / dist, y, x + 32 / dist, y - 3 / dist, color);
        line(worldbmp, x + 32 / dist, y - 3 / dist, x + 35 / dist, y - 5 / dist, color);
        line(worldbmp, x + 35 / dist, y - 5 / dist, x + 32 / dist, y - 8 / dist, color);
        line(worldbmp, x + 32 / dist, y - 8 / dist, x + 32 / dist, y - 16 / dist, color);
        line(worldbmp, x + 32 / dist, y - 16 / dist, x, y - 28 / dist, color);
        line(worldbmp, x + 7 / dist, y - 51 / dist, x + 44 / dist, y - 43 / dist, color);
        line(worldbmp, x + 44 / dist, y - 43 / dist, x + 46 / dist, y - 44 / dist, color);
        line(worldbmp, x + 46 / dist, y - 44 / dist, x + 21 / dist, y - 59 / dist, color);
        line(worldbmp, x + 21 / dist, y - 59 / dist, x + 14 / dist, y - 59 / dist, color);
        line(worldbmp, x + 14 / dist, y - 59 / dist, x + 12 / dist, y - 60 / dist, color);
        line(worldbmp, x + 12 / dist, y - 60 / dist, x + 9 / dist, y - 59 / dist, color);
        line(worldbmp, x + 9 / dist, y - 59 / dist, x - 9 / dist, y - 59 / dist, color);
        line(worldbmp, x - 9 / dist, y - 59 / dist, x - 12 / dist, y - 61 / dist, color);
        line(worldbmp, x - 12 / dist, y - 61 / dist, x + 16 / dist, y - 61 / dist, color);
        line(worldbmp, x + 16 / dist, y - 61 / dist, x + 18 / dist, y - 62 / dist, color);
        line(worldbmp, x + 18 / dist, y - 62 / dist, x + 9 / dist, y - 66 / dist, color);
        line(worldbmp, x + 9 / dist, y - 66 / dist, x + 7 / dist, y - 66 / dist, color);
        line(worldbmp, x + 7 / dist, y - 66 / dist, x + 3 / dist, y - 73 / dist, color);
        line(worldbmp, x + 3 / dist, y - 73 / dist, x + 3 / dist, y - 66 / dist, color);
        line(worldbmp, x + 3 / dist, y - 66 / dist, x - 3 / dist, y - 66 / dist, color);
        line(worldbmp, x - 3 / dist, y - 66 / dist, x - 3 / dist, y - 73 / dist, color);
        line(worldbmp, x - 3 / dist, y - 73 / dist, x - 7 / dist, y - 66 / dist, color);
        line(worldbmp, x - 7 / dist, y - 66 / dist, x - 9 / dist, y - 66 / dist, color);
        line(worldbmp, x - 9 / dist, y - 66 / dist, x - 18 / dist, y - 62 / dist, color);
        line(worldbmp, x - 18 / dist, y - 62 / dist, x - 16 / dist, y - 61 / dist, color);
        line(worldbmp, x - 16 / dist, y - 61 / dist, x - 12 / dist, y - 60 / dist, color);
        line(worldbmp, x - 12 / dist, y - 60 / dist, x - 14 / dist, y - 59 / dist, color);
        line(worldbmp, x - 14 / dist, y - 59 / dist, x - 21 / dist, y - 59 / dist, color);
        line(worldbmp, x - 21 / dist, y - 59 / dist, x - 44 / dist, y - 49 / dist, color);
        line(worldbmp, x - 44 / dist, y - 49 / dist, x - 44 / dist, y - 41 / dist, color);
        line(worldbmp, x - 44 / dist, y - 41 / dist, x - 32 / dist, y - 43 / dist, color);
        line(worldbmp, x - 32 / dist, y - 43 / dist, x - 32 / dist, y - 48 / dist, color);
        line(worldbmp, x - 32 / dist, y - 48 / dist, x - 23 / dist, y - 53 / dist, color);
        line(worldbmp, x - 23 / dist, y - 53 / dist, x - 21 / dist, y - 53 / dist, color);
        line(worldbmp, x - 21 / dist, y - 53 / dist, x - 16 / dist, y - 48 / dist, color);
        line(worldbmp, x - 32 / dist, y - 43 / dist, x + 35 / dist, y - 58 / dist, color);
        line(worldbmp, x + 32 / dist, y - 61 / dist, x + 25 / dist, y - 59 / dist, color);
        line(worldbmp, x + 25 / dist, y - 59 / dist, x + 25 / dist, y - 56 / dist, color);
        line(worldbmp, x + 25 / dist, y - 56 / dist, x + 32 / dist, y - 55 / dist, color);
        line(worldbmp, x + 32 / dist, y - 55 / dist, x + 39 / dist, y - 57 / dist, color);
        line(worldbmp, x + 23 / dist, y - 53 / dist, x + 30 / dist, y - 49 / dist, color);
        line(worldbmp, x + 12 / dist, y - 61 / dist, x + 12 / dist, y - 64 / dist, color);
        line(worldbmp, x + 12 / dist, y - 64 / dist, x + 5 / dist, y - 61 / dist, color);
        line(worldbmp, x - 12 / dist, y - 61 / dist, x - 12 / dist, y - 64 / dist, color);
        line(worldbmp, x - 12 / dist, y - 64 / dist, x - 5 / dist, y - 61 / dist, color);
        line(worldbmp, x + 46 / dist, y - 9 / dist, x + 58 / dist, y - 13 / dist, color);
        line(worldbmp, x + 58 / dist, y - 13 / dist, x + 46 / dist, y - 8 / dist, color);
        line(worldbmp, x - 9 / dist, y - 26 / dist, x + 7 / dist, y - 7 / dist, color);
        line(worldbmp, x + 7 / dist, y - 7 / dist, x + 35 / dist, y - 7 / dist, color);
        line(worldbmp, x + 35 / dist, y - 7 / dist, x + 32 / dist, y - 9 / dist, color);
        line(worldbmp, x + 32 / dist, y - 9 / dist, x + 23 / dist, y - 9 / dist, color);
        line(worldbmp, x + 23 / dist, y - 9 / dist, x + 7 / dist, y - 26 / dist, color);
        break;
    case 10:                                // balrog
        line(worldbmp, x + 7 / dist, y - 63 / dist, x + 35 / dist, y - 94 / dist, color);
        line(worldbmp, x + 35 / dist, y - 94 / dist, x + 69 / dist, y - 31 / dist, color);
        line(worldbmp, x + 69 / dist, y - 31 / dist, x + 69 / dist, y - 10 / dist, color);
        line(worldbmp, x + 69 / dist, y - 10 / dist, x + 35 / dist, y - 42 / dist, color);
        line(worldbmp, x + 35 / dist, y - 42 / dist, x + 17 / dist, y - 42 / dist, color);
        line(worldbmp, x - 7 / dist, y - 63 / dist, x - 35 / dist, y - 94 / dist, color);
        line(worldbmp, x - 35 / dist, y - 94 / dist, x - 76 / dist, y - 31 / dist, color);
        line(worldbmp, x - 76 / dist, y - 31 / dist, x - 69 / dist, y - 10 / dist, color);
        line(worldbmp, x - 69 / dist, y - 10 / dist, x - 35 / dist, y - 42 / dist, color);
        line(worldbmp, x - 35 / dist, y - 42 / dist, x - 17 / dist, y - 42 / dist, color);
        line(worldbmp, x, y - 26 / dist, x + 7 / dist, y - 26 / dist, color);
        line(worldbmp, x + 7 / dist, y - 26 / dist, x + 12 / dist, y - 21 / dist, color);
        line(worldbmp, x + 12 / dist, y - 21 / dist, x + 15 / dist, y - 10 / dist, color);
        line(worldbmp, x + 15 / dist, y - 10 / dist, x + 12 / dist, y - 6 / dist, color);
        line(worldbmp, x + 12 / dist, y - 6 / dist, x + 12 / dist, y, color);
        line(worldbmp, x + 12 / dist, y, x + 16 / dist, y, color);
        line(worldbmp, x + 16 / dist, y, x + 17 / dist, y - 5 / dist, color);
        line(worldbmp, x + 17 / dist, y - 5 / dist, x + 19 / dist, y, color);
        line(worldbmp, x + 19 / dist, y, x + 23 / dist, y, color);
        line(worldbmp, x + 23 / dist, y, x + 23 / dist, y - 7 / dist, color);
        line(worldbmp, x + 23 / dist, y - 7 / dist, x + 21 / dist, y - 10 / dist, color);
        line(worldbmp, x + 21 / dist, y - 10 / dist, x + 17, y - 31 / dist, color);
        line(worldbmp, x + 17, y - 31 / dist, x + 17 / dist, y - 47 / dist, color);
        line(worldbmp, x + 17 / dist, y - 47 / dist, x + 46 / dist, y - 63 / dist, color);
        line(worldbmp, x + 46 / dist, y - 63 / dist, x + 46 / dist, y - 73 / dist, color);
        line(worldbmp, x + 46 / dist, y - 73 / dist, x + 12 / dist, y - 58 / dist, color);
        line(worldbmp, x + 12 / dist, y - 58 / dist, x + 7 / dist, y - 63 / dist, color);
        line(worldbmp, x + 7 / dist, y - 63 / dist, x + 12 / dist, y - 78 / dist, color);
        line(worldbmp, x + 12 / dist, y - 78 / dist, x + 7 / dist, y - 84 / dist, color);
        line(worldbmp, x + 7 / dist, y - 84 / dist, x + 5 / dist, y - 84 / dist, color);
        line(worldbmp, x + 5 / dist, y - 84 / dist, x + 4 / dist, y - 86 / dist, color);
        line(worldbmp, x + 4 / dist, y - 86 / dist, x + 3 / dist, y - 84 / dist, color);
        line(worldbmp, x + 3 / dist, y - 84 / dist, x, y - 84 / dist, color);
        line(worldbmp, x, y - 26 / dist, x - 7 / dist, y - 26 / dist, color);
        line(worldbmp, x - 7 / dist, y - 26 / dist, x - 12 / dist, y - 21 / dist, color);
        line(worldbmp, x - 12 / dist, y - 21 / dist, x - 15 / dist, y - 10 / dist, color);
        line(worldbmp, x - 15 / dist, y - 10 / dist, x - 12 / dist, y - 6 / dist, color);
        line(worldbmp, x - 12 / dist, y - 6 / dist, x - 12 / dist, y, color);
        line(worldbmp, x - 12 / dist, y, x - 16 / dist, y, color);
        line(worldbmp, x - 16 / dist, y, x - 17 / dist, y - 5 / dist, color);
        line(worldbmp, x - 17 / dist, y - 5 / dist, x - 19 / dist, y, color);
        line(worldbmp, x - 19 / dist, y, x - 23 / dist, y, color);
        line(worldbmp, x - 23 / dist, y, x - 23 / dist, y - 7 / dist, color);
        line(worldbmp, x - 23 / dist, y - 7 / dist, x - 21 / dist, y - 10 / dist, color);
        line(worldbmp, x - 21 / dist, y - 10 / dist, x - 17, y - 31 / dist, color);
        line(worldbmp, x - 17, y - 31 / dist, x - 17 / dist, y - 47 / dist, color);
        line(worldbmp, x - 17 / dist, y - 47 / dist, x - 46 / dist, y - 63 / dist, color);
        line(worldbmp, x - 46 / dist, y - 63 / dist, x - 46 / dist, y - 73 / dist, color);
        line(worldbmp, x - 46 / dist, y - 73 / dist, x - 12 / dist, y - 58 / dist, color);
        line(worldbmp, x - 12 / dist, y - 58 / dist, x - 7 / dist, y - 63 / dist, color);
        line(worldbmp, x - 7 / dist, y - 63 / dist, x - 12 / dist, y - 78 / dist, color);
        line(worldbmp, x - 12 / dist, y - 78 / dist, x - 7 / dist, y - 84 / dist, color);
        line(worldbmp, x - 7 / dist, y - 84 / dist, x - 5 / dist, y - 84 / dist, color);
        line(worldbmp, x - 5 / dist, y - 84 / dist, x - 4 / dist, y - 86 / dist, color);
        line(worldbmp, x - 4 / dist, y - 86 / dist, x - 3 / dist, y - 84 / dist, color);
        line(worldbmp, x - 3 / dist, y - 84 / dist, x, y - 84 / dist, color);
        line(worldbmp, x - 7 / dist, y - 26 / dist, x, y - 7, color);
        line(worldbmp, x, y - 7, x + 12 / dist, y, color);
        line(worldbmp, x + 12 / dist, y, x + 5 / dist, y - 8 / dist, color);
        line(worldbmp, x + 5 / dist, y - 8 / dist, x + 7 / dist, y - 26 / dist, color);
        line(worldbmp, x - 46 / dist, y - 67 / dist, x - 46 / dist, y - 94 / dist, color);
        line(worldbmp, x - 46 / dist, y - 94 / dist, x - 60 / dist, y - 84 / dist, color);
        line(worldbmp, x + 46 / dist, y - 90 / dist, x + 44 / dist, y - 97 / dist, color);
        line(worldbmp, x + 44 / dist, y - 97 / dist, x + 48 / dist, y - 97 / dist, color);
        line(worldbmp, x + 48 / dist, y - 97 / dist, x + 46 / dist, y - 90 / dist, color);
        line(worldbmp, x + 46 / dist, y - 90 / dist, x + 46 / dist, y - 53 / dist, color);
        line(worldbmp, x + 5 / dist, y - 73 / dist, x + 7 / dist, y - 78 / dist, color);
        line(worldbmp, x - 5 / dist, y - 73 / dist, x - 7 / dist, y - 78 / dist, color);
        line(worldbmp, x, y - 67 / dist, x, y - 63 / dist, color);
        break;
    }
    return 0;
}

// The player attacks the square ahead. Weapons: R rapier (fighters only), A axe (swing, or
// throw it), S shield, B bow (fighters only, range 5), M the magic amulet; any other key fights
// with bare hands. A mage chooses the amulet's spell, a fighter gets a random one.
int akworld::attack(charsave::savestruct &player)
{
    int mn = 0;
    int dam = 0;
    int gold = 0;
    int key;
    int q;
    int y;
    int a;
    char string[8];
    char *weapon[] = {"Food", "Rapier", "Axe", "Shield", "Bow and Arrow", "Magic Amulet"};
    char *monster[] = {"", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper", "Carrion Crawler",
                       "Gremlin", "Mimic", "Daemon", "Balrog"};

again:
    textout(screen, font, "Attack!      ", 8, 167, 9);
    textout(screen, font, "Which Weapon?     ", 8, 175, 11);
    key = readkey() & 0xff;
    switch (key) {
    case 'r':
        textout(screen, font, weapon[1], 120, 175, 11);
        if (player.owned[1] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, 11);
            goto again;
        }
        if (player.ChClass == 'M')
        {
            textout(screen, font, "Mages can't use Rapiers!", 8, 191, 11);
            goto again;
        }
        dam = 10;
        break;
    case 'a':
        textout(screen, font, weapon[2], 120, 175, 11);
        if (player.owned[2] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, 11);
            goto again;
        }
        dam = 5;
        break;
    case 's':
        textout(screen, font, weapon[3], 120, 175, 11);
        if (player.owned[3] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, 11);
            goto again;
        }
        dam = 1;
        break;
    case 'b':
        textout(screen, font, weapon[4], 120, 175, 11);
        if (player.owned[4] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, 11);
            goto again;
        }
        if (player.ChClass == 'M')
        {
            textout(screen, font, "Mages can't use Bows!", 8, 191, 11);
            goto again;
        }
        dam = 4;
        goto ranged;
    case 'm':
        textout(screen, font, weapon[5], 120, 175, 11);
        if (player.owned[5] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, 11);
            goto again;
        }
        if (player.ChClass == 'F')
            a = rand() % 4 + 1;
        else
        {
            textout(screen, font, "1-Ladder-Up", 8, 183, 11);
            textout(screen, font, "2-Ladder-Dn", 130, 183, 11);
            textout(screen, font, "3-Kill", 8, 191, 11);
            textout(screen, font, "4-Bad??", 130, 191, 11);
            q = readkey() & 0xff;
            if (q == 27) goto again;
            while (q < 49 | q > 52)
            {
                q = readkey() & 0xff;
                if (q == 27) goto again;
            }
            a = q - 48;
        }
        if (rand() % 100 > 75)
        {
            textout(screen, font, "Last Charge on This Amulet!", 8, 183, 11);
            player.owned[5]--;
        }
        switch (a) {
        case 1:
            textout(screen, font, "Ladder Up                  ", 8, 183, 11);
            textout(screen, font, "                           ", 8, 191, 11);
            dng[player.px][player.py] = 8;
            drawdungeon(player);
            break;
        case 2:
            textout(screen, font, "Ladder Down                ", 8, 183, 11);
            textout(screen, font, "                           ", 8, 191, 11);
            dng[player.px][player.py] = 7;
            drawdungeon(player);
            break;
        case 3:
            textout(screen, font, "Magic Attack!              ", 8, 183, 11);
            dam = player.inout + 10;
            goto ranged;
        case 4:
            q = rand() % 3 + 1;
            if (q == 1)
            {
                textout(screen, font, "You have been turned      ", 8, 183, 11);
                textout(screen, font, "into a Toad!              ", 8, 191, 11);
                for (y = 1; y <= 4; y++)
                    player.character[y] = 3;
            }
            if (q == 2)
            {
                textout(screen, font, "You have been turned      ", 8, 183, 11);
                textout(screen, font, "into a Lizard Man!        ", 8, 191, 11);
                for (y = 0; y <= 4; y++)
                    player.character[y] = floor(player.character[y] * 2.5);
            }
            if (q == 3)
            {
                textout(screen, font, "Backfire!                 ", 8, 183, 12);
                player.character[0] = player.character[0] / 2;
            }
            break;
        }
        return 0;
    }
    if (dam == 0) textout(screen, font, "Hands", 120, 175, 11);
    if (dam == 5)
    {
        textout(screen, font, "To throw or swing? ", 8, 183, 11);
        key = readkey() & 0xff;
        if (key != 't')
            textout(screen, font, "Swing", 160, 183, 11);
        else
        {
            textout(screen, font, "Throw", 160, 183, 11);
            player.owned[2]--;
ranged:
            for (y = 1; y <= 5; y++)
            {
                if (player.px + player.dx * y < 1 | player.px + player.dx * y > 9
                    | player.py + player.dy * y > 9 | player.py * player.dy * y < 0)
                    goto hit;
                else
                {
                    mn = dng[player.px + player.dx * y][player.py + player.dy * y];
                    mn = floor(mn / 10);
                    if (mn > 0) goto hit;
                }
            }
        }
    }
    mn = floor(dng[player.px + player.dx][player.py + player.dy] / 10);
hit:
    if (mn < 1 | player.character[2] - rand() % 25 < mn + player.inout)
    {
        sound(100);
        rest(200);
        sound(0);
        textout(screen, font, "Missed!                    ", 8, 191, 11);
    }
    else
    {
        sound(2000);
        rest(200);
        sound(0);
        textout(screen, font, "Hit!                       ", 8, 191, 12);
        if (dam != 0)
            dam = rand() % dam + player.character[1] / 5;
        else
            dam = player.character[1] / 5;
        mz[mn][1] = mz[mn][1] - dam;
        textout(screen, font, "Hit Points = ", 48, 191, 11);
        itoa(mz[mn][1], string, 10);
        textout(screen, font, string, 152, 191, 9);
        if (mz[mn][1] < 1)
        {
            dng[ml[mn][0]][ml[mn][1]] = dng[ml[mn][0]][ml[mn][1]] - mn * 10;
            mz[mn][0] = 0;
            player.lk = player.lk + (int)floor(mn * player.inout / 2);
            if (mn == player.task)
            {
                player.task = -player.task;
                sound(1500);
                rest(300);
                sound(2000);
                rest(300);
                sound(0);
            }
            drawdungeon(player);
            textout(screen, font, "Thou hast killed a            ", 8, 175, 11);
            textout(screen, font, monster[mn], 160, 175, 9);
            gold = floor(mn + player.inout);
            textout(screen, font, "Thou shalt receive            ", 8, 183, 11);
            itoa(gold, string, 10);
            textout(screen, font, string, 160, 183, 11);
            textout(screen, font, "Pieces of Eight             ", 8, 191, 11);
            player.character[5] = player.character[5] + gold;
        }
    }
    rest(300);
    return 0;
}

// The monsters' turn. A monster next to the player attacks, unless it is too weak (then it
// flees); a thief or a gremlin may steal instead. The others walk towards the player, the
// mimic stays put. sgn(z1) is called where a statement does nothing.
int akworld::movemonsters(charsave::savestruct &player)
{
    int x;
    int xs;
    int ys;
    int z1;
    int z2;
    int it;
    int dam;
    int hit = 0;
    char *string;
    char *monster[] = {"", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper", "Carrion Crawler",
                       "Gremlin", "Mimic", "Daemon", "Balrog"};
    char *item[] = {"Food", "Rapier", "Axe", "Shield", "Bow and Arrow", "Magic Amulet"};

    run1 = 0;
    for (x = 1; x <= 10; x++)
    {
        if (mz[x][0] != 0)
        {
            if (mz[x][1] < ((x + player.inout) * 2 + player.level) * .3)
                ;                               // too weak to fight: it flees (below)
            else
            {
                if ((abs(player.px - ml[x][0]) == 1 & player.py == ml[x][1])
                    | (abs(player.py - ml[x][1]) == 1 & player.px == ml[x][0]))
                {
                    textout(screen, font, "You are being attacked     ", 8, 175, 11);
                    textout(screen, font, "by a                       ", 8, 183, 11);
                    textout(screen, font, monster[x], 48, 183, 9);
                    hit = 1;
                    if (x != 2 & x != 7)
                    {
attack:
                        if (rand() % 20 - sgn(player.owned[3]) - player.character[3] + x + player.inout < 0)
                        {
                            sound(100);
                            rest(200);
                            sound(0);
                            textout(screen, font, "Missed!             ", 8, 191, 11);
                        }
                        else
                        {
                            sound(2000);
                            rest(200);
                            sound(0);
                            textout(screen, font, "Hit!                ", 8, 191, 12);
                            textout(screen, font, "Damage = ", 48, 191, 11);
                            dam = rand() % x + player.inout;
                            itoa(dam, string, 10);
                            textout(screen, font, string, 120, 191, 9);
                            player.character[0] = player.character[0] - dam;
                        }
                    }
                    else
                    {
                        if (rand() % 100 < 90) goto attack;
                        if (x == 7)
                        {
                            textout(screen, font, "A Gremlin Stole Some Food!", 8, 191, 10);
                            player.owned[0] = floor(player.owned[0] / 2);
                        }
                        if (x == 2)
                        {
                            it = rand() % 6;
                            while (player.owned[it] < 1) it = rand() % 6;
                            textout(screen, font, "A Thief Stole a", 8, 191, 10);
                            textout(screen, font, item[it], 136, 191, 9);
                            player.owned[it] = player.owned[it] - 1;
                        }
                    }
                    goto next;
                }
            }
            if (x == 8) goto stay;              // the mimic never moves
            xs = sgn(player.px - ml[x][0]);
            ys = sgn(player.py - ml[x][1]);
            if (mz[x][1] < ((x + player.inout) * 2 + player.level) * .3)
            {
                xs = -xs;
                ys = -ys;
                run1 = 1;
            }
            z1 = dng[ml[x][0]][ml[x][1] + ys];
            z2 = dng[ml[x][0] + xs][ml[x][1]];
            if (z1 == 1 | z1 > 9 | z1 == 2)
                sgn(z1);
            else
            {
                dng[ml[x][0]][ml[x][1]] = dng[ml[x][0]][ml[x][1]] - x * 10;
                xs = 0;
                ml[x][0] = ml[x][0] + xs;
                ml[x][1] = ml[x][1] + ys;
                dng[ml[x][0]][ml[x][1]] = dng[ml[x][0]][ml[x][1]] + x * 10;
            }
            if (z2 == 1 | z2 > 9 | z2 == 2)
                sgn(z2);
            else
            {
                dng[ml[x][0]][ml[x][1]] = dng[ml[x][0]][ml[x][1]] - x * 10;
                ys = 0;
                ml[x][0] = ml[x][0] + xs;
                ml[x][1] = ml[x][1] + ys;
                dng[ml[x][0]][ml[x][1]] = dng[ml[x][0]][ml[x][1]] + x * 10;
            }
        }
next:
stay:
        sgn(z1);
    }
    if (run1 == 1) drawdungeon(player);
    return 0;
}

int akworld::sgn(int n)
{
    if (n > 0) return n = 1;
    if (n < 0) return n = -1;
    if (n == 0) return n = 0;
    return n;
}

// The introduction's demonstration: a cursor walks a little map by the script in str (A up,
// B down, C right, D left, W wait) until a key is pressed.
int akworld::viewintro(charsave::savestruct &player)
{
    char str[] = "AACCWWWDDBBWWWCCWWWWWWDDWWW";
    int i = 1;
    int x = -1;
    int y = 1;

    sav.inout = -1;
    sav.tx = 2;
    sav.ty = 2;
    world[1][1] = 2;
    world[1][2] = 0;
    world[1][3] = 3;
    world[3][1] = 5;
    world[3][2] = 0;
    world[3][3] = 4;
    world[2][1] = 0;
    world[2][2] = 1;
    world[2][3] = 0;
    textout_centre(screen, font, "Ultima 0 - Akalabeth", 160, 167, 11);
    textout_centre(screen, font, "Intro - Hit a Key", 160, 175, 11);
    while (!keypressed())
    {
        clear(worldbmp);
        drawworld(player);
        switch (str[i]) {
        case 'A':
            y--;
            break;
        case 'B':
            y++;
            break;
        case 'C':
            x++;
            break;
        case 'D':
            x--;
            break;
        case 'W':
            rest(300);
            break;
        }
        line(worldbmp, x * 58 + 138, (y - 1) * 52 + 78, x * 58 + 143, (y - 1) * 52 + 78, 11);
        line(worldbmp, x * 58 + 141, (y - 1) * 52 + 76, x * 58 + 141, (y - 1) * 52 + 80, 11);
        if (i == 4) textout(screen, font, "Quest Please!              ", 8, 183, 11);
        if (i == 11 | i == 24) textout(screen, font, "Three Axes to go! ", 8, 183, 11);
        if (i == 16) textout(screen, font, "PoW! ", 8, 183, 11);
        if (i == 18) textout(screen, font, "Thunk! ", 40, 183, 11);
        if (i == 20) textout(screen, font, "ZiFF!", 88, 183, 11);
        i++;
        if (i == 26) i = 0;
        rest(300);
    }
    return 0;
}
