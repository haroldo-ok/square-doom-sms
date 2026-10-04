"""PC-speaker SFX -> 60 Hz PSG pitch streams; SID tune -> PSG event stream."""
import sys, math, re
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from common import *
import sidcap
PSG_CLK = 3579545
out_c = ['#include "sound_data.h"\n']; out_h = ['#ifndef SOUND_DATA_H\n#define SOUND_DATA_H\n']
def arr(name, data, typ='unsigned char'):
    out_c.append(carr(name, data, typ)); out_h.append('extern const %s %s[%d];\n' % (typ, name, len(data)))
def period(hz):
    if hz <= 0: return 0
    return max(1, min(1023, round(PSG_CLK / (32 * hz))))
# ---- SFX ----
hz = [0] * 96
for line in open(SRC + 'pcsounds/speaker.txt'):
    m = re.match(r'^(\d+)\s+(\d+|-)\s+([\d.]+|-)', line)
    if m and 1 <= int(m[1]) <= 95 and m[3] != '-': hz[int(m[1])] = float(m[3])
arr('pcs_period', [period(h) for h in hz], 'unsigned int')
EFFECTS = ['dpclaw', 'dpdmpain', 'dpdoropn', 'dpdorcls', 'dpitemup', 'dpoof', 'dpbgact', 'dppistol', 'dpplpain',
           'dpsgcock', 'dpsgtdth', 'dpshotgn', 'dpstnmov', 'dpsawidl', 'dpsawful', 'dpsawhit', 'dppunch', 'dpbarexp']
EXTRA = {}   # menu sounds share the same lumps
blob = []; offs = []
for name in EFFECTS:
    b = open(SRC + 'pcsounds/%s.lmp' % name, 'rb').read()
    n = b[2] | (b[3] << 8); data = b[4:4 + n]
    frames = max(1, round(n * 60 / 140))
    res = [data[min(n - 1, (f * 140) // 60)] for f in range(frames)]
    offs.append(len(blob)); blob += [len(res)] + res
arr('sfx_data', blob); arr('sfx_off', offs, 'unsigned int')
arr('sfx_priority', [2, 2, 1, 1, 1, 2, 0, 2, 2, 2, 2, 2, 0, 0, 1, 2, 2, 2])
# ---- music ----
AT = [2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000]
DR = [6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000]
L = 2304; start = 195
st, cia, *_ = sidcap.capture(SRC + 'music/e1m1.sid', start + L + 1)
TICK_MS = 1000 * 39409 / 985248
env = [0.0, 0.0]; phase = ['rel', 'rel']; gate = [0, 0]
events = []
for t in range(start + L):
    s = st[t]
    row = []
    for v in range(2):
        r = s[v * 7:(v + 1) * 7]
        fn = r[0] | (r[1] << 8); ctrl = r[4]; a, d = r[5] >> 4, r[5] & 15; su, rl = (r[6] >> 4) / 15.0, r[6] & 15
        g = ctrl & 1
        if g and not gate[v]: phase[v] = 'att'
        if not g and gate[v]: phase[v] = 'rel'
        gate[v] = g
        ms = TICK_MS
        if phase[v] == 'att':
            env[v] += ms / AT[a]
            if env[v] >= 1: env[v] = 1; phase[v] = 'dec'
        elif phase[v] == 'dec':
            env[v] = max(su, env[v] - (ms / DR[d]) * (1 - su) * 1.5)
        else:
            env[v] = max(0.0, env[v] - ms / DR[rl] * 1.2)
        lvl = env[v] if (ctrl & 0xF0) and not (ctrl & 8) else 0.0
        att = 15 if lvl < 0.02 else min(15, max(0, round(-20 * math.log10(lvl) / 2)))
        f = fn * 985248 / 16777216
        noise = bool(ctrl & 0x80)
        row.append((period(f) if f > 0 else 0, att, noise, f))
    events.append(row)
# map to PSG: tone0 <- voice0 (if not noise), tone1 <- voice1 (if not noise), noise <- whichever noise
stream = []; prev = [None] * 5
loop_ofs = None
def noisemode(f):
    return 0 if f > 3000 else (1 if f > 1000 else 2)
idle = 0
NMP = [None]
for t, row in enumerate(events):
    if t == start:
        if idle: stream.append(0x80 | idle); idle = 0
        loop_ofs = len(stream)
    cur = [15, 15, None, None, 15]  # att0 att1 per0 per1 noiseatt; noise mode
    nm = None
    vals = {}
    for v in range(2):
        p, att, noise, f = row[v]
        if noise:
            vals['natt'] = min(vals.get('natt', 15), att); nm = noisemode(f)
            vals['att%d' % v] = 15
        else:
            vals['att%d' % v] = att
            if att < 15: vals['per%d' % v] = p
    fl = 0; data = []
    for i, k in enumerate(['per0', 'att0', 'per1', 'att1', 'natt']):
        if k not in vals: continue
        if vals[k] != prev[i]:
            fl |= 1 << i
            if k.startswith('per'): data += [vals[k] & 255, vals[k] >> 8]
            else: data.append(vals[k])
            prev[i] = vals[k]
    if nm is not None and nm != NMP[0]:
        NMP[0] = nm; fl |= 0x20; data.append(nm)
    if fl == 0:
        idle += 1
        if idle == 0x7F: stream.append(0xFF); idle = 0
        continue
    if idle: stream.append(0x80 | idle); idle = 0
    stream.append(fl); stream += data
if idle: stream.append(0x80 | idle)
stream.append(0x00)  # end -> loop
arr('music_data', stream)
out_h.append('#define MUSIC_LOOP %d\n' % loop_ofs)
out_h.append('#define MUSIC_TICK_RATE_X100 %d\n' % round(100000 / TICK_MS))
print('music bytes', len(stream), 'loop at', loop_ofs, 'start tick', start, 'sfx bytes', len(blob))
out_h.append('#endif\n')
open(GEN + 'sound_data.c', 'w').write(''.join(out_c)); open(GEN + 'sound_data.h', 'w').write(''.join(out_h))
