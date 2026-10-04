"""Compose C64 multi-layer weapon sprites into SMS poses (1:1 pixels; the SMS
view is the central 32x24 cells of the C64 40x25 view), then split each pose
into opaque-interior BG tiles + edge sprites."""
import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import acme
blobs = {}
for f in ['pistol_sprites.asm', 'muzzle_flash.asm', 'shotgun_weapon.asm', 'shotgun_cock_sprites.asm',
          'minigun_weapon.asm', 'rocket_weapon.asm', 'chainsaw_weapon.asm', 'fist_righthand.asm', 'fist_punch.asm']:
    d, o = acme.parse(f); blobs.update(d)
HL = 'HL'
def L(name, col, x, y): return (name, col, x, y)
def pistol(off, flash):
    ls = [L('pistol_hi', HL, 160, 192+off), L('pistol_mid', 11, 160, 192+off), L('pistol_dark', 0, 160, 192+off),
          L('pistol_orange', 8, 160, 214+off), L('pistol_brown', 9, 160, 214+off), L('pistol_hand_dark', 0, 160, 214+off)]
    if flash: ls += [L('muzzle_%s_white' % flash, 1, 166, 168+off), L('muzzle_%s_red' % flash, 2, 166, 168+off)]
    return ls
def shotgun(flash):
    ls = [L('shotgun_highlight', HL, 160, 208), L('shotgun_barrel', 0, 160, 186), L('shotgun_bodyleft', 0, 140, 208),
          L('shotgun_bodyright', 0, 188, 208), L('shotgun_brown', 9, 140, 208), L('shotgun_orange', 8, 140, 208)]
    if flash: ls += [L('muzzle_%s_white' % flash, 1, 160, 162), L('muzzle_%s_red' % flash, 2, 160, 162)]
    return ls
COCK = [L('cock_black_hi', 0, 100, 166), L('cock_black_lo', 0, 100, 208), L('cock_grey', 11, 100, 166),
        L('cock_highlight', 15, 100, 208), L('cock_brown', 9, 100, 186), L('cock_orange', 8, 100, 184)]
def minigun(frame, flash):
    ls = [L('minigun_%s_upperhighlight' % frame, HL, 160, 194), L('minigun_lowerhighlight', HL, 160, 208),
          L('minigun_%s_grey_left' % frame, 11, 136, 194), L('minigun_%s_grey_right' % frame, 11, 184, 194),
          L('minigun_black_left', 0, 136, 208), L('minigun_black_right', 0, 184, 208)]
    if flash: ls += [L('muzzle_%s_white' % flash, 1, 160, 172), L('muzzle_%s_red' % flash, 2, 160, 172)]
    return ls
def rocket(off, flash):
    ls = [L('rocket_hi', HL, 160, 180+off), L('rocket_detail', 11, 160, 208+off), L('rocket_dark_tl', 0, 136, 180+off),
          L('rocket_dark_tr', 0, 184, 180+off), L('rocket_dark_bl', 0, 136, 208+off), L('rocket_dark_br', 0, 184, 208+off)]
    if flash: ls += [L('rocket_flash_left', 10, 136, 162+off), L('rocket_flash_right', 10, 184, 162+off)]
    return ls
def chainsaw(off, blade):
    return [L('chainsaw_blade_hi2' if blade else 'chainsaw_blade_hi', 1, 174, 168+off), L('chainsaw_detail', 11, 182, 206+off),
            L('chainsaw_hand_orange', 8, 112, 208+off), L('chainsaw_hand_brown', 9, 112, 208+off),
            L('chainsaw_blade_dark', 0, 174, 168+off), L('chainsaw_body_left', 0, 112, 208+off),
            L('chainsaw_body_mid', 0, 160, 208+off), L('chainsaw_body_right', 0, 208, 208+off)]
def fist(punch):
    p = 'fist_punch' if punch else 'fist_right'
    xs = [160, 94, 126, 160, 94, 126, 160] if punch else [226, 160, 192, 226, 160, 192, 226]
    names = ['_hi', '_pink_l', '_pink_m', '_pink_r', '_dark_l', '_dark_m', '_dark_r']
    cols = [HL, 8, 8, 8, 9, 9, 9]
    return [L(p + n, c, x, 208) for n, c, x in zip(names, cols, xs)]
WEAPONS = [
    ('fist', [fist(0), fist(1)]),
    ('chainsaw', [chainsaw(0, 0), chainsaw(0, 1), chainsaw(8, 0), chainsaw(8, 1)]),
    ('pistol', [pistol(0, None), pistol(-4, 'a'), pistol(-4, 'b'), pistol(-6, None)]),
    ('shotgun', [shotgun(None), shotgun('a'), shotgun('b'), COCK]),
    ('minigun', [minigun('a', None), minigun('b', None), minigun('b', 'a'), minigun('a', 'b')]),
    ('rocket', [rocket(0, None), rocket(24, 1), rocket(16, 1), rocket(8, 1), rocket(0, 1)]),
]
def _paint(layers, clip):
    img = {}
    for name, col, sx, sy in reversed(layers):
        b = blobs[name]; x0 = sx - 24 - 32; y0 = sy - 50 - 8
        for r in range(21):
            for c in range(24):
                if b[r * 3 + c // 8] & (0x80 >> (c % 8)):
                    for dy in (0, 1):
                        for dx in (0, 1):
                            X = x0 + c * 2 + dx; Y = y0 + r * 2 + dy
                            if not clip or (0 <= X < 256 and 0 <= Y < 192): img[(X, Y)] = col
    return img
def compose(layers): return _paint(layers, True)
def compose_full(layers): return _paint(layers, False)
def decompose(img):
    cells = {}
    for (X, Y), c in img.items(): cells.setdefault((X // 8, Y // 8), {})[(X % 8, Y % 8)] = c
    bg, spr = {}, {}
    for k, px in cells.items(): (bg if len(px) == 64 else spr)[k] = px
    return bg, spr
SPAL = {HL: 12, 0: 8, 1: 5, 2: 3, 7: 4, 8: 9, 9: 1, 10: 7, 11: 10, 12: 2, 15: 11, 14: 6, 5: 13}
def tile_bytes(px):
    out = []
    for y in range(8):
        for p in range(4):
            b = 0
            for x in range(8):
                if (px.get((x, y), 0) >> p) & 1: b |= 0x80 >> x
            out.append(b)
    return bytes(out)
def build_weapon(poses, maxrow=8):
    tiles = []; index = {}
    def tid(b):
        if b not in index: index[b] = len(tiles); tiles.append(b)
        return index[b]
    out = []
    for p in poses:
        bg, spr = decompose(compose(p))
        rows = {}
        for k in spr: rows.setdefault(k[1], []).append(k)
        for cy, ks in rows.items():
            if len(ks) > maxrow:
                ks.sort(key=lambda k: -len(spr[k]))
                for k in ks[:len(ks) - maxrow]: bg[k] = spr.pop(k)
        bgl = [(cx, cy, tid(tile_bytes({xy: SPAL[c] for xy, c in px.items()}))) for (cx, cy), px in sorted(bg.items())]
        spl = [(cx * 8, cy * 8, tid(tile_bytes({xy: SPAL[c] for xy, c in px.items()}))) for (cx, cy), px in sorted(spr.items())]
        out.append((bgl, spl))
    return tiles, out
