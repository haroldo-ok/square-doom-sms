"""RGB image -> SMS tiles: 64-colour quantisation (optional ordered dither),
1 or 2 sixteen-colour palettes (the second = sprite palette, picked per tile),
and tile-count reduction by merging the most similar tiles."""
import numpy as np
LV = np.array([0, 85, 170, 255])
BAYER = np.array([[0, 2], [3, 1]]) / 4.0 - 0.375
def to_sms(img, dither=0.0):
    """img: HxWx3 uint8 -> HxWx3 levels 0..3"""
    a = img.astype(float) / 85.0
    if dither:
        h, w = a.shape[:2]
        t = np.tile(BAYER, (h // 2 + 1, w // 2 + 1))[:h, :w]
        a = a + t[:, :, None] * dither
    return np.clip(np.rint(a), 0, 3).astype(int)
def code(c): return c[..., 0] + 4 * c[..., 1] + 16 * c[..., 2]
def uncode(k): return (k % 4, (k // 4) % 4, k // 16)
def dist2(a, b):
    a = np.array(uncode(a)); b = np.array(uncode(b))
    return float(((a - b) ** 2).sum())
D2 = np.array([[dist2(i, j) for j in range(64)] for i in range(64)])
def pick_palette(hist, n=16, fixed=()):
    """greedy: colours minimising total squared error of the histogram"""
    pal = list(fixed)
    keys = np.nonzero(hist)[0]
    while len(pal) < n:
        best = None
        for c in keys:
            if c in pal: continue
            p = pal + [c]
            err = (hist[keys] * D2[keys][:, p].min(axis=1)).sum()
            if best is None or err < best[0]: best = (err, c)
        if best is None: break
        pal.append(best[1])
    while len(pal) < n: pal.append(0)
    return pal
def convert(img, npal=1, max_tiles=448, dither=0.0, fixed0=0):
    """-> dict(tiles=list of 32-byte, map=list of (tile, palsel), pals=[[16 codes]...])"""
    q = code(to_sms(img, dither))
    H, W = q.shape
    cells = q.reshape(H // 8, 8, W // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    hist_all = np.bincount(cells.ravel(), minlength=64).astype(float)
    if npal == 1:
        pals = [pick_palette(hist_all, 16, (fixed0,))]
        assign = np.zeros(len(cells), int)
    else:
        # 2 palettes: split tiles by k-means on their colour histograms
        hists = np.array([np.bincount(c, minlength=64) for c in cells], float)
        assign = (np.arange(len(cells)) % 2)
        for it in range(6):
            pals = [pick_palette(hists[assign == g].sum(0) + 1e-9 * hist_all, 16, (fixed0,)) for g in range(2)]
            errs = np.stack([(hists * D2[:, p].min(axis=1)[None, :]).sum(1) for p in pals], 1)
            na = errs.argmin(1)
            if (na == assign).all(): break
            assign = na
    # map pixels to palette indices
    idx = np.zeros_like(cells)
    for t in range(len(cells)):
        p = pals[assign[t]]
        idx[t] = D2[cells[t]][:, p].argmin(axis=1)
    # dedupe (tile content + palette) and merge down to max_tiles
    keys = [tuple(idx[t]) + (assign[t],) for t in range(len(cells))]
    uniq = {}
    for k in keys: uniq[k] = uniq.get(k, 0) + 1
    U = list(uniq)
    if len(U) > max_tiles:
        # merge using RGB distance of tiles (same palette group only)
        rgb = np.array([[uncode(pals[k[64]][v]) for v in k[:64]] for k in U], float).reshape(len(U), -1)
        cnt = np.array([uniq[k] for k in U], float)
        grp = np.array([k[64] for k in U])
        alive = np.ones(len(U), bool)
        rep = np.arange(len(U))
        Dm = ((rgb[:, None, :] - rgb[None, :, :]) ** 2).sum(-1)
        Dm[grp[:, None] != grp[None, :]] = np.inf
        np.fill_diagonal(Dm, np.inf)
        n_alive = len(U)
        while n_alive > max_tiles:
            W2 = Dm * np.minimum(cnt[:, None], cnt[None, :])
            i, j = np.unravel_index(np.argmin(W2), W2.shape)
            keep, drop = (i, j) if cnt[i] >= cnt[j] else (j, i)
            cnt[keep] += cnt[drop]; alive[drop] = False; n_alive -= 1
            rep[rep == drop] = keep
            Dm[drop, :] = np.inf; Dm[:, drop] = np.inf
        final = {}
        for n, k in enumerate(U): final[k] = U[rep[n]]
    else:
        final = {k: k for k in U}
    order = sorted(set(final.values()), key=lambda k: U.index(k))
    tid = {k: n for n, k in enumerate(order)}
    tiles = []
    for k in order:
        b = []
        for y in range(8):
            for pl in range(4):
                v = 0
                for x in range(8):
                    if (k[y * 8 + x] >> pl) & 1: v |= 0x80 >> x
                b.append(v)
        tiles.append(bytes(b))
    tmap = [(tid[final[k]], k[64]) for k in keys]
    return dict(tiles=tiles, map=tmap, pals=pals)
def preview(res, path, w=32, h=24):
    from PIL import Image
    im = Image.new('RGB', (w * 8, h * 8))
    for n, (t, ps) in enumerate(res['map']):
        tb = res['tiles'][t]; pal = res['pals'][ps]
        for y in range(8):
            for x in range(8):
                v = sum(((tb[y * 4 + p] >> (7 - x)) & 1) << p for p in range(4))
                im.putpixel(((n % w) * 8 + x, (n // w) * 8 + y), tuple(int(LV[c]) for c in uncode(pal[v])))
    im.save(path)
def sms_rgb_byte(k): r, g, b = uncode(k); return r | (g << 2) | (b << 4)
