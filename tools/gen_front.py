"""Front-end assets from Doom 1.9 (DOOM1.WAD): TITLEPIC title screen, Doom menus
(M_DOOM logo, M_* item graphics + harvested menu font, M_SKULL cursor, sound
thermometers), WI* intermission graphics, STCFN text font, text pages.
Image blobs: u16 ntiles, u8 w, u8 h, u8 npal, npal x 16 palette bytes, tiles, map
(w*h u16: tile index | 0x0800 for the second palette)."""
import sys, os, pickle
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import *
import numpy as np
from PIL import Image
import wad, doomfont, smsimage as S

G = pickle.load(open(GEN + 'banks_game.pkl', 'rb'))
banks = G['banks']
BANK_SPLASH = 7
FRONT_BANKS = [26, 27, 28, 29, 30]
hdr = ['#ifndef MENU_DATA_H\n#define MENU_DATA_H\n']
cfile = ['/* front-end data compiled into the front-end code bank */\n#include "menu_data.h"\n']
fixc = ['/* front-end image table (fixed area) */\n#include "menu_data.h"\n']
blobs = []                 # (name, bytes)

def patch_img(name):
    w, h, lo, to, px = wad.patch(name)
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    for (x, y), c in px.items(): im.putpixel((x, y), wad.PAL[c] + (255,))
    return im, lo, to
def text_img(text):
    g = doomfont.harvest()
    w, h, px = doomfont.render(text, g)
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    for (x, y), c in px.items(): im.putpixel((x, y), wad.PAL[c] + (255,))
    return im
def blob(res, w, h, npal):
    b = bytearray()
    b += bytes([len(res['tiles']) & 255, len(res['tiles']) >> 8, w, h, npal])
    for p in res['pals'][:npal]: b += bytes(S.sms_rgb_byte(k) for k in p)
    for t in res['tiles']: b += t
    for t, ps in res['map']:
        e = t | (0x800 if ps else 0)
        b += bytes([e & 255, e >> 8])
    return bytes(b)
def add_blob(name, data):
    blobs.append((name, data))

# ---------------------------------------------------------------- title screen
im, _, _ = patch_img('TITLEPIC')
title = np.array(im.convert('RGB').resize((256, 192), Image.LANCZOS))
res = S.convert(title, npal=2, max_tiles=440, dither=0.6)
S.preview(res, GEN + 'preview_title.png')
tb = blob(res, 32, 24, 2)
assert len(tb) <= 16384
banks[BANK_SPLASH] = tb
hdr.append('#define TITLE_BANK %d\n#define TITLE_ADDR 0x8000\n' % BANK_SPLASH)
print('title tiles', len(res['tiles']), 'bytes', len(tb))

# ---------------------------------------------------------------- menus (Doom m_menu.c layout)
SX, SY = 256 / 320, 192 / 200
def canvas(): return Image.new('RGBA', (320, 200), (0, 0, 0, 255))
def put(c, im, x, y): c.alpha_composite(im, (int(x), int(y)))
def stcfn_text(c, text, x, y, colour=None):
    for ch in text.upper():
        if ch == ' ': x += 4; continue
        n = 'STCFN%03d' % ord(ch)
        if n not in wad.LUMPS: x += 4; continue
        g, lo, to = patch_img(n)
        if colour:
            px = np.array(g); px[..., :3] = (px[..., :3] * np.array(colour) / 255).astype(np.uint8); g = Image.fromarray(px)
        put(c, g, x, y - to); x += g.width
