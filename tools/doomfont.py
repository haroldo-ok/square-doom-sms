"""Harvest Doom's big menu font from the M_* / WI* lumps (word images with known
text): split each lump into glyph column runs and map them to characters."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wad
SOURCES = [('M_NGAME', 'New Game'), ('M_OPTION', 'Options'), ('M_RDTHIS', 'Read This!'),
           ('M_QUITG', 'Quit Game'), ('M_LOADG', 'Load Game'), ('M_SAVEG', 'Save Game'),
           ('M_EPISOD', 'Which Episode?'), ('M_EPI1', 'Knee-Deep in the Dead'),
           ('M_EPI2', 'The Shores of Hell'), ('M_EPI3', 'Inferno'),
           ('M_HURT', 'Hurt me plenty.'), ('M_JKILL', "I'm too young to die."),
           ('M_ROUGH', 'Hey, not too rough.'), ('M_ULTRA', 'Ultra-Violence.'),
           ('M_NMARE', 'Nightmare!'), ('M_SFXVOL', 'Sfx Volume'), ('M_MUSVOL', 'Music Volume'),
           ('M_ENDGAM', 'End Game'), ('M_MESSG', 'Messages:'), ('M_DETAIL', 'Graphic Detail:'),
           ('M_SCRNSZ', 'Screen Size'), ('M_MSENS', 'Mouse sensitivity'), ('M_SVOL', 'Sound Volume'),
           ('M_SKILL', 'Choose Skill Level:'), ('M_PAUSE', 'Pause'), ('M_DISP', 'Display'), ('M_LGTTL', 'Load Game'), ('M_SGTTL', 'Save Game')]
def lum(v):
    r, g, b = wad.PAL[v]; return r * 3 + g * 6 + b
def runs(name, thr=330):
    w, h, lo, to, px = wad.patch(name)
    cols = [any((x, y) in px and lum(px[(x, y)]) > thr for y in range(h)) for x in range(w)]
    out = []; x = 0
    while x < w:
        if cols[x]:
            s = x
            while x < w and cols[x]: x += 1
            out.append((s, x))
        else: x += 1
    return w, h, px, out
def harvest():
    glyphs = {}
    for name, text in SOURCES:
        if name not in wad.LUMPS: continue
        w, h, px, rs = runs(name)
        if len(rs) != len([c for c in text if c != ' ']):
            for thr in (250, 400, 470, 520):
                w, h, px, rs = runs(name, thr)
                if len(rs) == len([c for c in text if c != ' ']): break
        chars = [c for c in text if c != ' ']
        if len(rs) != len(chars): continue
        bounds = [s for s, e in rs] + [w]
        for i, ((s, e), ch) in enumerate(zip(rs, chars)):
            if ch in glyphs: continue
            e2 = min(bounds[i + 1], e + 2)          # keep the shadow column(s)
            g = {(x - s, y): v for (x, y), v in px.items() if s <= x < e2}
            e = e2
            glyphs[ch] = (e - s, h, g)
        if os.environ.get('FONTDBG'): print(name, 'ok')
    return glyphs
def render(text, glyphs, space=6, gap=1):
    """-> (w, h, px) using harvested glyphs"""
    x = 0; px = {}; H = max(g[1] for g in glyphs.values())
    for ch in text:
        if ch == ' ': x += space; continue
        gw, gh, g = glyphs[ch]
        for (gx, gy), v in g.items(): px[(x + gx, gy)] = v
        x += gw + gap
    return x - gap, H, px
if __name__ == '__main__':
    g = harvest()
    print(len(g), ''.join(sorted(g)))
    for t in ['Controls', 'Credits', 'Back', 'Effects Volume']:
        try: print(t, render(t, g)[:2])
        except KeyError as e: print(t, 'missing', e)
