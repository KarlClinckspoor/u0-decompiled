#!/usr/bin/env python3
"""Read AK.DAT, the Allegro 2.2 datafile the game loads.

usage:  akdat.py list [AK.DAT]                 the objects with their properties
        akdat.py extract [AK.DAT] [-o DIR]     each object's data to DIR (default build/akdat),
                                               bitmaps also as .ppm with the file's palette

Format (Allegro 2.2, src/file.c and src/datafile.c):
  file      'slh!' then an LZSS stream (the whole file is one packed packfile)
  LZSS      4096-byte ring buffer filled with zeros, writing starts at 4096-18; a flag byte
            is read LSB first: 1 = literal byte, 0 = a pair c1 c2: position c1 | (c2 & F0) << 4,
            length (c2 & 0F) + 3
  datafile  'ALL.' + object count (big-endian), then each object:
              properties: 'prop' + type (4 chars) + length (BE) + text, repeated
              type (4 chars) + packed size (BE) + unpacked size (BE) + data
            a negative size would mean the object is packed on its own (not seen in AK.DAT)
  BMP       bits per pixel (BE word), width (BE word), height (BE word), pixels (8-bit: one byte)
  PAL       256 x (r, g, b), 0-63 each
  FONT      8 (8x8 font: 95 x 8 bytes) or 16 (8x16: 95 x 16 bytes), as a BE word first
"""
import argparse
import os
import struct
import sys


def lzss_unpack(src, pos=0):
    N, F = 4096, 18
    ring = bytearray(N)
    r = N - F
    out = bytearray()
    flags = 0
    while pos < len(src):
        flags >>= 1
        if not flags & 0x100:
            flags = src[pos] | 0xFF00
            pos += 1
            if pos >= len(src):
                break
        if flags & 1:
            c = src[pos]
            pos += 1
            out.append(c)
            ring[r] = c
            r = (r + 1) & (N - 1)
        else:
            if pos + 1 >= len(src):
                break
            c1, c2 = src[pos], src[pos + 1]
            pos += 2
            i = c1 | ((c2 & 0xF0) << 4)
            for k in range((c2 & 0x0F) + 3):
                c = ring[(i + k) & (N - 1)]
                out.append(c)
                ring[r] = c
                r = (r + 1) & (N - 1)
    return bytes(out)


def be32(b, o):
    return struct.unpack_from('>i', b, o)[0]


def read_datafile(raw):
    if raw[:4] == b'slh!':
        raw = lzss_unpack(raw, 4)
    if raw[:4] != b'ALL.':
        raise ValueError('not an Allegro datafile (no ALL.)')
    n = be32(raw, 4)
    p = 8
    objs = []
    for _ in range(n):
        props = {}
        while raw[p:p + 4] == b'prop':
            ptype = raw[p + 4:p + 8].decode('latin-1')
            ln = be32(raw, p + 8)
            props[ptype] = raw[p + 12:p + 12 + ln].decode('latin-1')
            p += 12 + ln
        otype = raw[p:p + 4].decode('latin-1')
        size, usize = be32(raw, p + 4), be32(raw, p + 8)
        data = raw[p + 12:p + 12 + abs(size)]
        p += 12 + abs(size)
        objs.append(dict(type=otype, size=size, usize=usize, props=props, data=data))
    return objs, raw, p


def describe(o):
    d = o['data']
    if o['type'] == 'BMP ' and len(d) >= 6:
        bpp, w, h = struct.unpack_from('>hHH', d, 0)
        return '%dx%d, %d bpp' % (w, h, bpp)
    if o['type'] == 'FONT' and len(d) >= 2:
        return 'font height %d' % struct.unpack_from('>h', d, 0)[0]
    if o['type'] == 'PAL ':
        return '%d colours' % (len(d) // 3)
    return ''


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['list', 'extract'])
    ap.add_argument('file', nargs='?', default='gamedata/AK.DAT')
    ap.add_argument('-o', '--outdir', default='build/akdat')
    a = ap.parse_args()
    objs, raw, end = read_datafile(open(a.file, 'rb').read())
    print('%s: %d objects, %d bytes unpacked (%d used)' % (a.file, len(objs), len(raw), end))
    for k, o in enumerate(objs):
        print('%3d %s %6d %-18s %-14s %s' % (k, o['type'], len(o['data']), o['props'].get('NAME', ''),
                                            describe(o), ' '.join('%s=%s' % kv for kv in o['props'].items()
                                                                  if kv[0] != 'NAME')))
    if a.cmd == 'extract':
        os.makedirs(a.outdir, exist_ok=True)
        pal = next((o['data'] for o in objs if o['type'] == 'PAL '), None)
        for k, o in enumerate(objs):
            name = '%02d_%s' % (k, o['props'].get('NAME', o['type'].strip()) or o['type'].strip())
            open(os.path.join(a.outdir, name + '.' + o['type'].strip().lower()), 'wb').write(o['data'])
            if o['type'] == 'BMP ' and pal:
                bpp, w, h = struct.unpack_from('>hHH', o['data'], 0)
                if bpp == 8:
                    px = o['data'][6:6 + w * h]
                    rgb = bytearray()
                    for c in px:
                        rgb += bytes(min(255, v * 4 + v // 16) for v in pal[3 * c:3 * c + 3])
                    with open(os.path.join(a.outdir, name + '.ppm'), 'wb') as f:
                        f.write(b'P6 %d %d 255\n' % (w, h) + bytes(rgb))
        print('extracted to', a.outdir)


if __name__ == '__main__':
    sys.exit(main())
