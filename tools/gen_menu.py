"""Front-end assets: title splash, menu logo, skull cursor, menu font, text pages."""
import sys, pickle
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from common import *
import numpy as np
from PIL import Image
VICE = [(0,0,0),(255,255,255),(104,55,43),(112,164,178),(111,61,134),(88,141,67),(53,40,121),(184,199,111),
        (111,79,37),(67,57,0),(154,103,89),(68,68,68),(108,108,108),(154,210,132),(108,94,181),(149,149,149)]
def c64idx(rgb):
    return min(range(16), key=lambda i: sum((a - b) ** 2 for a, b in zip(VICE[i], rgb)))
G = pickle.load(open(GEN + 'banks_game.pkl', 'rb'))
banks = G['banks']
BANK_SPLASH = 7
hdr = ['#ifndef MENU_DATA_H\n#define MENU_DATA_H\n']
cfile = ['/* front-end assets: compiled into the front-end code bank */\n#include "menu_data.h"\n']
buf = bytearray()
def add(name, data):
    global buf
    if name.startswith('SPLASH'):
        hdr.append('#define %s_ADDR 0x%04X\n#define %s_SIZE %d\n' % (name, 0x8000 + len(buf), name, len(data)))
        buf += data; return
    lname = name.lower()
    hdr.append('extern const unsigned char %s[%d];\n#define %s_SIZE %d\n' % (lname, len(data), name, len(data)))
    cfile.append(carr(lname, list(data)))