MENUS = []
def menu(name, title_parts, items, x, y, hint, sound=False):
    c = canvas()
    for t in title_parts:
        img = patch_img(t[0])[0] if t[0].startswith('M_') else text_img(t[0])
        put(c, img, t[1], t[2])
    ys = []
    for i, it in enumerate(items):
        img = patch_img(it)[0] if it.startswith('M_') else text_img(it)
        yy = y + i * (32 if sound else 16)
        put(c, img, x, yy); ys.append(yy)
        if sound:     # thermometer under the item (M_DrawThermo, 16 steps)
            ty = yy + 16; xx = x
            put(c, patch_img('M_THERML')[0], xx - 2, ty); xx += 8
            for k in range(16): put(c, patch_img('M_THERMM')[0], xx, ty); xx += 8
            put(c, patch_img('M_THERMR')[0], xx, ty)
    stcfn_text(c, hint, 160 - len(hint) * 3, 190, (180, 180, 180))
    img = np.array(c.convert('RGB').resize((256, 192), Image.LANCZOS))
    res = S.convert(img, npal=1, max_tiles=380, dither=0.15)
    S.preview(res, GEN + 'preview_%s.png' % name)
    add_blob('MENU_' + name.upper(), blob(res, 32, 24, 1))
    # skull cursor positions in SMS pixels (Doom: x - 32, y - 5)
    MENUS.append((name, [(int((x - 32) * SX), int((yy - 5) * SY)) for yy in ys],
                  [(int((x + 8) * SX), int((yy + 16 + 1) * SY)) for yy in ys] if sound else []))
    print('menu', name, 'tiles', len(res['tiles']))
menu('main', [('M_DOOM', 94, 2)],
     ['M_NGAME', 'M_OPTION', 'Controls', 'M_RDTHIS', 'Credits', 'M_QUITG'], 97, 64,
     'd-pad move   1 select   2 back')
menu('episode', [('M_EPISOD', 54, 38)], ['M_EPI1', 'M_EPI2', 'M_EPI3'], 48, 63,
     'd-pad move   1 select   2 back')
menu('skill', [('M_NEWG', 96, 14), ('M_SKILL', 54, 38)], ['M_JKILL', 'M_HURT', 'M_ULTRA'], 48, 63,
     'd-pad move   1 select   2 back')
menu('sound', [('M_SVOL', 60, 38)], ['M_SFXVOL', 'M_MUSVOL'], 80, 64,
     'd-pad move and adjust   2 back', sound=True)
# sprites: skull frames (16x16) + thermometer knob, own sprite palette
spr = Image.new('RGBA', (40, 16), (0, 0, 0, 0))
for i, n in enumerate(('M_SKULL1', 'M_SKULL2')):
    s = patch_img(n)[0].resize((16, 15), Image.LANCZOS)
    spr.alpha_composite(s, (i * 16, 0))
knob = patch_img('M_THERMO')[0]
knob = knob.resize((max(1, round(knob.width * SX)), max(1, round(knob.height * SY))), Image.NEAREST)
spr.alpha_composite(knob, (32, 2))
a = np.array(spr)
rgbq = S.code(S.to_sms(a[..., :3]))
op = a[..., 3] > 128
hist = np.bincount(rgbq[op].ravel(), minlength=64).astype(float)
sp = S.pick_palette(hist, 15)
spal = [0] + sp
stiles = b''
for (cx, cy) in [(0, 0), (1, 0), (0, 1), (1, 1), (2, 0), (3, 0), (2, 1), (3, 1), (4, 0), (4, 1)]:
    rows = []
    for y in range(8):
        for pl in range(4):
            v = 0
            for x in range(8):
                X, Y = cx * 8 + x, cy * 8 + y
                if op[Y, X]:
                    k = 1 + int(S.D2[rgbq[Y, X]][sp].argmin())
                    if (k >> pl) & 1: v |= 0x80 >> x
            rows.append(v)
    stiles += bytes(rows)
add_blob('MENU_SPRITES', bytes([10, 0, 5, 2, 1]) + bytes(S.sms_rgb_byte(k) for k in spal) + stiles)
hdr.append('#define MENU_N 4\n')
for i, (name, sk, th) in enumerate(MENUS):
    hdr.append('#define MENU_ID_%s %d\n' % (name.upper(), i))
