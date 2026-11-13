#!/usr/bin/env python3
"""Read a WDOSX 0.97 executable (AK.EXE, SETUP.EXE) and unpack what it carries.

Layout of a WDOSX file, as found in AK.EXE:

  0000      MZ header of the WDOSX kernel (32 bytes), then '$WdX' + version info,
            the copyright text, the 16-bit LZ decoder (`unpack` below) and the packed kernel.
            Entry CS:IP 0000:00FB calls 00E0, which moves the packed bytes up and unpacks
            them in place from 00FE, then returns into the unpacked kernel.
  kernel    the inner MZ: WDOSX's version of the DJGPP stub ('go32stub, v 2.00T'),
  end       starting at the outer MZ's file size; its own MZ size says where the next part is.
  WFSE      the embedded file system. Each entry:
              +00 'WFSE'
              +04 dword  entry size in the file, header included
              +08 dword  unpacked size
              +0C dword  flags: stub class (6 = DJGPP COFF) | 0x20 (WDOSX-Pack compressed)
              +10 16 bytes  file name "WdosxMain", zero-terminated, then word 6 and the
                         packed size of the last chunk (WDOSX 0.97 util/addfile.asm)
              +20 dword[n]  offsets of the n packed 4096-byte chunks, from the end of the table
              then the chunks, each an independent LZ stream (`unpack`); a chunk whose packed
              span equals its unpacked size is stored as it is.
            The program the stub runs is the entry named 'WdosxMain' (a DJGPP COFF image).

The LZ format is WDOSX-Pack 1.07 by Joergen Ibsen (tools/wpack.py packs it; tools/postlink.py
builds a whole WFSE entry). As the kernel's own decoder at 0083 (16-bit code) reads it:
  bits are read MSB first from a tag byte refilled from the stream; the first byte is a literal.
  0      literal byte
  1 L O  match: L and O are gamma codes (start at 1; repeat: value = 2*value + bit, continue
         while the next bit is 1). O -= 2, and 1 more if the previous item was a literal.
         O < 0 (only right after a literal): copy L bytes from the previous match offset.
         Otherwise offset = ((O << 6) | next 6 bits) + 1, and L += 1 when offset >= 0x781.

usage:  wdosx.py info EXE
        wdosx.py unpack EXE [-o OUTDIR]      (writes each WFSE entry, the kernel and the stub)
"""
import argparse
import hashlib
import os
import struct
import sys

CHUNK = 4096


class LZError(Exception):
    pass


def unpack(src, outlen, pos=0):
    """Decode one LZ stream from src[pos:] until outlen bytes are produced.
    Returns (bytes, position after the last byte read)."""
    out = bytearray()
    s = pos
    tag = 0x80

    def bit():
        nonlocal tag, s
        tag <<= 1
        if tag & 0xFF == 0:
            tag = (src[s] << 1) | (tag >> 8)
            s += 1
        b = tag >> 8
        tag &= 0xFF
        return b

    def gamma():
        v = 1
        while True:
            v = v * 2 + bit()
            if not bit():
                return v

    out.append(src[s])
    s += 1
    lastlit = True
    prev = 0
    while len(out) < outlen:
        if not bit():
            out.append(src[s])
            s += 1
            lastlit = True
            continue
        length = gamma()
        o = gamma() - 2 - (1 if lastlit else 0)
        lastlit = False
        if o < 0:
            off = prev
        else:
            o |= 0x400
            while not o & 0x10000:
                o = o * 2 + bit()
            off = ((o & 0xFFFF) + 1) & 0xFFFF
            if off >= 0x781:
                length += 1
            prev = off
        if off == 0 or off > len(out):
            raise LZError('offset %d at output %d' % (off, len(out)))
        for _ in range(length):
            out.append(out[-off])
    if len(out) != outlen:
        raise LZError('stream overran the end: %d > %d' % (len(out), outlen))
    return bytes(out), s


def mz_size(d, at=0):
    if d[at:at + 2] != b'MZ':
        raise ValueError('no MZ header at %x' % at)
    last, pages = struct.unpack_from('<HH', d, at + 2)
    return (pages - 1) * 512 + last if last else pages * 512


