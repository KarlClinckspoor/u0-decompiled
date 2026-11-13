#!/usr/bin/env python3
"""Wrap a DJGPP COFF program into a WDOSX 0.97 executable the way `stubit` does (phase 7).

    python tools/postlink.py COFF OUT.EXE [--from gamedata/AK.EXE]

The WDOSX kernel and its COFF loader stub ("go32stub, v 2.00T") are WDOSX's binaries. They are
copied at build time from the shipped executable (everything before its WFSE container) and
never committed. What follows is built here, as WDOSX 0.97's util/addfile.asm writes it:

  +00 'WFSE'
  +04 dword  size of the entry: header, chunk table and chunks
  +08 dword  unpacked size (the COFF's size)
  +0C dword  flags: the stub class (6 = DJGPP COFF) | 0x20 (packed with WDOSX-Pack)
  +10        "WdosxMain\\0", then word 6 and the packed size of the last chunk
  +20 dword[n]  offset of each 4096-byte chunk, counted from the end of the table
  then the chunks, each packed on its own (tools/wpack.py); a chunk that does not get smaller
  is stored as it is.

The kernel's $WdX info block already points at the WFSE entry (dword at 0x30) and names the
stub class (byte at 0x27); both are checked against what is built.
"""
import argparse
import hashlib
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import wdosx   # noqa: E402
import wpack   # noqa: E402

CHUNK = 4096
STUB_CLASS_COFF = 6
WFSE_COMP_WPACK = 0x20


def wfse(data, name=b'WdosxMain', stub_class=STUB_CLASS_COFF):
    n = (len(data) + CHUNK - 1) // CHUNK
    chunks = []
    for k in range(n):
        raw = data[k * CHUNK:(k + 1) * CHUNK]
        p = wpack.pack(raw)
        chunks.append(p if len(p) < len(raw) else raw)
    table = []
    off = 0
    for c in chunks:
        table.append(off)
        off += len(c)
    namefield = name + b'\0' + struct.pack('<HI', 6, len(chunks[-1]))
    head_len = 16 + len(namefield)
    size = head_len + 4 * n + off
    head = b'WFSE' + struct.pack('<III', size, len(data), (stub_class & 0x0F) | WFSE_COMP_WPACK)
    return head + namefield + struct.pack('<%dI' % n, *table) + b''.join(chunks)


def prefix_of(exe):
    """Kernel and stub of a WDOSX executable: everything before the first WFSE entry."""
    w = wdosx.WdosxFile(exe)
    return exe[:w.entries[0]['at']] if w.entries else exe


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('coff')
    ap.add_argument('out')
    ap.add_argument('--from', dest='src', default=os.path.join(ROOT, 'gamedata', 'AK.EXE'),
                    help='shipped WDOSX executable to take the kernel and stub from')
    a = ap.parse_args()
    data = open(a.coff, 'rb').read()
    shipped = open(a.src, 'rb').read()
    prefix = prefix_of(shipped)
    if prefix[32:36] != b'$WdX':
        raise SystemExit('%s: no WDOSX info block' % a.src)
    if struct.unpack_from('<I', prefix, 0x30)[0] != len(prefix) or prefix[0x27] != STUB_CLASS_COFF:
        raise SystemExit('%s: the kernel does not point at a COFF WFSE entry after the stub' % a.src)
    out = prefix + wfse(data)
    open(a.out, 'wb').write(out)
    print('%s: %d bytes, sha1 %s' % (a.out, len(out), hashlib.sha1(out).hexdigest()))
    if len(out) == len(shipped):
        print('  %s %s' % ('IDENTICAL to' if out == shipped else 'differs from', a.src))


if __name__ == '__main__':
    main()