cfile.append(carr('menu_items', [len(m[1]) for m in MENUS]))
cfile.append(carr('menu_skull_x', [p[0] for m in MENUS for p in m[1]] ))
cfile.append(carr('menu_skull_y', [p[1] for m in MENUS for p in m[1]] ))
cfile.append(carr('menu_first', [sum(len(m[1]) for m in MENUS[:i]) for i in range(len(MENUS))]))
th = MENUS[3][2]
cfile.append(carr('therm_x', [p[0] for p in th])); cfile.append(carr('therm_y', [p[1] for p in th]))
for n in ('menu_items[4]', 'menu_skull_x[%d]' % sum(len(m[1]) for m in MENUS), 'menu_skull_y[%d]' % sum(len(m[1]) for m in MENUS),
          'menu_first[4]', 'therm_x[2]', 'therm_y[2]'):
    hdr.append('extern const unsigned char %s;\n' % n)

# ---------------------------------------------------------------- intermission (wi_stuff.c look)
WI_IMAGES = {}
def wi_chunk(name, img, cols=None):
    """RGBA image -> blob of whole tiles (scaled 0.8 x, 0.96 y)"""
    w = max(8, round(img.width * SX)); h = max(8, round(img.height * SY))
    img = img.resize((w, h), Image.LANCZOS)
    tw = (w + 7) // 8; th = (h + 7) // 8
    c = Image.new('RGBA', (tw * 8, th * 8), (0, 0, 0, 255))
    c.alpha_composite(img, ((tw * 8 - w) // 2, (th * 8 - h) // 2))
    WI_IMAGES[name] = (np.array(c.convert('RGB')), tw, th)
wi_chunk('WI_FINISHED', patch_img('WIF')[0])
wi_chunk('WI_ENTERING', patch_img('WIENTER')[0])
wi_chunk('WI_KILLS', patch_img('WIOSTK')[0])
wi_chunk('WI_ITEMS', patch_img('WIOSTI')[0])
wi_chunk('WI_SECRET', patch_img('WISCRT2')[0])
wi_chunk('WI_TIME', patch_img('WITIME')[0])
wi_chunk('WI_PAR', patch_img('WIPAR')[0])
wi_chunk('WI_SUCKS', patch_img('WISUCKS')[0])
for i in range(9): wi_chunk('WI_LV%d' % i, patch_img('WILV0%d' % i)[0])
# digits: 0-9, %, : as 8x16 cells (WINUM scaled to fit 8 wide)
dig = Image.new('RGBA', (12 * 8, 16), (0, 0, 0, 255))
for i, n in enumerate(['WINUM%d' % d for d in range(10)] + ['WIPCNT', 'WICOLON']):
    g = patch_img(n)[0]
    g = g.resize((min(8, max(1, round(g.width * 0.7))), 13), Image.LANCZOS)
    dig.alpha_composite(g, (i * 8 + (8 - g.width) // 2, 2))
WI_IMAGES['WI_DIGITS'] = (np.array(dig.convert('RGB')), 12, 2)
# one shared palette for all intermission pieces
allpx = np.concatenate([S.code(S.to_sms(v[0], 0.5)).ravel() for v in WI_IMAGES.values()])
wipal = S.pick_palette(np.bincount(allpx, minlength=64).astype(float), 16, (0,))
for name, (arr_, tw, th) in WI_IMAGES.items():
    q = S.code(S.to_sms(arr_, 0.5))
    idxs = S.D2[q][:, :, wipal].argmin(axis=2)
    tiles = []; tmap = []; seen = {}
    for ty in range(th):
        for tx in range(tw):
            cell = tuple(idxs[ty * 8:(ty + 1) * 8, tx * 8:(tx + 1) * 8].ravel())
            if name == 'WI_DIGITS':      # keep digit tiles in order (glyph i -> tiles 2i, 2i+1... laid out row-major)
                pass
            if cell not in seen or name == 'WI_DIGITS':
                seen[cell] = len(tiles)
                b = []
                for y in range(8):
                    for pl in range(4):
                        v = 0
                        for x in range(8):
                            if (cell[y * 8 + x] >> pl) & 1: v |= 0x80 >> x
                        b.append(v)
                tiles.append(bytes(b))
                tmap.append((len(tiles) - 1, 0))
            else: tmap.append((seen[cell], 0))
    add_blob(name, blob(dict(tiles=tiles, map=tmap, pals=[wipal]), tw, th, 1))
    hdr.append('#define %s_W %d\n#define %s_H %d\n' % (name, tw, name, th))
print('wi pieces', len(WI_IMAGES))

# ---------------------------------------------------------------- text pages font (STCFN, 4bpp, own colours)
TXTPAL = [0, 0b000001, 0b000010, 0b000011, 0b010111, 0b101010, 0b111111, 0, 0, 0, 0, 0, 0, 0, 0, 0]
def txt_idx(rgb):
    lv = max(rgb)
    return 3 if lv >= 200 else 2 if lv >= 120 else 1
font = b''
for c in range(32, 96):
    n = 'STCFN%03d' % c
    img = {}
    if n in wad.LUMPS:
        w, h, lo, to, px = wad.patch(n)
        ox = max(0, (7 - w) // 2)
        for (x, y), v in px.items():
            X, Y = x + ox, y - to
            if 0 <= X < 8 and 0 <= Y < 8: img[(X, Y)] = txt_idx(wad.PAL[v])
    b = []
    for y in range(8):
        for pl in range(4):
            v = 0
            for x in range(8):
                if (img.get((x, y), 0) >> pl) & 1: v |= 0x80 >> x
            b.append(v)
    font += bytes(b)
cfile.append(carr('txt_font', list(font))); hdr.append('extern const unsigned char txt_font[%d];\n' % len(font))
cfile.append(carr('txt_pal', TXTPAL)); hdr.append('extern const unsigned char txt_pal[16];\n')

# ---------------------------------------------------------------- text pages
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'textpages.py')).read())
for name, pg in TEXT_PAGES:
    lname = 'txt_' + name.lower()
    data = text_blob(pg)
    cfile.append(carr(lname, list(data)))
    hdr.append('extern const unsigned char %s[%d];\n#define TXT_%s_PAGES %d\n' % (lname, len(data), name, len(pg)))

# ---------------------------------------------------------------- pack image blobs into banks
fb_i = 0; cur = bytearray(); table = []
for name, data in blobs:
    if len(cur) + len(data) > 16384:
        banks[FRONT_BANKS[fb_i]] = bytes(cur); fb_i += 1; cur = bytearray()
    table.append((name, FRONT_BANKS[fb_i], 0x8000 + len(cur)))
    cur += data
banks[FRONT_BANKS[fb_i]] = bytes(cur)
for i, (name, b, a) in enumerate(table): hdr.append('#define IMG_%s %d\n' % (name, i))
hdr.append('#define IMG_COUNT %d\n' % len(table))
hdr.append('extern const unsigned char img_bank[%d];\nextern const unsigned int img_addr[%d];\n' % (len(table), len(table)))
fixc.append(carr('img_bank', [b for n, b, a in table]))
fixc.append(carr('img_addr', [a for n, b, a in table], 'unsigned int'))
print('front banks used', FRONT_BANKS[:fb_i + 1], [len(banks[b]) for b in FRONT_BANKS[:fb_i + 1]])
hdr.append('#endif\n')
open(GEN + 'menu_data.h', 'w').write(''.join(hdr))
open(GEN + 'menu_assets.c', 'w').write(''.join(cfile))
open(GEN + 'front_tab.c', 'w').write(''.join(fixc))
G['banks'] = banks
pickle.dump(G, open(GEN + 'banks_all.pkl', 'wb'))
