import os
"""Minimal ACME data extractor: label -> bytes for plain !byte blocks."""
import re
SRC = os.environ.get('SQUAREDOOM_SRC', os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), '..', 'SquareDoom')).rstrip('/') + '/'
def num(t):
    t = t.strip()
    if t.startswith('$'): return int(t[1:], 16)
    if t.startswith('%'): return int(t[1:], 2)
    return int(t)
def parse(fname):
    out = {}; cur = None; order = []
    for line in open(SRC + fname):
        line = line.split(';')[0].rstrip()
        if not line.strip(): continue
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)\s*$', line)
        if m:
            cur = m.group(1); out[cur] = []; order.append(cur); continue
        m = re.match(r'^\s*!byte\s+(.*)$', line)
        if m and cur is not None:
            try: out[cur] += [num(x) & 255 for x in m.group(1).split(',')]
            except ValueError: pass
            continue
        if not line.startswith('\t') and not line.startswith(' '): cur = None
    return out, order
