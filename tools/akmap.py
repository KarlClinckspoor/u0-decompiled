"""Akalabeth's generators, reproduced: stats, world map, dungeon levels, monster placement.

    python tools/akmap.py stats LUCKY [ROLLS]       the stat rolls character creation shows
    python tools/akmap.py world LUCKY               the 21x21 world map of a new game
    python tools/akmap.py dungeon LUCKY LEVEL [PX PY]   a dungeon level and its monsters

Everything AK.EXE generates from the lucky number goes through DJGPP 2.01's random()
(src/libc/compat/stdlib/random.c, the BSD additive generator, TYPE_3), seeded with srandom():
- charcreate() seeds srandom(lucky) and rolls six stats per offer;
- createworld() seeds srandom(lucky) again and builds the map;
- createdungeon() seeds srandom(lucky * 3 + level), then createmonsters() continues the same
  sequence, so a level's layout and its monsters' starting squares depend only on the lucky
  number, the level and the square the player stands on.

rand() (combat, traps, gold, the amulet) is a separate generator that the game never seeds; it
is not needed here. The floating-point steps (pow, sqrt, floor) are done with Python doubles;
none of the values involved lies close enough to an integer for the x87's extended precision
to matter (docs/game/random.md).

Rows are y (north at the top: the up arrow decrements y on the world map), columns are x.
"""

import math
import sys

M32 = 0xFFFFFFFF


def s32(v):
    v &= M32
    return v - (1 << 32) if v & 0x80000000 else v


class Random:
    """DJGPP 2.01 srandom()/random(): degree 31, separation 3."""

    DEG, SEP = 31, 3

    def __init__(self, seed):
        st = [0] * self.DEG
        st[0] = s32(seed)
        for i in range(1, self.DEG):
            st[i] = s32(1103515245 * st[i - 1] + 12345)
        self.st, self.f, self.r = st, self.SEP, 0
        for _ in range(10 * self.DEG):
            self()

    def __call__(self):
        st = self.st
        st[self.f] = s32(st[self.f] + st[self.r])
        v = (st[self.f] >> 1) & 0x7FFFFFFF
        self.f += 1
        if self.f >= self.DEG:
            self.f = 0
            self.r += 1
        else:
            self.r += 1
            if self.r >= self.DEG:
                self.r = 0
        return v


def pos(rnd):
    """floor(random() % 100 * .01 * 19 + 1): 1..19"""
    return int(math.floor(rnd() % 100 * .01 * 19 + 1))


MONSTERS = ["", "Skeleton", "Thief", "Giant Rat", "Orc", "Viper", "Carrion Crawler",
            "Gremlin", "Mimic", "Daemon", "Balrog"]
STATS = ["Hit Points", "Strength", "Dexterity", "Stamina", "Wisdom", "Gold"]


def stats(lucky, rolls=3):
    rnd = Random(lucky)
    out = []
    for _ in range(rolls):
        out.append([int(math.floor(math.sqrt(rnd() % 100 * .01) * 21 + 4)) for _ in range(6)])
    return out


def world(lucky):
    """createworld(0, ...): returns (map[x][y], (tx, ty), castle)"""
    rnd = Random(lucky)
    w = [[0] * 21 for _ in range(21)]
    for x in range(21):
        w[x][0] = w[0][x] = w[x][20] = w[20][x] = 1
    for x in range(1, 20):
        for y in range(1, 20):
            w[x][y] = int(math.floor(math.pow(rnd() % 100 * .01, 5) * 4.5))
            # the town "thinning" sets 3 to 3 and calls rand(), not random(): nothing to do
    cx = pos(rnd)
    cy = pos(rnd)
    w[cx][cy] = 5
    tx = pos(rnd)
    ty = pos(rnd)
    w[tx][ty] = 3
    return w, (tx, ty), (cx, cy)


