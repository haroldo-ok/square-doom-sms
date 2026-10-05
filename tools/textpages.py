"""C64 text pages reflowed for the SMS (shared by gen_front)."""
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
TEXT_PAGES = []
for name, f in (('READTHIS', 'readthis.txt'), ('CREDITS', 'credits.txt'), ('ORDER', 'order.txt'), ('ENDING', 'ending.txt')):
    pg = pages(f)
    if name == 'CREDITS': pg.append(['Sega Master System version', '', 'converted from the C64 game.', 'Title screen, menus, weapons', 'and fonts from Doom 1.9', 'shareware by id Software.'])
    TEXT_PAGES.append((name, pg))
TEXT_PAGES.append(('CONTROL', [[l for l in CONTROL.split('\n')]]))
