#!/usr/bin/env python3
"""WDOSX-Pack 1.07 (Joergen Ibsen, 1999-2001), the packer of WDOSX's stubit, in Python.

A line-by-line port of WdosxPack() from WDOSX 0.97's src/wpack/wpack.c, so that the output is
the same bytes. The format is the one `tools/wdosx.py` decodes (aPLib's predecessor): a literal
first byte, then tag bits read MSB first, 0 = literal, 1 = match with Gamma2 length and offset.

    from wpack import pack
    packed = pack(data)          # data: up to about 64 KB (WDOSX packs 4096-byte chunks)
"""

LEN_2_LIMIT = 1920


class _Packer:
    def __init__(self, src):
        self.src = src
        self.out = bytearray()
        self.tag = None          # index of the current tag byte in out
        self.bitcount = 0
        self.lastmatchpos = -1
        self.lastwasmatch = 0
        # lookup[b0 << 8 | b1] = latest entry number; back[entry] = previous entry with the same
        # pair. Entry n stands for src[n - 1] (wpack.c's source_offs = source - 1).
        self.lookup = {}
        self.back = [0]
        self.backptr = 0

    # -------------------------------------------------------------- bit output
    def bit(self, b):
        if self.bitcount == 0:
            self.bitcount = 7
            self.tag = len(self.out)
            self.out.append(0)
        else:
            self.bitcount -= 1
        self.out[self.tag] = ((self.out[self.tag] << 1) + (1 if b else 0)) & 0xFF

    def gamma(self, val):
        invert = 0
        n = 0
        while True:
            invert = (invert << 1) | (val & 1)
            n += 1
            val >>= 1
            if val <= 1:
                break
        n -= 1
        while n:
            self.bit(invert & 1)
            self.bit(1)
            invert >>= 1
            n -= 1
        self.bit(invert & 1)
        self.bit(0)

    def literal(self, at):
        self.lastwasmatch = 0
        self.bit(0)
        self.out.append(self.src[at])

    def low6(self, pos):
        for m in (0x20, 0x10, 0x08, 0x04, 0x02, 0x01):
            self.bit(pos & m)

    def codepair(self, pos, length, at):
        if self.lastwasmatch:
            if pos > LEN_2_LIMIT and length == 2:
                self.literal(at)
                self.literal(at + 1)
            else:
                self.bit(1)
                if pos > LEN_2_LIMIT:
                    length -= 1
                self.gamma(length)
                self.lastmatchpos = pos
                pos -= 1
                self.gamma((pos >> 6) + 2)
                self.low6(pos)
        else:
            self.lastwasmatch = 1
            if pos > LEN_2_LIMIT and length == 2 and pos != self.lastmatchpos:
                self.literal(at)
                self.literal(at + 1)
            else:
                self.bit(1)
                if pos > LEN_2_LIMIT and pos != self.lastmatchpos:
                    length -= 1
                self.gamma(length)
                if pos == self.lastmatchpos:
                    self.bit(0)
                    self.bit(0)
                else:
                    self.lastmatchpos = pos
                    pos -= 1
                    self.gamma((pos >> 6) + 3)
                    self.low6(pos)

    # -------------------------------------------------------------- matching
    def findmatch(self, buf, lookback, lookforward):
        """Best (len, pos) for src[buf:], pos = distance back. wpack.c's findmatch()."""
        src = self.src
        lookup = self.lookup
        back = self.back
        while self.backptr < buf:
            key = src[self.backptr] << 8 | src[self.backptr + 1]
            back.append(lookup.get(key, 0))
            lookup[key] = len(back) - 1
            self.backptr += 1
        bestlen = 0
        bestpos = 0
        nxt = src[buf + 1] if buf + 1 < len(src) else 0      # read past the end: unused
        backpos = lookup.get(src[buf] << 8 | nxt, 0)
        if backpos and lookforward > 1:
            ptr = backpos - 1
            while ptr >= buf and backpos:
                backpos = back[backpos]
                ptr = backpos - 1
            last = self.lastmatchpos
            while backpos and buf - ptr <= lookback:
                if src[ptr + bestlen] == src[buf + bestlen]:
                    ml = 2
                    while ml < lookforward and src[ptr + ml] == src[buf + ml]:
                        ml += 1
                    if ml + (buf - ptr == last) > bestlen + (bestpos == last):
                        bestlen = ml
                        if bestlen == lookforward:
                            backpos = 0
                        bestpos = buf - ptr
                if backpos:
                    backpos = back[backpos]
                ptr = backpos - 1
        if bestpos > LEN_2_LIMIT and bestlen == 2 and bestpos != self.lastmatchpos:
            bestlen = bestpos = 0
        return bestlen, bestpos

    # -------------------------------------------------------------- main
    def run(self):
        src = self.src
        length = len(src)
        self.out.append(src[0])
        self.bitcount = 8
        self.tag = len(self.out)
        self.out.append(0)
        inb = 1
        pos = 1
        lastpos = -1
        literalcount = 0
        lit_len = lit_pos = 0
        nxt_len = nxt_pos = 0
        while pos < length:
            if pos == lastpos:
                m_len, m_pos = nxt_len, nxt_pos
            else:
                m_len, m_pos = self.findmatch(inb, pos, length - pos)
            if m_len:
                nxt_len, nxt_pos = self.findmatch(inb + 1, pos + 1, length - (pos + 1))
                lastpos = pos + 1
            else:
                nxt_len = 0
            last = self.lastmatchpos
            if m_len and m_len + (m_pos == last) >= nxt_len + (nxt_pos == last):
                if literalcount:
                    if literalcount == 1:
                        self.literal(inb - 1)
                    else:
                        t_len, t_pos = self.findmatch(inb - literalcount, lit_pos, literalcount)
                        if t_len >= literalcount:
                            self.codepair(t_pos, literalcount, inb - literalcount)
                        else:
                            self.codepair(lit_pos, literalcount, inb - literalcount)
                    literalcount = 0
                self.codepair(m_pos, m_len, inb)
                inb += m_len
                pos += m_len
            else:
                if literalcount:
                    literalcount += 1
                    if literalcount == lit_len:
                        self.codepair(lit_pos, literalcount, inb - literalcount + 1)
                        literalcount = 0
                else:
                    if m_len:
                        lit_len, lit_pos = m_len, m_pos
                        literalcount += 1
                    else:
                        self.literal(inb)
                inb += 1
                pos += 1
        if literalcount:
            if literalcount == 1:
                self.literal(inb - 1)
            else:
                self.codepair(lit_pos, literalcount, inb - literalcount)
        # EOD: move the last tag byte's bits into place
        self.out[self.tag] = (self.out[self.tag] << self.bitcount) & 0xFF
        return bytes(self.out)


def pack(data):
    if not data:
        return b''
    return _Packer(bytes(data)).run()