# ---- splash (single name table, <= 448 tiles) ----
im = Image.open(SRC + 'splash/DOOM_05.png').convert('RGB')
src = np.array([[c64idx(im.getpixel((x, y))) for x in range(320)] for y in range(200)])
scr = np.array([[src[(y * 200) // 192][((x * 320) // 256)] for x in range(256)] for y in range(192)])
cells = scr.reshape(24, 8, 32, 8).transpose(0, 2, 1, 3).reshape(768, 64)
rgb = np.array([[c for p in row for c in VICE[p]] for row in cells], dtype=float)
uniq, inv = np.unique(cells, axis=0, return_inverse=True)
inv = inv.reshape(-1)
K = 440
if len(uniq) > K:
    U = np.array([[c for p in row for c in VICE[p]] for row in uniq], dtype=float)
    cnt = np.bincount(inv, minlength=len(uniq)).astype(float)
    # greedy agglomerative merge on weighted distance until K remain
    alive = list(range(len(uniq)))
    rep = {i: i for i in alive}
    D = ((U[:, None, :] - U[None, :, :]) ** 2).sum(-1)
    np.fill_diagonal(D, np.inf)
    W = cnt.copy()
    active = np.ones(len(uniq), bool)
    while active.sum() > K:
        Dm = np.where(active[:, None] & active[None, :], D * np.minimum(W[:, None], W[None, :]), np.inf)
        i, j = np.unravel_index(np.argmin(Dm), Dm.shape)
        keep, drop = (i, j) if W[i] >= W[j] else (j, i)
        active[drop] = False; W[keep] += W[drop]
        for k, v in rep.items():
            if v == drop: rep[k] = keep
    reps = sorted(set(rep.values()))
    remap = {r: n for n, r in enumerate(reps)}
    tiles_u = [uniq[r] for r in reps]
    tmap = [remap[rep[inv[c]]] for c in range(768)]
else:
    tiles_u = list(uniq); tmap = list(inv)
print('splash tiles', len(tiles_u))
sbytes = b''.join(tile_from_px(t.reshape(8, 8)) for t in tiles_u)
smap = bytearray()
for t in tmap: smap += bytes([t & 255, t >> 8])
add('SPLASH_TILES', sbytes); add('SPLASH_MAP', bytes(smap))
hdr.append('#define SPLASH_NTILES %d\n' % len(tiles_u))
# ---- logo 128x64 (16x8 tiles) ----
lg = Image.open(SRC + 'itemgraphics/multicolour/m_doom_c64.png').convert('RGB')
lp = [[c64idx(lg.getpixel((x, y))) for x in range(128)] for y in range(64)]
lt = []
for ty in range(8):
    for tx in range(16):
        lt.append(tile_from_px([[lp[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)]))
add('LOGO_TILES', b''.join(lt))
# ---- skull cursor: 24x24 sprite, 2 frames (sprite palette = C64 colours) ----
sk = b''
for f in ('m_skull_c64', 'm_skull2_c64'):
    im = Image.open(SRC + 'itemgraphics/multicolour/%s.png' % f).convert('RGB')
    w, h = im.size
    px = [[0] * 24 for _ in range(24)]
    for y in range(h):
        for x in range(w):
            c = c64idx(im.getpixel((x, y)))
            px[y + 2][x + 2] = c if c else 0
    # black outline pixels inside the skull must stay opaque: use entry 15 remapped to black
    for ty in range(3):
        for tx in range(3):
            sk += tile_from_px([[px[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)])
add('SKULL_TILES', sk)
# ---- menu font (ASCII 32..127, 8 per row) ----
fi = Image.open(SRC + 'doommenufont.png').convert('L')
font = []
for i in range(96):
    cx, cy = (i % 8) * 8, (i // 8) * 8
    for y in range(8):
        b = 0
        for x in range(8):
            if fi.getpixel((cx + x, cy + y)) > 0: b |= 0x80 >> x
        font.append(b)
font2 = []
for i in range(96):
    rows = font[i * 8:i * 8 + 8]
    top = [rows[y // 2] for y in range(8)]; bot = [rows[4 + y // 2] for y in range(8)]
    font2 += top + bot
add('MFONT', bytes(font)); add('MFONT2', bytes(font2))
# ---- text pages (wrap 30 cols, <= 20 lines per SMS page) ----
def pages(fname, width=30, maxl=20):
    """C64 pages ('^' lines) -> paragraphs reflowed to 30 columns, split to fit."""
    out = []
    for chunk in open(SRC + fname).read().replace('\r', '').split('\n^\n'):
        lines = []
        paras = [p for p in chunk.split('\n\n')]
        for para in paras:
            words = ' '.join(l.strip().replace('^', '') for l in para.split('\n')).split()
            if not words: continue
            l = ''
            for w in words:
                if l and len(l) + 1 + len(w) > width: lines.append(l); l = w
                else: l = (l + ' ' + w) if l else w
            if l: lines.append(l)
            lines.append('')
        while lines and not lines[-1]: lines.pop()
        while lines:
            cut = min(maxl, len(lines))
            if cut < len(lines):
                while cut > maxl // 2 and lines[cut - 1]: cut -= 1   # prefer a paragraph break
                if cut <= maxl // 2: cut = maxl
            pg = lines[:cut]; lines = lines[cut:]
            while lines and not lines[0]: lines.pop(0)
            while pg and not pg[-1]: pg.pop()
            if pg: out.append(pg)
    return out
def text_blob(pgs):
    b = bytearray()
    for p in pgs:
        for l in p: b += l.encode('ascii', 'replace') + b'\n'
        b += b'\x00'
    return bytes(b)
CONTROL = """Controls

move.......d-pad up/down
turn.......d-pad left/right
strafe.....2 with left/right
use........tap 2
fire.......1
weapon.....2 with up/down
map........pause
quit.......hold 1 and 2 on map

Cheats (on the map):
left right left right, then
1 1 2  god mode
2 2 1  keys and full ammo
1 2 2  change level"""
open(GEN + 'control_sms.txt', 'w').write(CONTROL)
for name, f in (('READTHIS', 'readthis.txt'), ('CREDITS', 'credits.txt'), ('ORDER', 'order.txt'), ('ENDING', 'ending.txt')):
    pg = pages(f)
    if name == 'CREDITS': pg.append(['Sega Master System version', '', 'converted from the C64 game:', 'levels, graphics, sounds and', 'music taken from the original', 'SquareDoom sources.'])
    add('TXT_' + name, text_blob(pg)); hdr.append('#define TXT_%s_PAGES %d\n' % (name, len(pg)))
pg = []
cur = []
for raw in CONTROL.split('\n'):
    if raw.strip() == '^': continue
    cur.append(raw.replace('^', ''))
add('TXT_CONTROL', text_blob([cur])); hdr.append('#define TXT_CONTROL_PAGES 1\n')
banks[BANK_SPLASH] = bytes(buf)
hdr.append('#endif\n')
open(GEN + 'menu_data.h', 'w').write(''.join(hdr))
open(GEN + 'menu_assets.c', 'w').write(''.join(cfile))
G['banks'] = banks
pickle.dump(G, open(GEN + 'banks_all.pkl', 'wb'))
print('banks', {k: len(v) for k, v in banks.items()})
