#!/usr/bin/env python3
"""Laesst den ORIGINAL-Planer (Armada.exe @0x6322b8) im Unicorn-Emulator laufen: Gitter, Stempel, Wegsuche.
Liefert Soll-Werte fuer die C++-Portierung (engine/src/game/PathGrid*)."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from exe_run import Machine
from unicorn.x86_const import *

STUB = bytes.fromhex('83ec0cd97c2408668b442408660d000c668944240ad96c240adf3c24d96c24088b04248b54240483c40cc38d7426009083ec04d93c24668b04246625fff3660d00086689442402d96c2402dd442408d9fcd92c2483c404c38db426000000009083ec04d93c24668b04246625fff3660d00046689442402d96c2402dd442408d9fcd92c2483c404c3')

class PathMachine(Machine):
    def __init__(self, exe, res=50.0, bounds=(-1500.0, -1500.0, 1500.0, 1500.0)):
        super().__init__(exe)
        self.brk = self.heap + 0x1000
        self.uc.mem_map(0x70001000, 0x1000); self.uc.mem_write(0x70001000, STUB)
        # cfg-Globals (RTS_CFG.h)
        self.w32(0x5fb868, 90000); self.w32(0x5fb870, 0xffffffff); self.w32(0x5fb874, 100); self.w32(0x5fb878, 200)
        self.w32(0x5fb86c, 1); self.w32(0x5fb87c, 1); self.wf(0x5fbc48, res)
        self.wf(0x6343f0, bounds[0]); self.wf(0x6343f8, bounds[1]); self.wf(0x6343fc, bounds[2]); self.wf(0x634404, bounds[3])
        self.wf(0x5fbc0c, 67.0); self.wf(0x633a74, 10.0)
        # Allokator
        for a in (0x5bb998, 0x4f7200, 0x4fdf80, 0x407330):
            self.hook(a, self._h_new)
        for a in (0x5bb63c, 0x4fe180): self.hook(a, lambda m: m.ret_cdecl())
        self.hook(0x448330, lambda m: m.ret_stdcall(5))
        self.hook(0x407e70, lambda m: m.ret_stdcall(1))   # Knoten-Destruktor: nicht noetig (Speicher wird nicht freigegeben)
        sent = self.alloc(16); self.w32(sent, sent); self.w32(sent + 4, sent); self.w32(0x631eec, sent)   # globale Pfadobjekt-Liste
        self.w32(0x6d7fa4, 0x70001000); self.w32(0x6d7fe0, 0x70001030); self.w32(0x6d7fe4, 0x70001060)
    def alloc(self, n):
        n = (n + 15) & ~15
        a = self.brk; self.brk += n
        if self.brk > self.heap + 0x3f0000: raise RuntimeError('Heap voll')
        return a
    def ret_cdecl(self, eax=None):
        uc = self.uc; esp = self.reg(UC_X86_REG_ESP); ra = self.r32(esp)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        if eax is not None: uc.reg_write(UC_X86_REG_EAX, eax)
        uc.reg_write(UC_X86_REG_EIP, ra)
    def _h_new(self, m):
        pc = self.reg(UC_X86_REG_EIP)
        n = self.arg(0); print('alloc', hex(pc), hex(n)) if n > 0x100000 else None
        a = self.alloc(n); self.uc.mem_write(a, b'\0' * ((n + 15) & ~15))
        self.ret_cdecl(a)

    def make_planner(self):
        p = self.alloc(0x30)
        self.call(0x408e30, ecx=p)
        return p

if __name__ == '__main__':
    m = PathMachine(os.path.join(os.path.dirname(__file__), '..', 'originalFiles', 'Armada.exe'))
    p = m.make_planner(); print('planner', hex(p), [hex(m.r32(p + 4 * i)) for i in range(12)])

def fbits(x): return struct.unpack('<I', struct.pack('<f', x))[0]

def run_scenario(fields, start, goal, bounds=(-1500., -1500., 1500., 1500.), res=50.0, soft=False):
    # soft=True: weicher Ring wie 0x44e7e9 (Byte 0x5fdfc8 = 1 im Original): Radius 75+40, Kosten 300, VOR dem Kern
    m = PathMachine(os.path.join(os.path.dirname(__file__), '..', 'originalFiles', 'Armada.exe'), res, bounds)
    m.w32(0x6322b8, m.make_planner()); p = m.r32(0x6322b8)
    m.call(0x4093b0, ecx=p)
    for (x, z) in fields:
        if soft:
            cp = m.alloc(4); m.w32(cp, 300); r2 = fbits(115.0)
            m.call(0x409900, ecx=p, args=[fbits(x), fbits(z), fbits(x), fbits(z), r2, r2, cp])
        # Asteroidenfeld (0x44e848): Kern unpassierbar, Radius ASTEROID_BELT_PATHING_DISTANCE/2
        r = fbits(75.0)
        m.call(0x409900, ecx=p, args=[fbits(x), fbits(z), fbits(x), fbits(z), r, r, p + 4])
    out = m.alloc(16)
    args = [fbits(start[0]), fbits(start[1]), fbits(start[2]), fbits(goal[0]), fbits(goal[1]), fbits(goal[2]), 0, 0, out, 0]
    path = m.call(0x40a310, ecx=p, args=args)
    pts = []
    if path:
        n = m.r32(path); arr = m.r32(path + 4)
        pts = [tuple(m.rf(arr + 12 * i + 4 * k) for k in range(3)) for i in range(n)]
    return m, path, pts

if __name__ == '__main__' and len(sys.argv) > 1 and sys.argv[1] == 'scenario':
    m, path, pts = run_scenario([(0.0, 0.0)], (-600.0, 30.0, 0.0), (600.0, 30.0, 0.0))
    print('path', hex(path)); [print(p) for p in pts]
