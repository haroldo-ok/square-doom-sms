import re, sys, bisect
mod, sym = sys.argv[1], sys.argv[2]
syms = {}
for l in open('game.noi'):
    p = l.split()
    if len(p) >= 3 and p[0] == 'DEF': syms[p[1]] = int(p[2], 16)
start = syms[sym]
nxt = min([v for v in syms.values() if v > start] + [start + 0x400])
prof = {int(a, 16): int(c) for a, c in (l.split() for l in open('prof.out'))}
rows = []
for l in open(mod + '.lst'):
    m = re.match(r'^\s+([0-9A-F]{6})\s.*?\s(\d+)\s(.*)$', l)
    if m: rows.append((int(m.group(1), 16), m.group(3).strip()))
off0 = [o for o, t in rows if t.startswith(sym + ':')][0]
base = start - off0
for a in sorted(prof):
    if start - 31 <= a < nxt and prof[a] > 20000:
        lo = a - base
        src = [t for o, t in rows if lo <= o < lo + 32 and t and not t.startswith(';')]
        print('%5x %8d | %s' % (lo, prof[a], ' / '.join(src)[:170]))
