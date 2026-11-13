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
#include "akdefs.h"

// The perspective tables for the 3-D dungeon view. xx/yy are the half-width and half-height
// of the corridor at each distance (the Apple's 280x160 view scaled to 320x166); per, cd, ld,
// ft and lad are the corners of walls, doors, ladders and so on at each distance.
akworld::akworld()
{
    int side;
    int dist;
    int y;

    worldbmp = create_sub_bitmap(screen, 0, 0, 320, 166);
    xx[0] = 159;
    yy[0] = 83;
    for (dist = 1; dist <= 10; dist++)
    {
        xx[dist] = floor(atan(1 / (dist * 2.0)) / .785398 * 140 + .5);
        yy[dist] = floor(xx[dist] * 4.0 / 7);
        xx[dist] = floor(xx[dist] / 279.0 * 320);
        yy[dist] = floor(yy[dist] / 159.0 * 166);
        per[dist][0] = 159 - xx[dist];
        per[dist][1] = 159 + xx[dist];
        per[dist][2] = 83 - yy[dist];
        per[dist][3] = 83 + yy[dist];
    }
    per[0][0] = 0;
    per[0][1] = 319;
    per[0][2] = 0;
    per[0][3] = 166;
    for (dist = 1; dist <= 10; dist++)
    {
        cd[dist][0] = 159 - (int)(xx[dist] / 3.0);
        cd[dist][1] = 159 + (int)(xx[dist] / 3.0);
        cd[dist][2] = 83 - (int)(yy[dist] * .7);
        cd[dist][3] = 83 + yy[dist];
    }
    for (dist = 0; dist <= 9; dist++)
    {
        ld[dist][0] = (per[dist][0] * 2 + per[dist + 1][0]) / 3;
        ld[dist][1] = (per[dist][0] + 2 * per[dist + 1][0]) / 3;
        side = ld[dist][0] - per[dist][0];
        ld[dist][2] = per[dist][2] + side * 4 / 7;
        ld[dist][3] = per[dist][2] + 2 * side * 4 / 7;
        ld[dist][4] = (per[dist][3] * 2 + per[dist + 1][3]) / 3;
        ld[dist][5] = (per[dist][3] + 2 * per[dist + 1][3]) / 3;
        ld[dist][2] = ld[dist][4] - (ld[dist][4] - ld[dist][2]) * .8;
        ld[dist][3] = ld[dist][5] - (ld[dist][5] - ld[dist][3]) * .8;
        if (ld[dist][3] == ld[dist][4]) ld[dist][3] = ld[dist][3] - 1;
    }
    for (dist = 0; dist <= 9; dist++)
    {
        ft[dist][0] = 159 - xx[dist] / 3;
        ft[dist][1] = 159 + xx[dist] / 3;
        ft[dist][2] = 159 - xx[dist + 1] / 3;
        ft[dist][3] = 159 + xx[dist + 1] / 3;
        ft[dist][4] = 83 + (yy[dist] * 2 + yy[dist + 1]) / 3;
        ft[dist][5] = 83 + (yy[dist] + 2 * yy[dist + 1]) / 3;
    }
    for (dist = 0; dist <= 9; dist++)
    {
        lad[dist][0] = (ft[dist][0] * 2 + ft[dist][1]) / 3;
        lad[dist][1] = (ft[dist][0] + 2 * ft[dist][1]) / 3;
        lad[dist][3] = ft[dist][4];
        lad[dist][2] = 166 - lad[dist][3];
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
    textout(screen, font, "Welcome to Akalabeth, World of Doom!", 16, 96, CLR_LIGHTCYAN);
    textout(screen, font, "(Please Wait)", 24, 184, CLR_LIGHTCYAN);
    for (x = 0; x <= 20; x++)
    {
        world[x][0] = LAND_MOUNTAINS;
        world[0][x] = LAND_MOUNTAINS;
        world[x][20] = LAND_MOUNTAINS;
        world[20][x] = LAND_MOUNTAINS;
    }
    for (x = 1; x <= 19; x++)
        for (y = 1; y <= 19; y++)
        {
            world[x][y] = floor(pow(random() % 100 * .01, 5) * 4.5);
            if (world[x][y] == LAND_TOWN & rand() % 100 > 50) world[x][y] = LAND_TOWN;
        }
    world[(int)floor(random() % 100 * .01 * 19 + 1)][(int)floor(random() % 100 * .01 * 19 + 1)] = LAND_CASTLE;
    if (mode == 0)
    {
        player.tx = floor(random() % 100 * .01 * 19 + 1);
        player.ty = floor(random() % 100 * .01 * 19 + 1);
        world[player.tx][player.ty] = LAND_TOWN;
    }
    else
        world[(int)floor(random() % 100 * .01 * 19 + 1)][(int)floor(random() % 100 * .01 * 19 + 1)] = LAND_TOWN;
    return 0;
}

// The 3x3 squares around the player, 58x52 pixels each: 1 mountains, 2 a tree, 3 a town,
// 4 a dungeon entrance, 5 the castle. On the surface a cross marks the player.
int akworld::drawworld(charsave::savestruct &player)
{
    int x;
    int y;
    int land;
    int left;
    int top;

    clear(worldbmp);
    for (y = -1; y <= 1; y++)
        for (x = -1; x <= 1; x++)
        {
            if (player.inout == 0)
            {
                line(worldbmp, 140, 78, 144, 78, CLR_LIGHTCYAN);
                line(worldbmp, 142, 76, 142, 80, CLR_LIGHTCYAN);
            }
            land = world[player.tx + x][player.ty + y];
            left = (x + 1) * 58 + 55;
            top = (y + 1) * 52;
            switch (land) {
            case LAND_MOUNTAINS:
                line(worldbmp, left + 12, top + 53, left + 12, top + 42, CLR_LIGHTCYAN);
                line(worldbmp, left + 12, top + 42, left + 23, top + 31, CLR_LIGHTCYAN);
                line(worldbmp, left + 23, top + 31, left + 46, top + 31, CLR_LIGHTCYAN);
                line(worldbmp, left + 46, top + 31, left + 46, top + 53, CLR_LIGHTCYAN);
                line(worldbmp, left, top + 10, left + 12, top + 10, CLR_LIGHTCYAN);
                line(worldbmp, left + 58, top + 10, left + 46, top + 10, CLR_LIGHTCYAN);
                line(worldbmp, left, top + 42, left + 12, top + 42, CLR_LIGHTCYAN);
                line(worldbmp, left + 46, top + 42, left + 58, top + 42, CLR_LIGHTCYAN);
                line(worldbmp, left + 12, top, left + 12, top + 21, CLR_LIGHTCYAN);
                line(worldbmp, left + 12, top + 21, left + 23, top + 21, CLR_LIGHTCYAN);
                line(worldbmp, left + 23, top + 21, left + 23, top + 31, CLR_LIGHTCYAN);
                line(worldbmp, left + 23, top + 31, left + 35, top + 31, CLR_LIGHTCYAN);
                line(worldbmp, left + 35, top + 31, left + 35, top + 11, CLR_LIGHTCYAN);
                line(worldbmp, left + 35, top + 10, left + 46, top + 10, CLR_LIGHTCYAN);
                line(worldbmp, left + 46, top + 10, left + 46, top, CLR_LIGHTCYAN);
                break;
            case LAND_TREE:
                rect(worldbmp, left + 23, top + 21, left + 35, top + 31, CLR_LIGHTCYAN);
                break;
            case LAND_TOWN:
                rect(worldbmp, left + 11, top + 10, left + 23, top + 21, CLR_LIGHTCYAN);
                rect(worldbmp, left + 35, top + 10, left + 46, top + 21, CLR_LIGHTCYAN);
                rect(worldbmp, left + 23, top + 21, left + 35, top + 31, CLR_LIGHTCYAN);
                rect(worldbmp, left + 11, top + 31, left + 23, top + 43, CLR_LIGHTCYAN);
                rect(worldbmp, left + 35, top + 31, left + 46, top + 43, CLR_LIGHTCYAN);
                break;
            case LAND_DUNGEON:
                line(worldbmp, left + 23, top + 21, left + 34, top + 31, CLR_LIGHTCYAN);
                line(worldbmp, left + 23, top + 31, left + 34, top + 21, CLR_LIGHTCYAN);
                break;
            case LAND_CASTLE:
                rect(worldbmp, left, top, left + 58, top + 53, CLR_LIGHTCYAN);
                rect(worldbmp, left + 12, top + 10, left + 46, top + 42, CLR_LIGHTCYAN);
                line(worldbmp, left + 12, top + 10, left + 46, top + 42, CLR_LIGHTCYAN);
                line(worldbmp, left + 46, top + 10, left + 12, top + 42, CLR_LIGHTCYAN);
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
            dng[x][y] = DG_FLOOR;
    for (x = 0; x <= 10; x++)
    {
        dng[x][0] = DG_WALL;
        dng[x][10] = DG_WALL;
        dng[0][x] = DG_WALL;
        dng[10][x] = DG_WALL;
    }
    for (x = 2; x <= 8; x += 2)
        for (y = 1; y <= 9; y++)
        {
            dng[x][y] = DG_WALL;
            dng[y][x] = DG_WALL;
        }
    for (x = 2; x <= 8; x += 2)
        for (y = 1; y <= 9; y += 2)
        {
            if (random() % 100 > 95) dng[x][y] = DG_TRAP;
            if (random() % 100 > 95) dng[y][x] = DG_TRAP;
            if (random() % 100 > 70) dng[x][y] = DG_HIDDEN_DOOR;
            if (random() % 100 > 70) dng[y][x] = DG_HIDDEN_DOOR;
            if (random() % 100 > 40) dng[x][y] = DG_DOOR;
            if (random() % 100 > 40) dng[y][x] = DG_DOOR;
            if (random() % 100 > 97) dng[y][x] = DG_HOLE;
            if (random() % 100 > 97) dng[x][y] = DG_HOLE;
            if (random() % 100 > 94) dng[x][y] = DG_CHEST;
            if (random() % 100 > 94) dng[y][x] = DG_CHEST;
        }
    dng[2][1] = DG_FLOOR;
    if (player.inout / 2.0 == floor(player.inout / 2.0))
    {
        dng[7][3] = DG_LADDER_DOWN;
        dng[3][7] = DG_LADDER_UP;
    }
    else
    {
        dng[7][3] = DG_LADDER_UP;
        dng[3][7] = DG_LADDER_DOWN;
    }
    if (player.inout == 1)
    {
        dng[1][1] = DG_LADDER_UP;
        dng[7][3] = DG_FLOOR;
    }
    return 0;
}

// The 3-D view of the dungeon, drawn from the player's square outwards, one distance l at a
// time, until a wall or a door ends it (en is never set: the returns end the loop). cent, left
// and right are the squares ahead and beside at distance l; l1/r1/t1/b1 the near corners of
// the corridor there and l2/r2/t2/b2 the far ones. With mdraw set the monsters are drawn too.
int akworld::drawdungeon(charsave::savestruct &player)
{
    int lbottom, ltop, lleft, lright;
    int rung1, rung2, rung3, rung4;
    int ahead, left, right;
    int l1, l2, r1, r2, b1, b2, t1, t2;
    int mtype;
    int stop = 0;
    int dist = 0;

    color = player.inout + 30;
    char *monster[] = {"", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper", "Carrion Crawler",
                       "Gremlin", "Mimic", "Daemon", "Balrog"};
    if (mdraw == 0) clear(worldbmp);
    while (stop != 1)
    {
        ahead = floor(dng[player.px + player.dx * dist][player.py + player.dy * dist]);
        left = floor(dng[player.px + player.dx * dist + player.dy][player.py + player.dy * dist - player.dx]);
        right = floor(dng[player.px + player.dx * dist - player.dy][player.py + player.dy * dist + player.dx]);
        mtype = floor(ahead / DG_MONSTER);
        ahead = ahead - mtype * DG_MONSTER;
        l1 = per[dist][0];
        r1 = per[dist][1];
        t1 = per[dist][2];
        b1 = per[dist][3];
        l2 = per[dist + 1][0];
        r2 = per[dist + 1][1];
        t2 = per[dist + 1][2];
        b2 = per[dist + 1][3];
        if (mdraw == 1)
            if (mtype > 0)
            {
                if (dist > 1)
                {
                    if (mtype != MON_MIMIC)
                        textout(screen, font, monster[mtype], 225, 167, CLR_LIGHTMAGENTA);
                    else
                        textout(screen, font, "Chest!      ", 226, 167, CLR_LIGHTMAGENTA);
                }
                drawmonsters(mtype, dist);
            }
        if (ahead != DG_WALL & ahead != DG_HIDDEN_DOOR & ahead != DG_DOOR)
        {
            if (left == DG_WALL | left == DG_HIDDEN_DOOR | left == DG_DOOR)
            {
                line(worldbmp, l1, t1, l2, t2, color);
                line(worldbmp, l1, b1, l2, b2, color);
            }
            if (right == DG_WALL | right == DG_HIDDEN_DOOR | right == DG_DOOR)
            {
                line(worldbmp, r1, t1, r2, t2, color);
                line(worldbmp, r1, b1, r2, b2, color);
            }
        }
        if (dist != 0)
        {
            if (ahead == DG_WALL | ahead == DG_HIDDEN_DOOR | ahead == DG_DOOR)
            {
                line(worldbmp, l1, t1, r1, t1, color);
                line(worldbmp, r1, t1, r1, b1, color);
                line(worldbmp, r1, b1, l1, b1, color);
                line(worldbmp, l1, b1, l1, t1, color);
                if (ahead != DG_DOOR) return 0;
            }
            if (ahead == DG_DOOR)
            {
                line(worldbmp, cd[dist][0], cd[dist][3], cd[dist][0], cd[dist][2], color);
                line(worldbmp, cd[dist][0], cd[dist][2], cd[dist][1], cd[dist][2], color);
                line(worldbmp, cd[dist][1], cd[dist][2], cd[dist][1], cd[dist][3], color);
                return 0;
            }
            if (left == DG_DOOR)
            {
                line(worldbmp, ld[dist][0], ld[dist][4], ld[dist][0], ld[dist][2], color);
                line(worldbmp, ld[dist][0], ld[dist][2], ld[dist][1], ld[dist][3], color);
                line(worldbmp, ld[dist][1], ld[dist][3], ld[dist][1], ld[dist][5], color);
            }
            if (right == DG_DOOR)
            {
                line(worldbmp, 319 - ld[dist][0], ld[dist][4], 319 - ld[dist][0], ld[dist][2], color);
                line(worldbmp, 319 - ld[dist][0], ld[dist][2], 319 - ld[dist][1], ld[dist][3], color);
                line(worldbmp, 319 - ld[dist][1], ld[dist][3], 319 - ld[dist][1], ld[dist][5], color);
            }
            if (left != DG_WALL & left != DG_HIDDEN_DOOR & left != DG_DOOR)
            {
                line(worldbmp, l1, t1, l1, b1, color);
                line(worldbmp, l1, t2, l2, t2, color);
                line(worldbmp, l2, t2, l2, b2, color);
                line(worldbmp, l2, b2, l1, b2, color);
            }
            if (right != DG_WALL & right != DG_HIDDEN_DOOR & right != DG_DOOR)
            {
                line(worldbmp, r1, t1, r1, b1, color);
                line(worldbmp, r1, t2, r2, t2, color);
                line(worldbmp, r2, t2, r2, b2, color);
                line(worldbmp, r2, b2, r1, b2, color);
            }
        }
        else
        {
            if (left == DG_DOOR)
            {
                line(worldbmp, 0, ld[dist][2] - 3, ld[dist][1], ld[dist][3], color);
                line(worldbmp, ld[dist][1], ld[dist][3], ld[dist][1], ld[dist][5], color);
            }
            if (right == DG_DOOR)
            {
                line(worldbmp, 319, ld[dist][2] - 3, 319 - ld[dist][1], ld[dist][3], color);
                line(worldbmp, 319 - ld[dist][1], ld[dist][3], 319 - ld[dist][1], ld[dist][5], color);
            }
            if (stop != 1)
            {
                if (left != DG_WALL & left != DG_HIDDEN_DOOR & left != DG_DOOR)
                {
                    line(worldbmp, l1, t2, l2, t2, color);
                    line(worldbmp, l2, t2, l2, b2, color);
                    line(worldbmp, l2, b2, l1, b2, color);
                }
                if (right != DG_WALL & right != DG_HIDDEN_DOOR & right != DG_DOOR)
                {
                    line(worldbmp, r1, t2, r2, t2, color);
                    line(worldbmp, r2, t2, r2, b2, color);
                    line(worldbmp, r2, b2, r1, b2, color);
                }
            }
        }
        if (ahead == DG_LADDER_DOWN | ahead == DG_HOLE)
        {
            line(worldbmp, ft[dist][0], ft[dist][4], ft[dist][2], ft[dist][5], color);
            line(worldbmp, ft[dist][2], ft[dist][5], ft[dist][3], ft[dist][5], color);
            line(worldbmp, ft[dist][3], ft[dist][5], ft[dist][1], ft[dist][4], color);
            line(worldbmp, ft[dist][1], ft[dist][4], ft[dist][0], ft[dist][4], color);
        }
        if (ahead == DG_LADDER_UP)
        {
            line(worldbmp, ft[dist][0], 166 - ft[dist][4], ft[dist][2], 166 - ft[dist][5], color);
            line(worldbmp, ft[dist][2], 166 - ft[dist][5], ft[dist][3], 166 - ft[dist][5], color);
            line(worldbmp, ft[dist][3], 166 - ft[dist][5], ft[dist][1], 166 - ft[dist][4], color);
            line(worldbmp, ft[dist][1], 166 - ft[dist][4], ft[dist][0], 166 - ft[dist][4], color);
        }
        if (ahead == DG_LADDER_DOWN | ahead == DG_LADDER_UP)
        {
            lbottom = lad[dist][3];
            ltop = lad[dist][2];
            lleft = lad[dist][0];
            lright = lad[dist][1];
            line(worldbmp, lleft, lbottom, lleft, ltop, color);
            line(worldbmp, lright, ltop, lright, lbottom, color);
            rung1 = (lbottom * 4 + ltop) / 5;
            rung2 = (lbottom * 3 + ltop * 2) / 5;
            rung3 = (lbottom * 2 + ltop * 3) / 5;
            rung4 = (lbottom + ltop * 4) / 5;
            line(worldbmp, lleft, rung1, lright, rung1, color);
            line(worldbmp, lleft, rung2, lright, rung2, color);
            line(worldbmp, lleft, rung3, lright, rung3, color);
            line(worldbmp, lleft, rung4, lright, rung4, color);
        }
        if (ahead == DG_CHEST & dist != 0)
        {
            line(worldbmp, 159 - 11 / dist, per[dist][3], 159 - 11 / dist, per[dist][3] - 11 / dist, color);
            line(worldbmp, 159 - 11 / dist, per[dist][3] - 11 / dist, 159 + 11 / dist, per[dist][3] - 11 / dist, color);
            line(worldbmp, 159 + 11 / dist, per[dist][3] - 11 / dist, 159 + 11 / dist, per[dist][3], color);
            line(worldbmp, 159 + 11 / dist, per[dist][3], 159 - 11 / dist, per[dist][3], color);
            line(worldbmp, 159 - 11 / dist, per[dist][3] - 11 / dist, 159 - 6 / dist, per[dist][3] - 15 / dist, color);
            line(worldbmp, 159 - 6 / dist, per[dist][3] - 15 / dist, 159 + 16 / dist, per[dist][3] - 15 / dist, color);
            line(worldbmp, 159 + 16 / dist, per[dist][3] - 15 / dist, 159 + 16 / dist, per[dist][3] - 6 / dist, color);
            line(worldbmp, 159 + 16 / dist, per[dist][3] - 6 / dist, 159 + 11 / dist, per[dist][3], color);
            line(worldbmp, 159 + 11 / dist, per[dist][3] - 11 / dist, 159 + 16 / dist, per[dist][3] - 15 / dist, color);
            textout(screen, font, "Chest!", 225, 167, CLR_LIGHTMAGENTA);
        }
        dist++;
    }
done:                                           // never jumped to
    return 0;
}

// The monsters of a level: monster x (1 skeleton ... 10 balrog) lives on levels down to x - 2,
// at a free square; its hit points (mz[x][1]) grow with the level of play and the depth.
int akworld::createmonsters(charsave::savestruct &player)
{
    int count = 0;
    int m;

    for (m = 1; m <= 10; m++)
    {
        mz[m][0] = 0;
        mz[m][1] = m + 3 + player.inout;
        if (m - 2 < player.inout)
        {
            ml[m][0] = floor(random() % 9 + 1);
            ml[m][1] = floor(random() % 9 + 1);
            while (dng[ml[m][0]][ml[m][1]] != DG_FLOOR | (ml[m][0] == player.px & ml[m][1] == player.py))
            {
                ml[m][0] = floor(random() % 9 + 1);
                ml[m][1] = floor(random() % 9 + 1);
            }
            dng[ml[m][0]][ml[m][1]] = m * DG_MONSTER;
            mz[m][0] = 1;
            count++;
            mz[m][1] = m * 2 + player.inout * 2 + player.level;
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
    case MON_SKELETON:
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
    case MON_THIEF:
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
    case MON_GIANT_RAT:
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
    case MON_ORC:
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
    case MON_VIPER:
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
    case MON_CARRION_CRAWLER:
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
    case MON_GREMLIN:
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
        putpixel(worldbmp, x + 4 / dist, y - 8 / dist, CLR_WHITE);
        putpixel(worldbmp, x - 4 / dist, y - 8 / dist, CLR_WHITE);
        break;
    case MON_MIMIC:
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
    case MON_DAEMON:
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
    case MON_BALROG:
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
    int target = 0;
    int damage = 0;
    int gold = 0;
    int key;
    int choice;
    int i;
    int spell;
    char num[8];
    char *weapon[] = {"Food", "Rapier", "Axe", "Shield", "Bow and Arrow", "Magic Amulet"};
    char *monster[] = {"", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper", "Carrion Crawler",
                       "Gremlin", "Mimic", "Daemon", "Balrog"};

again:
    textout(screen, font, "Attack!      ", 8, 167, CLR_LIGHTBLUE);
    textout(screen, font, "Which Weapon?     ", 8, 175, CLR_LIGHTCYAN);
    key = readkey() & 0xff;
    switch (key) {
    case 'r':
        textout(screen, font, weapon[IT_RAPIER], 120, 175, CLR_LIGHTCYAN);
        if (player.owned[IT_RAPIER] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, CLR_LIGHTCYAN);
            goto again;
        }
        if (player.ChClass == 'M')
        {
            textout(screen, font, "Mages can't use Rapiers!", 8, 191, CLR_LIGHTCYAN);
            goto again;
        }
        damage = 10;
        break;
    case 'a':
        textout(screen, font, weapon[IT_AXE], 120, 175, CLR_LIGHTCYAN);
        if (player.owned[IT_AXE] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, CLR_LIGHTCYAN);
            goto again;
        }
        damage = 5;
        break;
    case 's':
        textout(screen, font, weapon[IT_SHIELD], 120, 175, CLR_LIGHTCYAN);
        if (player.owned[IT_SHIELD] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, CLR_LIGHTCYAN);
            goto again;
        }
        damage = 1;
        break;
    case 'b':
        textout(screen, font, weapon[IT_BOW], 120, 175, CLR_LIGHTCYAN);
        if (player.owned[IT_BOW] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, CLR_LIGHTCYAN);
            goto again;
        }
        if (player.ChClass == 'M')
        {
            textout(screen, font, "Mages can't use Bows!", 8, 191, CLR_LIGHTCYAN);
            goto again;
        }
        damage = 4;
        goto ranged;
    case 'm':
        textout(screen, font, weapon[IT_AMULET], 120, 175, CLR_LIGHTCYAN);
        if (player.owned[IT_AMULET] < 1)
        {
            textout(screen, font, "Not Owned!", 8, 183, CLR_LIGHTCYAN);
            goto again;
        }
        if (player.ChClass == 'F')
            spell = rand() % 4 + 1;
        else
        {
            textout(screen, font, "1-Ladder-Up", 8, 183, CLR_LIGHTCYAN);
            textout(screen, font, "2-Ladder-Dn", 130, 183, CLR_LIGHTCYAN);
            textout(screen, font, "3-Kill", 8, 191, CLR_LIGHTCYAN);
            textout(screen, font, "4-Bad??", 130, 191, CLR_LIGHTCYAN);
            choice = readkey() & 0xff;
            if (choice == ASCII_ESC) goto again;
            while (choice < '1' | choice > '4')
            {
                choice = readkey() & 0xff;
                if (choice == ASCII_ESC) goto again;
            }
            spell = choice - '0';
        }
        if (rand() % 100 > 75)
        {
            textout(screen, font, "Last Charge on This Amulet!", 8, 183, CLR_LIGHTCYAN);
            player.owned[IT_AMULET]--;
        }
        switch (spell) {
        case 1:
            textout(screen, font, "Ladder Up                  ", 8, 183, CLR_LIGHTCYAN);
            textout(screen, font, "                           ", 8, 191, CLR_LIGHTCYAN);
            dng[player.px][player.py] = DG_LADDER_UP;
            drawdungeon(player);
            break;
        case 2:
            textout(screen, font, "Ladder Down                ", 8, 183, CLR_LIGHTCYAN);
            textout(screen, font, "                           ", 8, 191, CLR_LIGHTCYAN);
            dng[player.px][player.py] = DG_LADDER_DOWN;
            drawdungeon(player);
            break;
        case 3:
            textout(screen, font, "Magic Attack!              ", 8, 183, CLR_LIGHTCYAN);
            damage = player.inout + 10;
            goto ranged;
        case 4:
            choice = rand() % 3 + 1;
            if (choice == 1)
            {
                textout(screen, font, "You have been turned      ", 8, 183, CLR_LIGHTCYAN);
                textout(screen, font, "into a Toad!              ", 8, 191, CLR_LIGHTCYAN);
                for (i = 1; i <= 4; i++)
                    player.character[i] = 3;
            }
            if (choice == 2)
            {
                textout(screen, font, "You have been turned      ", 8, 183, CLR_LIGHTCYAN);
                textout(screen, font, "into a Lizard Man!        ", 8, 191, CLR_LIGHTCYAN);
                for (i = 0; i <= 4; i++)
                    player.character[i] = floor(player.character[i] * 2.5);
            }
            if (choice == 3)
            {
                textout(screen, font, "Backfire!                 ", 8, 183, CLR_LIGHTRED);
                player.character[ST_HP] = player.character[ST_HP] / 2;
            }
            break;
        }
        return 0;
    }
    if (damage == 0) textout(screen, font, "Hands", 120, 175, CLR_LIGHTCYAN);
    if (damage == 5)
    {
        textout(screen, font, "To throw or swing? ", 8, 183, CLR_LIGHTCYAN);
        key = readkey() & 0xff;
        if (key != 't')
            textout(screen, font, "Swing", 160, 183, CLR_LIGHTCYAN);
        else
        {
            textout(screen, font, "Throw", 160, 183, CLR_LIGHTCYAN);
            player.owned[IT_AXE]--;
ranged:
            for (i = 1; i <= 5; i++)
            {
                if (player.px + player.dx * i < 1 | player.px + player.dx * i > 9
                    | player.py + player.dy * i > 9 | player.py * player.dy * i < 0)
                    goto hit;
                else
                {
                    target = dng[player.px + player.dx * i][player.py + player.dy * i];
                    target = floor(target / DG_MONSTER);
                    if (target > 0) goto hit;
                }
            }
        }
    }
    target = floor(dng[player.px + player.dx][player.py + player.dy] / DG_MONSTER);
hit:
    if (target < 1 | player.character[ST_DEXTERITY] - rand() % 25 < target + player.inout)
    {
        sound(100);
        rest(200);
        sound(0);
        textout(screen, font, "Missed!                    ", 8, 191, CLR_LIGHTCYAN);
    }
    else
    {
        sound(2000);
        rest(200);
        sound(0);
        textout(screen, font, "Hit!                       ", 8, 191, CLR_LIGHTRED);
        if (damage != 0)
            damage = rand() % damage + player.character[ST_STRENGTH] / 5;
        else
            damage = player.character[ST_STRENGTH] / 5;
        mz[target][1] = mz[target][1] - damage;
        textout(screen, font, "Hit Points = ", 48, 191, CLR_LIGHTCYAN);
        itoa(mz[target][1], num, 10);
        textout(screen, font, num, 152, 191, CLR_LIGHTBLUE);
        if (mz[target][1] < 1)
        {
            dng[ml[target][0]][ml[target][1]] = dng[ml[target][0]][ml[target][1]] - target * DG_MONSTER;
            mz[target][0] = 0;
            player.lk = player.lk + (int)floor(target * player.inout / 2);
            if (target == player.task)
            {
                player.task = -player.task;
                sound(1500);
                rest(300);
                sound(2000);
                rest(300);
                sound(0);
            }
            drawdungeon(player);
            textout(screen, font, "Thou hast killed a            ", 8, 175, CLR_LIGHTCYAN);
            textout(screen, font, monster[target], 160, 175, CLR_LIGHTBLUE);
            gold = floor(target + player.inout);
            textout(screen, font, "Thou shalt receive            ", 8, 183, CLR_LIGHTCYAN);
            itoa(gold, num, 10);
            textout(screen, font, num, 160, 183, CLR_LIGHTCYAN);
            textout(screen, font, "Pieces of Eight             ", 8, 191, CLR_LIGHTCYAN);
            player.character[ST_GOLD] = player.character[ST_GOLD] + gold;
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
    int m;
    int stepx;
    int stepy;
    int nexty;
    int nextx;
    int stolen;
    int damage;
    int hit = 0;
    char *num;
    char *monster[] = {"", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper", "Carrion Crawler",
                       "Gremlin", "Mimic", "Daemon", "Balrog"};
    char *item[] = {"Food", "Rapier", "Axe", "Shield", "Bow and Arrow", "Magic Amulet"};

    run1 = 0;
    for (m = 1; m <= 10; m++)
    {
        if (mz[m][0] != 0)
        {
            if (mz[m][1] < ((m + player.inout) * 2 + player.level) * .3)
                ;                               // too weak to fight: it flees (below)
            else
            {
                if ((abs(player.px - ml[m][0]) == 1 & player.py == ml[m][1])
                    | (abs(player.py - ml[m][1]) == 1 & player.px == ml[m][0]))
                {
                    textout(screen, font, "You are being attacked     ", 8, 175, CLR_LIGHTCYAN);
                    textout(screen, font, "by a                       ", 8, 183, CLR_LIGHTCYAN);
                    textout(screen, font, monster[m], 48, 183, CLR_LIGHTBLUE);
                    hit = 1;
                    if (m != MON_THIEF & m != MON_GREMLIN)
                    {
strike:
                        if (rand() % 20 - sgn(player.owned[IT_SHIELD]) - player.character[ST_STAMINA] + m + player.inout < 0)
                        {
                            sound(100);
                            rest(200);
                            sound(0);
                            textout(screen, font, "Missed!             ", 8, 191, CLR_LIGHTCYAN);
                        }
                        else
                        {
                            sound(2000);
                            rest(200);
                            sound(0);
                            textout(screen, font, "Hit!                ", 8, 191, CLR_LIGHTRED);
                            textout(screen, font, "Damage = ", 48, 191, CLR_LIGHTCYAN);
                            damage = rand() % m + player.inout;
                            itoa(damage, num, 10);
                            textout(screen, font, num, 120, 191, CLR_LIGHTBLUE);
                            player.character[ST_HP] = player.character[ST_HP] - damage;
                        }
                    }
                    else
                    {
                        if (rand() % 100 < 90) goto strike;
                        if (m == MON_GREMLIN)
                        {
                            textout(screen, font, "A Gremlin Stole Some Food!", 8, 191, CLR_LIGHTGREEN);
                            player.owned[IT_FOOD] = floor(player.owned[IT_FOOD] / 2);
                        }
                        if (m == MON_THIEF)
                        {
                            stolen = rand() % 6;
                            while (player.owned[stolen] < 1) stolen = rand() % 6;
                            textout(screen, font, "A Thief Stole a", 8, 191, CLR_LIGHTGREEN);
                            textout(screen, font, item[stolen], 136, 191, CLR_LIGHTBLUE);
                            player.owned[stolen] = player.owned[stolen] - 1;
                        }
                    }
                    goto next;
                }
            }
            if (m == MON_MIMIC) goto stay;      // the mimic never moves
            stepx = sgn(player.px - ml[m][0]);
            stepy = sgn(player.py - ml[m][1]);
            if (mz[m][1] < ((m + player.inout) * 2 + player.level) * .3)
            {
                stepx = -stepx;
                stepy = -stepy;
                run1 = 1;
            }
            nexty = dng[ml[m][0]][ml[m][1] + stepy];
            nextx = dng[ml[m][0] + stepx][ml[m][1]];
            if (nexty == DG_WALL | nexty > 9 | nexty == DG_TRAP)
                sgn(nexty);
            else
            {
                dng[ml[m][0]][ml[m][1]] = dng[ml[m][0]][ml[m][1]] - m * DG_MONSTER;
                stepx = 0;
                ml[m][0] = ml[m][0] + stepx;
                ml[m][1] = ml[m][1] + stepy;
                dng[ml[m][0]][ml[m][1]] = dng[ml[m][0]][ml[m][1]] + m * DG_MONSTER;
            }
            if (nextx == DG_WALL | nextx > 9 | nextx == DG_TRAP)
                sgn(nextx);
            else
            {
                dng[ml[m][0]][ml[m][1]] = dng[ml[m][0]][ml[m][1]] - m * DG_MONSTER;
                stepy = 0;
                ml[m][0] = ml[m][0] + stepx;
                ml[m][1] = ml[m][1] + stepy;
                dng[ml[m][0]][ml[m][1]] = dng[ml[m][0]][ml[m][1]] + m * DG_MONSTER;
            }
        }
next:
stay:
        sgn(nexty);
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
    world[1][1] = LAND_TREE;
    world[1][2] = LAND_PLAINS;
    world[1][3] = LAND_TOWN;
    world[3][1] = LAND_CASTLE;
    world[3][2] = LAND_PLAINS;
    world[3][3] = LAND_DUNGEON;
    world[2][1] = LAND_PLAINS;
    world[2][2] = LAND_MOUNTAINS;
    world[2][3] = LAND_PLAINS;
    textout_centre(screen, font, "Ultima 0 - Akalabeth", 160, 167, CLR_LIGHTCYAN);
    textout_centre(screen, font, "Intro - Hit a Key", 160, 175, CLR_LIGHTCYAN);
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
        line(worldbmp, x * 58 + 138, (y - 1) * 52 + 78, x * 58 + 143, (y - 1) * 52 + 78, CLR_LIGHTCYAN);
        line(worldbmp, x * 58 + 141, (y - 1) * 52 + 76, x * 58 + 141, (y - 1) * 52 + 80, CLR_LIGHTCYAN);
        if (i == 4) textout(screen, font, "Quest Please!              ", 8, 183, CLR_LIGHTCYAN);
        if (i == 11 | i == 24) textout(screen, font, "Three Axes to go! ", 8, 183, CLR_LIGHTCYAN);
        if (i == 16) textout(screen, font, "PoW! ", 8, 183, CLR_LIGHTCYAN);
        if (i == 18) textout(screen, font, "Thunk! ", 40, 183, CLR_LIGHTCYAN);
        if (i == 20) textout(screen, font, "ZiFF!", 88, 183, CLR_LIGHTCYAN);
        i++;
        if (i == 26) i = 0;
        rest(300);
    }
    return 0;
}
