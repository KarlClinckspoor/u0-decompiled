#!/usr/bin/env python3
"""Unpack the toolchain from the archives in tools/ into build/tc/.

    python tools/setup.py [--archives DIR] [--clean]

The archives are not committed (docs/HANDOFF.md lists each with its source and sha1):

  djdev201.zip  DJGPP 2.01 (crt0, libc, headers, stubify, djgpp.env)   -> build/tc/DJGPP
  bnu27b.zip    binutils 2.7                                           -> build/tc/DJGPP
  bnu281b.zip   binutils 2.8.1, unpacked over 2.7                      -> build/tc/DJGPP
  mak375b.zip   make 3.75                                              -> build/tc/DJGPP
  gcc2721b.zip  gcc 2.7.2.1                                            -> build/tc/DJGPP
  gpp2721b.zip  cc1plus 2.7.2.1                                        -> build/tc/DJGPP
  lgp271b.zip   libg++ 2.7.1                                           -> build/tc/DJGPP
  csdpmi4b.zip  CWSDPMI r4 (BIN/CWSDPMI.EXE)                           -> build/tc/DJGPP
  alleg22.zip   Allegro 2.2 source                                     -> build/tc/ALLEGRO
  djlsr201.zip  DJGPP 2.01 libc sources (reference)                    -> build/tc/DJLSR

All are plain zips with 8.3 names, so Python's zipfile is enough (no 7-Zip).
The sha1 of each archive is checked first.
"""
import argparse
import hashlib
import os
import shutil
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TC = os.path.join(ROOT, 'build', 'tc')

ARCHIVES = [  # (file, sha1, target directory under build/tc)
    ('djdev201.zip', 'ef2129363cdc031c1163543b9dbdf4e763750173', 'DJGPP'),
    ('bnu27b.zip', '89696fbc9766b32ab5d554a1df591f7093dbbcb0', 'DJGPP'),
    # binutils 2.8.1 replaces 2.7's as/ld: the game's alignment fillers (89 f6, 8d 76 00) and
    # its kept L labels are 2.8's, not 2.7's (docs/HANDOFF.md)
    ('bnu281b.zip', '7230cb4ab1e3a8a11312fce8c7634e265059cc53', 'DJGPP'),
    # binutils 2.7 again, on its own, for liballeg.a: Allegro in AK.EXE has 2.7's fillers.
    # Select it with gcc -BD:/BNU27/BIN/
    ('bnu27b.zip', '89696fbc9766b32ab5d554a1df591f7093dbbcb0', 'DJGPP/BNU27'),
    ('mak375b.zip', '2ba09b8fdbb0af7fe955c4966c6083ad8ff512a0', 'DJGPP'),
    ('gcc2721b.zip', 'a52d1dfce480ad64b30cc24a6febc475d3cad792', 'DJGPP'),
    ('gpp2721b.zip', '6e2b72e9b60fb54aa79177b97c39f490e1cc474f', 'DJGPP'),
    ('lgp271b.zip', 'c13396c901ca3c2ebd8f01a6f751723146fc0296', 'DJGPP'),
    ('csdpmi4b.zip', '4f56ee401bafa6e8881e96dbd85794a1b08dc690', 'DJGPP'),
    ('alleg22.zip', '56f8c1362a3f2920b584201cc159faa0f80e0162', 'ALLEGRO'),
    ('djlsr201.zip', '9dd9de039f632093f03ba6ca53059dd3388209b5', 'DJLSR'),
]


def unzip(path, dest):
    """Extract with upper-case 8.3 names (as DOS sees them), refusing paths that leave dest."""
    root = os.path.abspath(dest)
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            name = info.filename.replace('\\', '/').upper()
            target = os.path.abspath(os.path.join(root, *name.rstrip('/').split('/')))
            if not target.startswith(root + os.sep):
                raise SystemExit('%s: refusing %s' % (path, info.filename))
            if info.is_dir():
                os.makedirs(target, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with z.open(info) as src, open(target, 'wb') as dst:
                shutil.copyfileobj(src, dst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--archives', default=HERE)
    ap.add_argument('--clean', action='store_true', help='remove build/tc first')
    a = ap.parse_args()
    if a.clean and os.path.isdir(TC):
        shutil.rmtree(TC)
    for name, sha1, sub in ARCHIVES:
        path = os.path.join(a.archives, name)
        if not os.path.exists(path):
            raise SystemExit('missing archive: %s (docs/HANDOFF.md says where it comes from)' % path)
        got = hashlib.sha1(open(path, 'rb').read()).hexdigest()
        if got != sha1:
            raise SystemExit('%s: sha1 %s, expected %s' % (name, got, sha1))
        unzip(path, os.path.join(TC, sub))
        print('%-13s -> build/tc/%s' % (name, sub))


if __name__ == '__main__':
    sys.exit(main())
