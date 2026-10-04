"""Generate game tables, tiles, textures, level banks (SquareDoom SMS port)."""
import sys, math, re
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from common import *
import acme, weapons as W

fixed_c = ['#include "fixed.h"\n']
fixed_h = ['#ifndef FIXED_H\n#define FIXED_H\n']
banks = {}          # bank number -> bytearray
def H(s): fixed_h.append(s + '\n')
def C(s): fixed_c.append(s)
def arr(name, data, typ='unsigned char'):
    C(carr(name, data, typ)); H('extern const %s %s[%d];' % (typ, name, len(data)))

# ------------------------------------------------------------------ tables
COLS = 32; HALF_FOV = 24
sins = [round(64 * math.sin(i * 2 * math.pi / 256)) & 255 for i in range(256)]
arr('sintab', sins + sins[:64], 'signed char')
H('#define costab (sintab + 64)')
colang = [round(((c + 0.5) / COLS - 0.5) * 2 * HALF_FOV) & 255 for c in range(COLS)]
arr('angtab', colang)
fish = [max(1, min(255, round(255 * math.cos(((a << 24) >> 24) * 2 * math.pi / 256)))) for a in colang]
arr('fishtab', fish)
# per-angle delta distances (8.8 tiles) for x- and y- crossings
ddx = []; ddy = []
for a in range(256):
    s = abs(math.sin(a * 2 * math.pi / 256)); c = abs(math.cos(a * 2 * math.pi / 256))
    ddx.append(min(0x4000, round(256 / max(s, 1e-9)))); ddy.append(min(0x4000, round(256 / max(c, 1e-9))))
# projection reciprocal: T = 2^20 / P (P = perp 8.8 tiles); two tiers
t1 = [min(65535, round((1 << 20) / max(16, i * 8 + 4))) for i in range(256)]       # P < 2048 : P>>3
t2 = [min(65535, round((1 << 20) / max(2048, i * 64 + 32))) for i in range(256)]   # P >= 2048: P>>6
# item reciprocal rz[z] = 8192/z (z world units 1..255)

# ---- projection: R = rows per height unit (x16 near / x32 far) + quarter squares
# ---- projection row sets: P -> set id; set = rows n(dh) = min(13, ceil(dh*1024/P))
import math as _m
sets = {}; set_list = []
def set_for(Rnum, Rden):       # rows per unit = Rnum / Rden
    vec = []
    for dh in range(32):
        n = 0 if dh == 0 else min(13, _m.ceil(dh * Rnum / Rden - 1e-9))
        vec.append(n)
    L = 32
    while L > 1 and vec[L - 1] == 13: L -= 1
    key = tuple(vec[:L])
    if key not in sets: sets[key] = len(set_list); set_list.append(key)
    return sets[key]
