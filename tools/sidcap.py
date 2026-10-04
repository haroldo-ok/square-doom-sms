"""Run the PSID through a 6502 core and capture SID writes per play call."""
import struct
from py65.devices.mpu6502 import MPU
from py65.memory import ObservableMemory
def capture(path, frames):
    d = open(path, 'rb').read()
    off, load, init, play = struct.unpack('>HHHH', d[6:14])
    data = d[off:]
    if load == 0: load = data[0] | (data[1] << 8); data = data[2:]
    mem = ObservableMemory()
    for i, b in enumerate(data): mem[load + i] = b
    sid = [0] * 32; writes = []
    cia = {}
    def wsid(addr, val):
        sid[addr - 0xD400] = val; writes.append((addr - 0xD400, val))
    def wcia(addr, val): cia[addr] = val
    mem.subscribe_to_write(range(0xD400, 0xD420), wsid)
    mem.subscribe_to_write(range(0xDC00, 0xDC10), wcia)
    cpu = MPU(memory=mem)
    def call(addr, a=0):
        cpu.pc = 0xFFF0; mem[0xFFF0] = 0x20; mem[0xFFF1] = addr & 255; mem[0xFFF2] = addr >> 8
        mem[0xFFF3] = 0xEA
        cpu.a = a; cpu.sp = 0xFF
        n = 0
        while cpu.pc != 0xFFF3 and n < 200000: cpu.step(); n += 1
        return n
    call(init, 0)
    out = []
    for f in range(frames):
        writes.clear(); call(play)
        out.append(list(sid))
    return out, cia, load, init, play
if __name__ == '__main__':
    st, cia, load, init, play = capture(__import__('acme').SRC + 'music/e1m1.sid', 300)
    print(hex(load), hex(init), hex(play), {hex(k): v for k, v in cia.items()})
    for f in range(0, 64, 2): print(f, ' '.join('%02x' % x for x in st[f][:25]))
