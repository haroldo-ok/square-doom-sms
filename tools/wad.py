"""Doom WAD reader: lumps, PLAYPAL, patch decoding (column posts)."""
import os, struct
WAD = os.environ.get('DOOM_WAD', os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'assets', 'DOOM1.WAD'))
_d = open(WAD, 'rb').read()
_n, _off = struct.unpack('<4xII', _d[:12])
LUMPS = {}
ORDER = []
for i in range(_n):
    o, s, name = struct.unpack('<II8s', _d[_off + i * 16:_off + i * 16 + 16])
    name = name.rstrip(b'\0').decode('ascii', 'replace')
    ORDER.append(name)
    if name not in LUMPS: LUMPS[name] = _d[o:o + s]
def lump(name): return LUMPS[name]
PAL = [tuple(LUMPS['PLAYPAL'][i * 3:i * 3 + 3]) for i in range(256)]
def patch(name):
    """-> (w, h, leftoff, topoff, pixels dict (x,y)->palette index)"""
    b = LUMPS[name]
    w, h, lo, to = struct.unpack('<HHhh', b[:8])
    cols = struct.unpack('<%dI' % w, b[8:8 + 4 * w])
    px = {}
    for x, c in enumerate(cols):
        p = c
        while b[p] != 0xFF:
            top = b[p]; n = b[p + 1]
            for i in range(n): px[(x, top + i)] = b[p + 3 + i]
            p += n + 4
    return w, h, lo, to, px
def raw_pic(name, w=320, h=200):
    b = LUMPS[name]
    return [[b[y * w + x] for x in range(w)] for y in range(h)]