r1 = [set_for(min(255, round(16384 / (i * 8 + 4))), 16) for i in range(256)]     # P < 2048, idx P>>3
r2 = [set_for(1024, i * 64 + 32) for i in range(256)]             # P >= 2048, idx P>>6
assert len(set_list) <= 256, len(set_list)
data = []; offs = []; lens = []
for v in set_list: offs.append(len(data)); lens.append(len(v)); data += list(v)
arr('rtab1', r1); arr('rtab2', r2)
arr('rset_off', offs, 'unsigned int'); arr('rset_len', lens); arr('rset_data', data)
print('row sets', len(set_list), 'bytes', len(data))
arr('qsq', [(i * i) // 4 for i in range(512)], 'unsigned int')
COL_ANGLE = (2 * HALF_FOV / COLS) * (2 * math.pi / 256)
spr = []
for i in range(256):
    v = (i << 24) >> 24
    if v >= 127: spr.append(100)
    elif v <= -127: spr.append(156)
    else: spr.append(round(COLS / 2 + math.atan(v / 32) / COL_ANGLE) & 255)
arr('sprcol', spr)
ws = []
for c in range(-16, 48):
    th = (c - COLS / 2) * COL_ANGLE; t = math.tan(th)
    ws.append(max(1, min(255, round(256 * (1 / (COL_ANGLE * (1 + t * t))) / 32))))
arr('sprwscale', ws)

# ------------------------------------------------------------------ palettes
arr('game_bgpal', [smsrgb(c) for c in BGPAL_C64])
arr('game_sprpal', [smsrgb(c) for c in SPRPAL_C64])
arr('c64_sms', [smsrgb(c) for c in range(16)])

# ------------------------------------------------------------------ view + font tiles
cs, _ = acme.parse('charset.asm'); charset = cs['charset_image']
glyph = lambda i: charset[i * 8:i * 8 + 8]
dith, _ = acme.parse('ditherchars.asm')
WALLG = [dith['dither_wall_glyphs'][i*8:i*8+8] for i in range(16)]
FLOORG = [dith['dither_floor_glyphs'][i*8:i*8+8] for i in range(16)]
tiles = [bytes(32)]
for k in range(1, 7):
    for p in range(15): tiles.append(tile_from_1bpp(WALLG[p], k))
    for p in range(15): tiles.append(tile_from_1bpp(FLOORG[p], k))
    tiles.append(tile_from_1bpp([255] * 8, k))
tiles.append(tile_from_1bpp([255] * 8, 7)); tiles.append(tile_from_1bpp([255] * 8, 8))
assert len(tiles) == 189
FONT0 = len(tiles)
for i in range(26): tiles.append(tile_from_1bpp(glyph(64 + i), 4))          # A-Z
for i in range(10): tiles.append(tile_from_1bpp(glyph(48 + i), 4))          # 0-9 yellow/blue
PUNCT = '.,!\':-%?'
for ch in PUNCT: tiles.append(tile_from_1bpp(glyph(ord(ch)), 4))
for i in range(10): tiles.append(tile_from_1bpp(glyph(48 + i), 3))          # 0-9 red/green
for g in (38, 31, 34, 60): tiles.append(tile_from_1bpp(glyph(g), 7))        # ammo icons cyan
tiles.append(tile_from_1bpp(glyph(47), 5))                                  # health white
tiles.append(tile_from_1bpp(glyph(30), 3)); tiles.append(tile_from_1bpp(glyph(30), 4))
tiles.append(tile_from_1bpp(glyph(59), 3)); tiles.append(tile_from_1bpp(glyph(59), 4))
for i in range(4): tiles.append(tile_from_1bpp(glyph(17 + i), 5))           # map arrows
assert len(tiles) == 256, len(tiles)
H('#define T_FONT_AZ %d' % FONT0)
H('#define T_FONT_09Y %d' % (FONT0 + 26))
H('#define T_FONT_PUNCT %d' % (FONT0 + 36))
H('#define FONT_PUNCT_CHARS "%s"' % PUNCT.replace('\\', '\\\\').replace('"', '\\"'))
H('#define T_FONT_09R %d' % (FONT0 + 44))
H('#define T_AMMOICON %d' % (FONT0 + 54))
H('#define T_HEALTHICON %d' % (FONT0 + 58))
H('#define T_ARMORICON_G %d' % (FONT0 + 59))
H('#define T_ARMORICON_B %d' % (FONT0 + 60))
H('#define T_KEY_K3 %d' % (FONT0 + 61))
H('#define T_KEY_K4 %d' % (FONT0 + 62))
H('#define T_ARROW %d' % (FONT0 + 63))
# colour -> fb entry tables
def entry(k, s, p, kind):
    if k > 6 and kind != 'solid':  # fall back to nearest dithered colour
        k, s = {7: (3, 1), 8: (3, 0)}[k] if k == 7 else (3, 0)
    if kind == 'solid': t = {7: 187, 8: 188}.get(k, 1 + (k - 1) * 31 + 30)
    else: t = 1 + (k - 1) * 31 + (0 if kind == 'wall' else 15)
    return t | (0x800 if s else 0)
wallb = []; floorb = []; solid = []
for c in range(16):
    k, s = PAIR[c]
    if c == 0: wallb.append(0); floorb.append(0); solid.append(0); continue
    if c == 13: k, s = 3, 0      # light green -> green dither
    if c == 10: k, s = 3, 1      # light red -> red dither
    if c == 3: k, s = 2, 1       # cyan dither (sky handled separately) -> grey
    wallb.append(entry(k, s, 0, 'wall')); floorb.append(entry(k, s, 0, 'floor'))
    k, s = PAIR[c]; solid.append(entry(k, s, 0, 'solid'))
arr('wall_entry', wallb, 'unsigned int'); arr('floor_entry', floorb, 'unsigned int')
arr('solid_entry', solid, 'unsigned int')

# ------------------------------------------------------------------ banks
BANK_FRONT, BANK_LOGIC_T, BANK_WPN0, BANK_SPLASH, BANK_RENDER0, BANK_LOGIC0, BANK_INIT0 = 2, 3, 4, 7, 8, 17, 26
for k, v in (('BANK_FRONT', BANK_FRONT), ('BANK_WPN0', BANK_WPN0), ('BANK_SPLASH', BANK_SPLASH),
             ('BANK_RENDER0', BANK_RENDER0), ('BANK_LOGIC0', BANK_LOGIC0), ('BANK_INIT0', BANK_INIT0)):
    H('#define %s %d' % (k, v))
H('#define LOGIC_CODE_ADDR 0x8400')
# weapon banks: game tileset + weapon tiles + pose data
wb = {BANK_WPN0 + i: bytearray() for i in range(3)}
def wadd(name, data):
    b = next(b for b in sorted(wb) if len(wb[b]) + len(data) <= 16384)
    off = len(wb[b]); wb[b] += data
    H('#define %s_BANK %d' % (name, b)); H('#define %s_ADDR 0x%04X' % (name, 0x8000 + off))
    return b, 0x8000 + off
wadd('GAMETILES', b''.join(tiles))
wtile_tab = []; wpose_ptrs = []; posedata = []; WPOSE_N = []
for wi, (name, poses) in enumerate(W.WEAPONS):
    if name == 'pistol':
        tl = []; idx = {}
        def tid(b):
            if b not in idx: idx[b] = len(tl); tl.append(b)
            return idx[b]
        variants = {}
        for fl in (None, 'a', 'b'):
            img = W.compose_full(W.pistol(0, fl))
            xs = [x for x, y in img]; ys = [y for x, y in img]; x0, y0 = min(xs) & ~7, min(ys)
            cells = {}
            for (x, y), c in img.items():
                cells.setdefault(((x - x0) // 8, (y - y0) // 8), {})[((x - x0) % 8, (y - y0) % 8)] = W.SPAL[c]
            variants[fl] = [(x0 + cx * 8, y0 + cy * 8, tid(W.tile_bytes(px))) for (cx, cy), px in sorted(cells.items())]
        out = []
        for fl, off in ((None, 0), ('a', -4), ('b', -4), (None, -6)):
            out.append(([], [(x, y + off, t) for x, y, t in variants[fl] if -8 < y + off < 192]))
        tiles_w = tl
    else:
        tiles_w, out = W.build_weapon(poses)
    assert len(tiles_w) <= 136, (name, len(tiles_w))
    b, a = wadd('WTILES%d' % wi, b''.join(tiles_w))
    wtile_tab += [b, a & 255, a >> 8, len(tiles_w)]
    WPOSE_N.append(len(out))
    for bgl, spl in out:
        assert len(spl) <= 64
        rec = [len(bgl), len(spl)]
        for cx, cy, t in bgl:
            cell = cy * 32 + cx
            rec += [cell & 255, cell >> 8, t]
        for x, y, t in spl:
            rec += [x & 255, y & 255, t if t < 128 else 184 + (t - 128)]
        wpose_ptrs.append(len(posedata)); posedata += rec
b, a = wadd('WPOSE', bytes(posedata))
arr('wtile_tab', wtile_tab)
arr('wpose_off', wpose_ptrs, 'unsigned int')
pf = [0]
for n in WPOSE_N: pf.append(pf[-1] + n)
arr('wpose_first', pf)
banks.update(wb)
print('weapon banks', [len(v) for v in wb.values()])

# ------------------------------------------------------------------ render banks: [map 1K][textures][tables]
es, eo = acme.parse('enemy_sprites.asm')
ib, io_ = acme.parse('item_bitmaps.asm')
line = [l for l in open(SRC + 'item_bitmaps.asm') if l.strip().startswith('!byte <item_spr')][0]
item_labels = [x.strip().lstrip('<') for x in line.strip()[6:].split(',')]
tex = bytearray(1024)          # map placeholder at 0x8000
emip = []; imip = []; labels_done = {}
frames = ['walk', 'atk', 'pain', 'imp_walk', 'imp_atk', 'imp_pain', 'demon_walk', 'demon_atk', 'demon_pain', 'baron_walk', 'baron_atk']
for fr in frames:
    for m in range(5):
        d = es['enemy_spr_%s_m%d' % (fr, m)]
        emip.append(0x8000 + len(tex)); tex += bytes(0xFF if v == 0 else v for v in d)
for lab in item_labels:
    if lab not in labels_done:
        labels_done[lab] = 0x8000 + len(tex); tex += bytes(ib[lab])
    imip.append(labels_done[lab])
def radd(name, data):
    global tex
    H('#define %s 0x%04X' % (name, 0x8000 + len(tex))); tex += data
sky, _ = acme.parse('sky.asm'); radd('SKY_ADDR', bytes(sky['sky_cols']))
sw, _ = acme.parse('wall_switch.asm'); radd('SWITCH_ADDR', bytes(sw['wall_switch_tex']))
def u16s(a): return b''.join(bytes([v & 255, v >> 8]) for v in a)
radd('DDX_ADDR', u16s(ddx)); radd('DDY_ADDR', u16s(ddy))
radd('RZ_ADDR', u16s([0] + [round(8192 / z) for z in range(1, 256)]))
arr('enemy_mip_addr', emip, 'unsigned int'); arr('item_mip_addr', imip, 'unsigned int')
arr('enemy_mip_w', [16, 8, 4, 2, 1]); arr('enemy_mip_h', [32, 16, 8, 4, 2])
arr('item_mip_w', [8, 4, 2, 1])
print('render bank bytes', len(tex))
assert len(tex) <= 16384
levels = [open(SRC + 'levels/e1m%d.bin' % n, 'rb').read() for n in range(1, 10)]
for n in range(9):
    banks[BANK_RENDER0 + n] = bytes(levels[n][:1024]) + bytes(tex[1024:])
# level init data (everything after the map), 6 per bank
INIT_SZ = 3473 - 1024
for n in range(9):
    b = BANK_INIT0 + n // 6
    banks.setdefault(b, bytearray())
    banks[b] = bytes(banks[b]) + levels[n][1024:]
H('#define LEVEL_INIT_SIZE %d' % INIT_SZ)
H('#define LEVELS_PER_INIT_BANK 6')
import pickle
pickle.dump({'banks': banks, 'maps': [l[:1024] for l in levels]}, open(GEN + 'banks_game.pkl', 'wb'))
H('#endif')
open(GEN + 'fixed.c', 'w').write(''.join(fixed_c))
open(GEN + 'fixed.h', 'w').write(''.join(fixed_h))