def dungeon(lucky, level, px=1, py=1):
    """createdungeon + createmonsters: returns (dng[x][y], {monster: (x, y, hp)})"""
    rnd = Random(s32(lucky * 3 + level))
    d = [[0] * 11 for _ in range(11)]
    for x in range(11):
        d[x][0] = d[x][10] = d[0][x] = d[10][x] = 1
    for x in range(2, 9, 2):
        for y in range(1, 10):
            d[x][y] = 1
            d[y][x] = 1
    for x in range(2, 9, 2):
        for y in range(1, 10, 2):
            if rnd() % 100 > 95: d[x][y] = 2
            if rnd() % 100 > 95: d[y][x] = 2
            if rnd() % 100 > 70: d[x][y] = 3
            if rnd() % 100 > 70: d[y][x] = 3
            if rnd() % 100 > 40: d[x][y] = 4
            if rnd() % 100 > 40: d[y][x] = 4
            if rnd() % 100 > 97: d[y][x] = 9
            if rnd() % 100 > 97: d[x][y] = 9
            if rnd() % 100 > 94: d[x][y] = 5
            if rnd() % 100 > 94: d[y][x] = 5
    d[2][1] = 0
    if level % 2 == 0:
        d[7][3], d[3][7] = 7, 8
    else:
        d[7][3], d[3][7] = 8, 7
    if level == 1:
        d[1][1] = 8
        d[7][3] = 0
    mons = {}
    for m in range(1, 11):
        if m - 2 < level:
            mx, my = rnd() % 9 + 1, rnd() % 9 + 1
            while d[mx][my] != 0 or (mx == px and my == py):
                mx, my = rnd() % 9 + 1, rnd() % 9 + 1
            d[mx][my] = m * 10
            mons[m] = (mx, my, m * 2 + level * 2)     # + the level of play
    return d, mons


WCH = {0: '.', 1: '^', 2: 't', 3: 'T', 4: 'D', 5: 'C'}
DCH = {0: '.', 1: '#', 2: 'x', 3: 'h', 4: '+', 5: '$', 7: '>', 8: '<', 9: 'v'}
MCH = '?123456789B'


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    cmd, lucky = argv[1], int(argv[2])
    if cmd == 'stats':
        rolls = int(argv[3]) if len(argv) > 3 else 3
        for k, s in enumerate(stats(lucky, rolls), 1):
            print('offer %d: ' % k + ', '.join('%s %d' % (n, v) for n, v in zip(STATS, s)))
    elif cmd == 'world':
        w, (tx, ty), (cx, cy) = world(lucky)
        for y in range(21):
            print(' '.join('@' if (x, y) == (tx, ty) else WCH[w[x][y]] for x in range(21)))
        print('start (town) at x=%d y=%d, castle at x=%d y=%d%s' % (
            tx, ty, cx, cy, '' if w[cx][cy] == 5 else ' (overwritten by the start town)'))
        n = {}
        for x in range(1, 20):
            for y in range(1, 20):
                n[w[x][y]] = n.get(w[x][y], 0) + 1
        print('inner squares: ' + ', '.join('%s %d' % (k, n.get(v, 0)) for v, k in (
            (0, 'plains'), (1, 'mountains'), (2, 'trees'), (3, 'towns'), (4, 'dungeons'),
            (5, 'castle'))))
        print('legend: . plains  ^ mountains  t tree  T town  D dungeon  C castle  @ start')
    elif cmd == 'dungeon':
        level = int(argv[3])
        px, py = (int(argv[4]), int(argv[5])) if len(argv) > 5 else (1, 1)
        d, mons = dungeon(lucky, level, px, py)
        for y in range(11):
            row = []
            for x in range(11):
                v = d[x][y]
                row.append('@' if (x, y) == (px, py) else MCH[v // 10] if v >= 10 else DCH[v])
            print(' '.join(row))
        for m, (x, y, hp) in mons.items():
            print('%-16s at x=%d y=%d, %d hit points + level of play' % (MONSTERS[m], x, y, hp))
        print('legend: # wall  . floor  x trap  h hidden door  + door  $ chest  > ladder down  '
              'v hole down  < ladder up  1-9,B monster  @ player')
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main(sys.argv)
