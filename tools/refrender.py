"""Python reference of the column renderer (editor previewView.js logic with the
C64 projection: rows = ceil(dh*1024/P)), compared with the emulator's framebuffer."""
import sys, math, struct
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from acme import SRC
state = open(sys.argv[1], 'rb').read()
ram = state[16384 + 32 + 16:]
syms = {}
for l in open('game.noi'):
    p = l.split()
    if len(p) >= 3 and p[0] == 'DEF': syms[p[1]] = int(p[2], 16)
def rd(name, n=1, off=0): a = syms[name] - 0xC000 + off; return ram[a:a + n]
u16 = lambda b: b[0] | (b[1] << 8)
px, py, pa = u16(rd('_playerx', 2)), u16(rd('_playery', 2)), rd('_playera')[0]
eye = rd('_eyeheight')[0]
lvl = rd('_level_num')[0]
lv = open(SRC + 'levels/e1m%d.bin' % lvl, 'rb').read()
M = lv[:1024]
fl = rd('_sec_floor', 200); ce = rd('_sec_ceil', 200)
fb = rd('_fb', 1536)
fbv = [u16(fb[i*2:i*2+2]) for i in range(768)]
COLS = 32; H = 24; HZ = 12
ang = [round(((c + 0.5) / COLS - 0.5) * 48) for c in range(COLS)]
def rows(dh, P):
    return min(13, math.ceil(abs(dh) * 1024 / max(P, 1) - 1e-9))
def proj(h, P):
    d = h - eye
    if d == 0: return HZ
    return HZ - rows(d, P) if d > 0 else HZ + rows(d, P)
mismatch = 0
ref = [[None] * COLS for _ in range(H)]
for c in range(COLS):
    a = (pa + ang[c]) & 255
    th = a * 2 * math.pi / 256
    dx, dy = math.sin(th), -math.cos(th)
    fish = math.cos(ang[c] * 2 * math.pi / 256)
    tx, ty = px / 2048, py / 2048
    mx, my = int(tx), int(ty)
    sx = 1 if dx > 0 else -1; sy = 1 if dy > 0 else -1
    ddx = abs(1 / dx) if abs(dx) > 1e-9 else 1e9; ddy = abs(1 / dy) if abs(dy) > 1e-9 else 1e9
    sdx = ((mx + 1 - tx) if sx > 0 else (tx - mx)) * ddx
    sdy = ((my + 1 - ty) if sy > 0 else (ty - my)) * ddy
    cur = M[my * 32 + mx]; yt, yb = 0, H
    for _ in range(64):
        if sdx < sdy: mx += sx; dist = sdx; sdx += ddx; side = 0
        else: my += sy; dist = sdy; sdy += ddy; side = 1
        nxt = M[my * 32 + mx] if 0 <= mx < 32 and 0 <= my < 32 else 0
        if nxt == cur: continue
        P = dist * fish * 256
        nc, nf = proj(ce[cur], P), proj(fl[cur], P)
        e = max(yt, min(yb, nc))
        for y in range(yt, e): ref[y][c] = 'C'
        yt = max(yt, e)
        if nf >= HZ:
            e = max(yt, min(yb, nf))
            for y in range(e, yb): ref[y][c] = 'F'
            yb = min(yb, e)
        if yt >= yb: break
        if not nxt:
            for y in range(yt, yb): ref[y][c] = 'W'
            break
        if ce[nxt] < ce[cur]:
            t = max(yt, min(yb, nc)); bo = max(yt, min(yb, proj(ce[nxt], P)))
            for y in range(t, bo): ref[y][c] = 'U'
            yt = max(yt, bo)
            if yt >= yb: break
        if fl[nxt] > fl[cur]:
            t = max(yt, min(yb, proj(fl[nxt], P))); bo = max(yt, min(yb, nf))
            for y in range(t, bo): ref[y][c] = 'L'
            yb = min(yb, t)
            if yt >= yb: break
        cur = nxt
# classify emulator cells: tile index -> W (wall pattern), F (flat pattern), other
def cls(v):
    t = v & 0x1ff
    if t == 0: return '.'
    if 1 <= t <= 186:
        k = (t - 1) % 31
        return 'w' if k < 15 else ('f' if k < 30 else 's')
    return 'x'
out_ref = []; out_emu = []
bad = 0
for y in range(H - 1):
    r = ''.join((ref[y][c] or '?') for c in range(COLS))
    e = ''.join(cls(fbv[y * 32 + c]) for c in range(COLS))
    out_ref.append(r); out_emu.append(e)
    for c in range(COLS):
        rr, ee = r[c], e[c]
        exp_wall = rr in 'WUL'; exp_flat = rr in 'CF'
        if ee == 'w' and not exp_wall: bad += 1
        if ee == 'f' and not exp_flat: bad += 1
print('player', px, py, pa, 'eye', eye)
for a, b in zip(out_ref, out_emu): print(a, ' ', b)
print('class mismatches (wall vs flat):', bad)