class WdosxFile:
    def __init__(self, data):
        self.data = data
        d = data
        self.kernel_end = mz_size(d)
        hdr = struct.unpack_from('<H', d, 8)[0] * 16
        if d[hdr:hdr + 4] != b'$WdX':
            raise ValueError('not a WDOSX file (no $WdX after the MZ header)')
        self.version = d[hdr + 0x14:hdr + 0x60].split(b' DOS')[0].decode('ascii', 'replace')
        self.stub_start = self.kernel_end
        self.stub_end = self.stub_start + mz_size(d, self.stub_start)
        self.entries = []
        p = self.stub_end
        while p + 0x20 <= len(d) and d[p:p + 4] == b'WFSE':
            size, usize, unk = struct.unpack_from('<III', d, p + 4)
            name = d[p + 0x10:p + 0x20].split(b'\0')[0].decode('ascii', 'replace')
            n = (usize + CHUNK - 1) // CHUNK
            offs = list(struct.unpack_from('<%dI' % n, d, p + 0x20))
            self.entries.append(dict(at=p, size=size, usize=usize, unk=unk, name=name,
                                     nameraw=d[p + 0x10:p + 0x20], offs=offs,
                                     base=p + 0x20 + 4 * n))
            p += size
        self.end = p

    def unpack_kernel(self):
        """The kernel's 16-bit image unpacked as it is in memory (load offset 0 = file 20h)."""
        d = self.data
        hdr = struct.unpack_from('<H', d, 8)[0] * 16
        img = d[hdr:self.kernel_end]
        # 00E0: mov si,src_end / mov di,dst_end / mov cx,count / std / rep movsb, then
        # bx=outlen, si=di+1, di=00FE: read the operands instead of assuming them.
        if img[0xE0:0xE5] != bytes.fromhex('0e1f1e07be') or img[0xEB] != 0xB9:
            raise ValueError('unexpected kernel loader at 00E0')
        outlen = struct.unpack_from('<H', img, 0xF5)[0]
        out, end = unpack(img + bytes(0x10), outlen, 0xFE)
        return bytes(img[:0xFE]) + out, end

    def unpack_entry(self, e):
        d = self.data
        out = bytearray()
        n = len(e['offs'])
        for i, off in enumerate(e['offs']):
            ln = min(CHUNK, e['usize'] - CHUNK * i)
            stop = e['base'] + (e['offs'][i + 1] if i + 1 < n else e['at'] + e['size'] - e['base'])
            if stop - (e['base'] + off) == ln:
                # a chunk that does not get smaller is stored as it is (SETUP.EXE 66, 67)
                out += d[e['base'] + off:stop]
                continue
            chunk, pos = unpack(d, ln, e['base'] + off)
            if pos != stop:
                raise LZError('%s chunk %d ends at %x, next starts at %x' % (e['name'], i, pos, stop))
            out += chunk
        return bytes(out)


def sha1(b):
    return hashlib.sha1(b).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['info', 'unpack'])
    ap.add_argument('exe')
    ap.add_argument('-o', '--outdir', default='build')
    a = ap.parse_args()
    w = WdosxFile(open(a.exe, 'rb').read())
    print('%s: %d bytes, %s' % (a.exe, len(w.data), w.version))
    print('  kernel   0000-%04X' % w.kernel_end)
    print('  stub     %04X-%04X  (%s)' % (w.stub_start, w.stub_end,
          w.data[w.stub_start:w.stub_end].split(b'go32stub')[1][:10].strip(b'\0 ,').decode()
          if b'go32stub' in w.data[w.stub_start:w.stub_end] else '?'))
    for e in w.entries:
        print('  WFSE %-10s %05X-%05X  packed %d, unpacked %d, %d chunks, field 0C = %#x, name field %s'
              % (e['name'], e['at'], e['at'] + e['size'], e['size'], e['usize'], len(e['offs']),
                 e['unk'], e['nameraw'].hex()))
    if w.end != len(w.data):
        print('  %d bytes after the last WFSE entry' % (len(w.data) - w.end))
    if a.cmd == 'unpack':
        os.makedirs(a.outdir, exist_ok=True)
        stem = os.path.splitext(os.path.basename(a.exe))[0].upper()
        k, kend = w.unpack_kernel()
        files = {stem + '.KRN': k, stem + '.STB': w.data[w.stub_start:w.stub_end]}
        for e in w.entries:
            ext = '.COF' if e['name'] == 'WdosxMain' else '.' + e['name']
            files[stem + ext] = w.unpack_entry(e)
        for name, b in files.items():
            path = os.path.join(a.outdir, name)
            open(path, 'wb').write(b)
            print('  wrote %s (%d bytes, sha1 %s)' % (path, len(b), sha1(b)))


if __name__ == '__main__':
    sys.exit(main())
