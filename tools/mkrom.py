"""Build the SMS ROM from SDCC's .ihx + generated data banks.
banks 0-1 fixed code; 2 front-end code+data; 4-6 tiles/weapons; 7 splash;
8-16 render banks (map + textures + tables); 17-25 logic banks (map + logic
code linked at 0x8400 + level init data at the end)."""
import sys, pickle
ihx, mapf, pk, out = sys.argv[1:5]
mem = {}
ext = 0
for line in open(ihx):
    line = line.strip()
    if not line.startswith(':'): continue
    n = int(line[1:3], 16); addr = int(line[3:7], 16); typ = int(line[7:9], 16)
    data = bytes.fromhex(line[9:9 + 2 * n])
    if typ == 4: ext = int.from_bytes(data, 'big') << 16; continue
    if typ != 0: continue
    for i, b in enumerate(data): mem[ext + addr + i] = b
def region(lo, hi):
    return bytearray(mem.get(a, 0) for a in range(lo, hi))
assert max(a for a in mem if a < 0x10000) < 0x8000, 'fixed code overflows 32 KiB'
import re as _re
_noi = {l.split()[1]: int(l.split()[2], 16) for l in open(mapf.replace('.map', '.noi')) if l.startswith('DEF ')}
assert _noi['s__DATA'] + _noi['l__DATA'] + _noi.get('l__INITIALIZED', 0) <= 0xD900, 'RAM data runs into the clip pages at 0xD900'
G = pickle.load(open(pk, 'rb'))
banks = dict(G['banks']); maps = G['maps']
rom = {}
rom[0] = region(0, 0x4000); rom[1] = region(0x4000, 0x8000)
rom[2] = region(0x28000, 0x2C000)
logic = region(0x38400, 0x3C000)
used_logic = max([a for a in mem if 0x38400 <= a < 0x3C000] or [0x38400]) - 0x38400 + 1
init_addr = G['init_addr']
assert 0x8400 + used_logic <= init_addr, 'logic code runs into the level init data'
for n in range(9):
    b = bytearray(maps[n]) + logic
    b[init_addr - 0x8000:init_addr - 0x8000 + len(G['inits'][n])] = G['inits'][n]
    rom[17 + n] = b
for b, d in banks.items(): rom[b] = bytearray(d)
nb = max(rom) + 1
img = bytearray()
for b in range(nb):
    d = rom.get(b, bytearray())
    assert len(d) <= 0x4000, (b, len(d))
    img += d + bytes(0x4000 - len(d))
size = 1
while size < len(img): size <<= 1
img += bytes(size - len(img))
# SEGA header at 0x7FF0: checksum over the first 32 KiB (size code 0xC), region 4 (export)
assert img[0x7FF0:0x7FF8] == b'TMR SEGA', 'SEGA header missing'
img[0x7FFF] = (img[0x7FFF] & 0xF0) | 0x0C
cs = sum(img[0:0x7FF0]) & 0xFFFF
img[0x7FFA] = cs & 255; img[0x7FFB] = cs >> 8
open(out, 'wb').write(img)
fixed_used = max(a for a in mem if a < 0x7FF0) + 1
print('ROM %s: %d KiB, %d banks; fixed %d bytes, front %d bytes, logic code %d bytes; checksum %04X' % (
    out, len(img) // 1024, nb, fixed_used, len([a for a in mem if 0x28000 <= a < 0x2C000]), used_logic, cs))
