import math, os
GEN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'gen') + '/'
SRC = os.environ.get('SQUAREDOOM_SRC', os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), '..', 'SquareDoom')).rstrip('/') + '/'
# C64 colour -> SMS RGB (2 bits/channel)
C64SMS = [(0,0,0),(3,3,3),(2,0,0),(1,3,3),(2,1,2),(1,2,1),(1,0,2),(3,3,1),
          (2,1,0),(1,1,0),(2,1,1),(1,1,1),(2,2,2),(2,3,2),(1,1,3),(3,3,3)]
def smsrgb(c): r, g, b = C64SMS[c]; return r | (g << 2) | (b << 4)
# palette pairing: C64 colour -> (fg index k, uses sprite palette)
PAIR = {8:(1,0),9:(1,1),11:(2,0),12:(2,1),5:(3,0),2:(3,1),6:(4,0),7:(4,1),
        15:(5,0),1:(5,1),4:(6,0),14:(6,1),3:(7,0),10:(7,1),13:(8,0),0:(8,1)}
BGPAL_C64 = [0,8,11,5,6,15,4,3,13,0,0,0,0,0,0,0]
SPRPAL_C64 = [0,9,12,2,7,1,14,10,0,8,11,15,12,5,0,0]   # 12 = HL init, 14 = backdrop
def carr(name, data, typ='unsigned char', per=16, static=False):
    s = ('' if not static else 'static ') + 'const %s %s[%d] = {\n' % (typ, name, len(data))
    for i in range(0, len(data), per):
        s += '  ' + ','.join(str(x) for x in data[i:i+per]) + ',\n'
    return s + '};\n'
def tile_from_1bpp(rows, fg, bg=0):
    out = []
    for r in rows:
        for p in range(4):
            b = 0
            for x in range(8):
                v = fg if r & (0x80 >> x) else bg
                if (v >> p) & 1: b |= 0x80 >> x
            out.append(b)
    return bytes(out)
def tile_from_px(px):  # px[y][x] palette indices
    out = []
    for y in range(8):
        for p in range(4):
            b = 0
            for x in range(8):
                if (px[y][x] >> p) & 1: b |= 0x80 >> x
            out.append(b)
    return bytes(out)
