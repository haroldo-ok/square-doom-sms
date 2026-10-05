"""Doom 1.9 weapon sprites -> SMS weapon poses. Uses Doom's full-screen view
geometry (320x200, centre y 100, no status bar), scaled 0.8 x / 0.85 y with the
bottom of the Doom screen at the bottom of the SMS screen."""
import sys, os, itertools
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wad
SX, SY = 0.8, 0.85
LV = [0, 85, 170, 255]
def sms_of(rgb):
    return tuple(min(range(4), key=lambda i: abs(LV[i] - c)) for c in rgb)
# sprite palette entries fixed by the view's colour pairs (C64 colours in SMS RGB)
FIXED = {1: (1,1,0), 2: (2,2,2), 3: (2,0,0), 4: (3,3,1), 5: (3,3,3), 6: (1,1,3), 7: (2,1,1), 8: (0,0,0)}
FREE = [9, 10, 11, 12, 13, 15]
def place(name, dx=0, dy=0):
    w, h, lo, to, px = wad.patch(name)
    x1 = 1 - lo + dx; y1 = 32 - to + dy
    out = {}
    X0 = int(x1 * SX) - 1; X1 = int((x1 + w) * SX) + 2
    Y0 = int(192 - (200 - y1) * SY) - 1; Y1 = int(192 - (200 - (y1 + h)) * SY) + 2
    for Y in range(max(0, Y0), min(192, Y1)):
        for X in range(max(0, X0), min(256, X1)):
            # HUD corners on row 23 stay visible (C64 / Doom draw the bar on top)
            if Y >= 184 and (X < 64 or X >= 192): continue
            sx = int((X + 0.5) / SX) - x1; sy = int(200 - (192 - Y - 0.5) / SY) - y1
            if (sx, sy) in px: out[(X, Y)] = wad.PAL[px[(sx, sy)]]
    return out
# Pose table per weapon: lists of (sprite, [flash sprites])
WEAPONS = [
    ('fist', [('PUNGA0', []), ('PUNGC0', [])]),
    ('chainsaw', [('SAWGC0', []), ('SAWGD0', []), ('SAWGA0', []), ('SAWGB0', [])]),
    ('pistol', [('PISGA0', []), ('PISGB0', ['PISFA0']), ('PISGB0', ['PISFA0']), ('PISGC0', [])]),
    ('shotgun', [('SHTGA0', []), ('SHTGA0', ['SHTFA0']), ('SHTGA0', ['SHTFB0']), ('SHTGB0', [])]),
    ('minigun', [('CHGGA0', []), ('CHGGB0', []), ('CHGGB0', ['CHGFB0']), ('CHGGA0', ['CHGFA0'])]),
    ('rocket', [('MISGA0', []), ('MISGB0', ['MISFA0']), ('MISGB0', ['MISFB0']), ('MISGB0', ['MISFC0']), ('MISGA0', ['MISFD0'])]),
]
def compose(spr, flashes):
    img = {}
    for f in flashes: img.update(place(f))      # flash under the gun (Doom draws flash psprite after? flash is a separate psprite drawn on top)
    img.update(place(spr))
    # Doom draws the flash psprite after the weapon (on top)
    for f in flashes: img.update(place(f))
    return img
def all_images():
    out = []
    for name, poses in WEAPONS:
        out.append([compose(s, f) for s, f in poses])
    return out
def choose_palette(images):
    hist = {}
    for w in images:
        for img in w:
            for rgb in img.values():
                c = sms_of(rgb); hist[c] = hist.get(c, 0) + 1
    fixed = list(FIXED.values())
    def d2(a, b): return sum((x - y) ** 2 for x, y in zip(a, b))
    # greedy: repeatedly add the colour that most reduces total error
    chosen = []
    cand = list(hist)
    for _ in range(len(FREE)):
        best = None
        for c in cand:
            if c in chosen or c in fixed: continue
            pal = fixed + chosen + [c]
            err = sum(n * min(d2(k, p) for p in pal) for k, n in hist.items())
            if best is None or err < best[0]: best = (err, c)
        chosen.append(best[1])
    pal = dict(FIXED)
    for i, c in zip(FREE, chosen): pal[i] = c
    return pal
def quantize(img, pal):
    out = {}
    cache = {}
    for xy, rgb in img.items():
        c = sms_of(rgb)
        if c not in cache:
            cache[c] = min(pal, key=lambda i: sum((a - b) ** 2 for a, b in zip(pal[i], c)) * 4 +
                           (0 if i <= 8 else 0))
        out[xy] = cache[c]
    return out

# fixed sprite palette (entries 9-15 picked for the Doom weapons; 14 = live border colour)
SPRPAL = dict(FIXED)
SPRPAL.update({9: (1, 1, 1), 10: (1, 0, 0), 11: (3, 2, 1), 12: (3, 0, 0), 13: (3, 2, 2), 15: (3, 1, 1)})
BANK_TILES = 68
def tile_bytes(px):
    out = []
    for y in range(8):
        for p in range(4):
            b = 0
            for x in range(8):
                if (px.get((x, y), 0) >> p) & 1: b |= 0x80 >> x
            out.append(b)
    return bytes(out)
def build_pose(img):
    """img: (X,Y)->rgb -> (tiles[], bg [(cell, t)], spr [(x, y, t)]) with <= BANK_TILES
    tiles and <= 8 edge sprites per 8-line row."""
    q = quantize(img, SPRPAL)
    cells = {}
    for (x, y), c in q.items(): cells.setdefault((x // 8, y // 8), {})[(x % 8, y % 8)] = c
    bg = {k: v for k, v in cells.items() if len(v) == 64}
    spr = {k: v for k, v in cells.items() if len(v) < 64}
    rows = {}
    for k in spr: rows.setdefault(k[1], []).append(k)
    for cy, ks in rows.items():
        if len(ks) > 8:
            ks.sort(key=lambda k: -len(spr[k]))
            for k in ks[:len(ks) - 8]: bg[k] = spr.pop(k)
    # unique tiles; merge the most alike until they fit the bank
    keys = list(bg) + list(spr)
    def vec(k): src = bg.get(k) or spr.get(k); return tuple(src.get((x, y), 0) for y in range(8) for x in range(8))
    vecs = {k: vec(k) for k in keys}
    uniq = sorted(set(vecs.values()))
    rep = {u: u for u in uniq}
    def dist(a, b):
        return sum(1 for p, q2 in zip(a, b) if p != q2) * 4 + sum(1 for p, q2 in zip(a, b) if (p == 0) != (q2 == 0)) * 16
    live = list(uniq)
    while len(live) > BANK_TILES:
        best = None
        for i in range(len(live)):
            for j in range(i + 1, len(live)):
                d = dist(live[i], live[j])
                if best is None or d < best[0]: best = (d, i, j)
        _, i, j = best
        a, b = live[i], live[j]
        for u in rep:
            if rep[u] == b: rep[u] = a
        live.pop(j)
    index = {u: n for n, u in enumerate(live)}
    tiles = [None] * len(live)
    for u in live: tiles[index[u]] = tile_bytes({(n % 8, n // 8): v for n, v in enumerate(u)})
    bgl = [(cy * 32 + cx, index[rep[vecs[(cx, cy)]]]) for (cx, cy) in sorted(bg)]
    spl = [(cx * 8, cy * 8, index[rep[vecs[(cx, cy)]]]) for (cx, cy) in sorted(spr)]
    return tiles, bgl, spl
