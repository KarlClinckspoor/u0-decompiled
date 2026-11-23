#!/usr/bin/env python3
"""progress.py: progress card (docs/progress.svg) and numbers (docs/progress.json).

AK.EXE rebuilds byte for byte, so the card tracks what is Akalabeth's own code:

  matched  a function of the game's five C++ objects (docs/MAP.md, "The game") that has its C++
           in src/ (its original address is in a comment at its definition in src/*.cc, or it
           is an inline from a header, which the compiler builds from allegro.h)
  attempt  reserved (a draft that is not in src/ yet)
  open     a game function of docs/MAP.md that is not in src/
  aside    set aside because it is not Akalabeth's code: the objects of crt0, Allegro 2.2, DJGPP's
           libc, libgcc and libg++'s iostreams (docs/MAP.md, "All objects in link order"),
           rebuilt or linked as shipped. One tile is one object here, not one function: the
           repository has no list of the library's functions (it would need the game's files).

The percentage is matched / (matched + attempt + open); aside is left out. The count needs only
the repository; tools/build.py is the check that everything is byte-identical.

    python3 tools/progress.py            # writes docs/progress.svg and docs/progress.json
    python3 tools/progress.py --check    # exit 1 if the committed files are stale
"""
import argparse, glob, json, os, re, sys
from html import escape

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAME = 'u0-decompiled'
GAME_OBJECTS = 5        # objects 1-5 of the link are the game (ak, akaux, akchar, akmenu, akworld)
CAPTION = ('Function map: 1 game function per tile; each Aside tile is one library object '
           '(crt0, Allegro, libc, libgcc, iostreams), in link order')
COLORS = dict(matched='#2a8c3c', attempt='#b8860b', open='#64748b', aside='#3b5b8a')
HEADLINE = 'functions matched'
LABELS = dict(matched='Matched', attempt='Attempt', open='Open', aside='Aside')


def read(path):
    with open(path, encoding='utf-8', newline='') as f:
        return f.read().replace('\r\n', '\n')


def map_tables():
    text = read(os.path.join(ROOT, 'docs', 'MAP.md'))
    game = text[text.index('## The game'):text.index('### Classes')]
    funcs = [(m.group(1), m.group(3)) for m in
             re.finditer(r'^\| ([0-9A-F]{6}) \| (\d+) \| (.+) \|$', game, re.M)]
    objs = text[text.index('## All objects in link order'):]
    objects = [(int(m.group(1)), m.group(2)) for m in
               re.finditer(r'^\| (\d+) \| (\S+) \|', objs, re.M)]
    return funcs, objects


def classify():
    funcs, objects = map_tables()
    src = ''.join(read(p) for p in sorted(glob.glob(os.path.join(ROOT, 'src', '*.cc'))))
    in_src = set(re.findall(r'//\s*([0-9A-F]{6})\b', src))
    rows = []
    for n, obj in objects:
        if n == 0:                                    # crt0.s comes first in the link
            rows.append(dict(name=obj, addr='', status='aside'))
    game = []
    for addr, desc in funcs:
        name = re.search(r'`([^`]+)`', desc).group(1)
        ok = addr in in_src or 'inline from a header' in desc
        game.append(dict(name=name, addr=addr, status='matched' if ok else 'open'))
    libs = [dict(name=o, addr='', status='aside') for n, o in objects if n > GAME_OBJECTS]
    return rows + sorted(game, key=lambda r: r['addr']) + libs


def summary(rows):
    c = {k: 0 for k in COLORS}
    for r in rows:
        c[r['status']] += 1
    total = c['matched'] + c['attempt'] + c['open']
    return dict(c, total=total, percent=round(100.0 * c['matched'] / total, 1) if total else 100.0)


def svg(flat):
    s = summary(flat)
    n = len(flat)
    W, PAD = 840, 20
    inner = W - 2 * PAD
    cols = 1
    for cols_try in range(20, 200):      # square tiles of at least 11 px
        if inner / cols_try < 11:
            break
        cols = cols_try
    cell = inner / cols
    top = 160
    H = int(top + -(-n // cols) * cell + 22)
    out = []
    a = out.append
    a('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
      'font-family="Roboto, Helvetica, Arial, sans-serif" role="img" aria-label="%s">'
      % (W, H, W, H, escape('Decompilation progress: %d of %d %s' % (s['matched'], s['total'], HEADLINE))))
    a('<rect width="%d" height="%d" rx="14" fill="#0f172a"/>' % (W, H))
    a('<text x="%d" y="38" font-size="21" fill="#ffffff">Decompilation progress</text>' % PAD)
    a('<text x="%d" y="38" font-size="15" fill="#ffffff" text-anchor="end">%s</text>' % (W - PAD, escape(NAME)))
    a('<text x="%d" y="62" font-size="15" fill="#e5e7eb">%s / %s %s (%.1f%%)</text>'
      % (PAD, format(s['matched'], ','), format(s['total'], ','), HEADLINE, s['percent']))
    a('<rect x="%d" y="74" width="%d" height="18" rx="5" fill="#334155"/>' % (PAD, inner))
    if s['total']:
        a('<rect x="%d" y="74" width="%.2f" height="18" rx="5" fill="%s"/>'
          % (PAD, inner * s['matched'] / s['total'], COLORS['matched']))
    for x, k in zip([PAD, 232, 430, 628], ['matched', 'attempt', 'open', 'aside']):
        a('<rect x="%d" y="104" width="10" height="10" fill="%s"/>' % (x, COLORS[k]))
        a('<text x="%d" y="113" font-size="12" fill="#e5e7eb">%s: %s</text>' % (x + 14, LABELS[k], format(s[k], ',')))
    a('<text x="%d" y="140" font-size="11" fill="#e5e7eb">%s</text>' % (PAD, escape(CAPTION)))
    for i, r in enumerate(flat):
        x, y = PAD + (i % cols) * cell, top + (i // cols) * cell
        a('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="%s" stroke="#0f172a" stroke-width="1">'
          '<title>%s%s (%s)</title></rect>'
          % (x, y, cell, cell, COLORS[r['status']], (r['addr'] + ' ') if r['addr'] else '', escape(r['name']), r['status']))
    a('</svg>')
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    flat = classify()
    js = json.dumps({'all': summary(flat)}, indent=1) + '\n'
    paths = {os.path.join(ROOT, 'docs', 'progress.json'): js, os.path.join(ROOT, 'docs', 'progress.svg'): svg(flat)}
    if args.check:
        stale = [p for p, t in paths.items() if not os.path.exists(p) or read(p) != t]
        if stale:
            print('stale:', *stale)
            sys.exit(1)
        return
    for p, t in paths.items():
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(t)
    r = summary(flat)
    print('matched %d  attempt %d  open %d  aside %d  -> %.1f%% of %d'
          % (r['matched'], r['attempt'], r['open'], r['aside'], r['percent'], r['total']))
    for x in flat:
        if x['status'] == 'open':
            print('open:', x['addr'], x['name'])


if __name__ == '__main__':
    main()
