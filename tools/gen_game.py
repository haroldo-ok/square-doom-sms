"""Generate game tables, tiles, textures, level banks (SquareDoom SMS port)."""
import sys, math, re
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from common import *
import acme

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
pass  # row sets now live in the per-level cast banks
RSET = (r1, r2, offs, lens)
set_list_saved = set_list
print('row sets', len(set_list), 'bytes', len(data))
arr('qsq', [(i * i) // 4 for i in range(512)], 'unsigned int')
arr('recip_tab', [65535] + [min(65535, round(65536 / w)) for w in range(1, 128)], 'unsigned int')
# wall distance lighting: dither steps added for a wall P (8.8 tiles) away, index P>>4.
# Roughly linear like Doom's diminishing light, levelling off before full black.
LIGHTFALL = [min(13, round((i / 16) * 1.25)) for i in range(256)]

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
# In-game text: Doom's STCFN message font, red shades from the sprite palette
import wad, doomweap as DWF
def red_idx(rgb):
    lv = max(rgb)
    return 12 if lv >= 200 else 15 if (lv >= 160 and rgb[1] > 40) else 3 if lv >= 110 else 10
def stcfn_tile(ch):
    w, h, lo, to, px = wad.patch('STCFN%03d' % ord(ch))
    img = {}
    ox = max(0, (7 - w) // 2)          # centre the proportional glyph in its cell
    for (x, y), v in px.items():
        X, Y = x + ox, y - to
        if 0 <= X < 8 and 0 <= Y < 8: img[(X, Y)] = red_idx(wad.PAL[v])
    return DWF.tile_bytes(img)
def sprite_tile(name, ox=0, oy=0):
    w, h, lo, to, px = wad.patch(name)
    img = {}
    for (x, y), v in px.items():
        c = DWF.sms_of(wad.PAL[v])
        k = min(DWF.SPRPAL, key=lambda i: sum((a - b) ** 2 for a, b in zip(DWF.SPRPAL[i], c)))
        img[(x + ox, y + oy)] = k
    return DWF.tile_bytes(img)
def icon(g, col):
    return DWF.tile_bytes({(x, y): col for y in range(8) for x in range(8) if glyph(g)[y] & (0x80 >> x)})
for i in range(26): tiles.append(stcfn_tile(chr(65 + i)))                  # A-Z
for i in range(10): tiles.append(stcfn_tile(chr(48 + i)))                  # 0-9
PUNCT = '.,!\':-%?'
for ch in PUNCT: tiles.append(stcfn_tile(ch))
for g in (38, 31, 34, 60): tiles.append(icon(g, 4))                        # ammo icons (yellow)
tiles.append(icon(47, 5))                                                  # health (white)
tiles.append(icon(30, 2))                                                  # armor (grey)
for k in (2, 1, 0): tiles.append(sprite_tile('STKEYS%d' % k, 0, 1))         # red, yellow, blue keycards
for i in range(4): tiles.append(icon(17 + i, 5))                           # map arrows (white)
while len(tiles) < 256: tiles.append(bytes(32))
H('#define T_FONT_AZ %d' % FONT0)
H('#define T_FONT_09 %d' % (FONT0 + 26))
H('#define T_FONT_09Y T_FONT_09')
H('#define T_FONT_09R T_FONT_09')
H('#define T_FONT_PUNCT %d' % (FONT0 + 36))
H('#define FONT_PUNCT_CHARS "%s"' % PUNCT.replace('\\', '\\\\').replace('"', '\\"'))
H('#define T_AMMOICON %d' % (FONT0 + 44))
H('#define T_HEALTHICON %d' % (FONT0 + 48))
H('#define T_ARMORICON %d' % (FONT0 + 49))
H('#define T_KEY_RED %d' % (FONT0 + 50))
H('#define T_KEY_YELLOW %d' % (FONT0 + 51))
H('#define T_KEY_BLUE %d' % (FONT0 + 52))
H('#define T_ARROW %d' % (FONT0 + 53))
ct = []
for ch in range(32, 96):
    c = chr(ch)
    if 'A' <= c <= 'Z': ct.append(FONT0 + ord(c) - 65)
    elif '0' <= c <= '9': ct.append(FONT0 + 26 + ord(c) - 48)
    elif c in PUNCT: ct.append(FONT0 + 36 + PUNCT.index(c))
    else: ct.append(0)
arr('char_tiles', ct)
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
floor_entry_tab = floorb
# flat entries by colour and sector light: flat_tab[colour * 32 + bright] (pattern 15 = black)
ft = []
for c in range(16):
    for b in range(16):
        pat = 15 - b
        ft.append(0 if (pat >= 15 or c == 0) else floor_entry_tab[c] + pat)
    ft.append(floor_entry_tab[c] if c else 0)      # bright 16 = full bright
    ft += [0] * 15
arr('flat_tab', ft, 'unsigned int')
arr('solid_entry', solid, 'unsigned int')

# ------------------------------------------------------------------ banks
BANK_FRONT, BANK_LOGIC_T, BANK_WPN0, BANK_SPLASH, BANK_RENDER0, BANK_LOGIC0, BANK_INIT0 = 2, 3, 3, 7, 8, 17, 17
for k, v in (('BANK_FRONT', BANK_FRONT), ('BANK_WPN0', BANK_WPN0), ('BANK_SPLASH', BANK_SPLASH),
             ('BANK_RENDER0', BANK_RENDER0), ('BANK_LOGIC0', BANK_LOGIC0), ('BANK_INIT0', BANK_INIT0)):
    H('#define %s %d' % (k, v))
H('#define LOGIC_CODE_ADDR 0x8400')
# weapon banks: game tileset + weapon tiles + pose data
wb = {b: bytearray() for b in [3, 4, 5, 6] + list(range(32, 40))}
def wadd(name, data):
    b = next(b for b in sorted(wb) if len(wb[b]) + len(data) <= 16384)
    off = len(wb[b]); wb[b] += data
    H('#define %s_BANK %d' % (name, b)); H('#define %s_ADDR 0x%04X' % (name, 0x8000 + off))
    return b, 0x8000 + off
wadd('GAMETILES', b''.join(tiles))
import doomweap as DW
# Doom weapon poses. Each pose record: ntiles, nbg, nspr, then nbg x (cell lo, cell hi, tile),
# nspr x (x, y, tile), then the tile data. Tiles are bank-local 0..67 (runtime maps
# them to one of two VRAM tile banks so the shown frame never sees a half-loaded pose).
wpose_bank = []; wpose_addr = []; WPOSE_N = []
for wi, (name, poses) in enumerate(DW.WEAPONS):
    recs = []
    for spr_name, flashes in poses:
        tiles_p, bgl, spl = DW.build_pose(DW.compose(spr_name, flashes))
        rec = [len(tiles_p), len(bgl), len(spl)]
        def vram(bank, t):
            if not bank: return 256 + t
            return 324 + t if t < 60 else 440 + (t - 60)
        for bank in (0, 1):      # final fb writes + sprite table bytes for each VRAM tile bank
            for cell, t in bgl:
                off = cell * 2; e = vram(bank, t) | 0x0800
                rec += [off & 255, off >> 8, e & 255, e >> 8]
            rec += [(y - 1) & 255 for x, y, t in spl]
            for x, y, t in spl: rec += [x & 255, (vram(bank, t) - 256) & 255]
        recs.append(bytes(rec) + b''.join(tiles_p))
    blob_total = sum(len(r) for r in recs)
    # keep a weapon's poses in one bank
    bnk = next(b for b in sorted(wb) if len(wb[b]) + blob_total <= 16384)
    for r in recs:
        wpose_bank.append(bnk); wpose_addr.append(0x8000 + len(wb[bnk])); wb[bnk] += r
    WPOSE_N.append(len(poses))
    print('weapon', name, 'bytes', blob_total)
arr('wpose_bank', wpose_bank)
arr('wpose_addr', wpose_addr, 'unsigned int')
pf = [0]
for n in WPOSE_N: pf.append(pf[-1] + n)
arr('wpose_first', pf)
H('#define WPOSE_BANK_TILES %d' % DW.BANK_TILES)
arr('game_sprpal_doom', [r | (g << 2) | (b << 4) for r, g, b in [DW.SPRPAL.get(i, (0, 0, 0)) for i in range(16)]])
banks.update({b: v for b, v in wb.items() if len(v)})
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
# per-crossing info: row set offset (into rset_data), row set length, wall falloff
r1, r2, offs, lens = RSET
ri = bytearray()
for i in range(256):
    P = i * 8 + 4; s_ = r1[i]
    ri += bytes([offs[s_] & 255, offs[s_] >> 8, lens[s_], LIGHTFALL[min(255, P >> 4)]])
for j in range(256):
    P = j * 64 + 32; s_ = r2[j]
    ri += bytes([offs[s_] & 255, offs[s_] >> 8, lens[s_], LIGHTFALL[min(255, P >> 4)]])
arr('enemy_mip_addr', emip, 'unsigned int'); arr('item_mip_addr', imip, 'unsigned int')
arr('enemy_mip_w', [16, 8, 4, 2, 1]); arr('enemy_mip_h', [32, 16, 8, 4, 2])
arr('item_mip_w', [8, 4, 2, 1])
print('render bank bytes', len(tex))
assert len(tex) <= 16384
levels = [open(SRC + 'levels/e1m%d.bin' % n, 'rb').read() for n in range(1, 10)]
for n in range(9):
    banks[BANK_RENDER0 + n] = bytes(levels[n][:1024]) + bytes(tex[1024:])
# ------------------------------------------------------------------ ray banks
# per player angle: the 32 columns' perpendicular DDA steps (fish-corrected), as
# rebuild_col_rays used to compute them: max(1, (fish * dd) >> 8)
BANK_RAYS0 = 49
H('#define BANK_RAYS0 %d' % BANK_RAYS0)
rays = bytearray()
for pa in range(256):
    xs = []; ys = []
    for c in range(COLS):
        a = (pa + colang[c]) & 255
        xs.append(max(1, (fish[c] * ddx[a]) >> 8)); ys.append(max(1, (fish[c] * ddy[a]) >> 8))
    sx = bytes([1 if sins[(pa + colang[c]) & 255] < 128 and sins[(pa + colang[c]) & 255] > 0 else 255 for c in range(COLS)])
    cy = [((sins + sins[:64])[((pa + colang[c]) & 255) + 64]) for c in range(COLS)]
    sy = bytes([255 if (v < 128 and v > 0) else 1 for v in cy])
    rays += u16s(xs) + u16s(ys) + sx + sy + bytes(64)        # 256 bytes per angle
for i in range(4): banks[BANK_RAYS0 + i] = bytes(rays[i * 16384:(i + 1) * 16384])
# ------------------------------------------------------------------ cast banks (one per level)
# [map 1K][rinfo 512 x 4][row table: per row set, 75 clamped rows for dh = -40..34]
# [sky][switch texture][ddx/ddy]: everything the column caster reads, in one bank.
BANK_CAST0 = 40
H('#define BANK_CAST0 %d' % BANK_CAST0)
H('#define CAST_BANK (BANK_CAST0 + level_num - 1)')
r1, r2, offs, lens = RSET
cb = bytearray(1024)
RI_OFF = len(cb); cb += bytes(512 * 4)
T_OFF = len(cb)
set_base = []
DH0, DHN = 40, 75
for v in set_list:
    set_base.append(0x8000 + len(cb) + DH0)
    for k in range(DHN):
        dh = k - DH0
        a = abs(dh)
        n = 0 if a == 0 else (v[a] if a < len(v) else (13 if len(v) < 32 else v[31]))
        row = 12 - n if dh > 0 else 12 + n
        cb.append(max(0, min(24, row)))
ri = bytearray()
for i in range(256):
    P = i * 8 + 4; b = set_base[r1[i]]
    ri += bytes([b & 255, b >> 8, min(255, P >> 4), LIGHTFALL[min(255, P >> 4)]])
for j in range(256):
    P = j * 64 + 32; b = set_base[r2[j]]
    ri += bytes([b & 255, b >> 8, min(255, P >> 4), LIGHTFALL[min(255, P >> 4)]])
cb[RI_OFF:RI_OFF + len(ri)] = ri
def cadd(name, data):
    global cb
    H('#define %s 0x%04X' % (name, 0x8000 + len(cb))); cb += data
cadd('CAST_SKY', bytes(sky['sky_cols']))
cadd('CAST_SWITCH', bytes(sw['wall_switch_tex']))
cadd('CAST_DDX', u16s(ddx)); cadd('CAST_DDY', u16s(ddy))
print('cast bank bytes', len(cb))
assert len(cb) <= 16384
for n in range(9):
    banks[BANK_CAST0 + n] = bytes(levels[n][:1024]) + bytes(cb[1024:])
open(GEN + 'rinfo.inc', 'w').write('        RINFO_ADDR = 0x%04X\n' % (0x8000 + RI_OFF))
# level init data (everything after the map) lives at the end of each logic bank
INIT_SZ = 3473 - 1024
INIT_ADDR = 0xC000 - INIT_SZ
H('#define LEVEL_INIT_SIZE %d' % INIT_SZ)
H('#define LEVEL_INIT_ADDR 0x%04X' % INIT_ADDR)
import pickle
pickle.dump({'banks': banks, 'maps': [l[:1024] for l in levels], 'inits': [l[1024:] for l in levels], 'init_addr': INIT_ADDR}, open(GEN + 'banks_game.pkl', 'wb'))
H('#endif')
open(GEN + 'fixed.c', 'w').write(''.join(fixed_c))
open(GEN + 'fixed.h', 'w').write(''.join(fixed_h))

