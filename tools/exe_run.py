#!/usr/bin/env python3
"""Fuehrt Funktionen aus Armada.exe mit Unicorn (echtes x87) aus - zum Abgleich der C++-Portierungen.
Benutzung als Modul: m = Machine('originalFiles/Armada.exe'); m.call(addr, ecx=..., args=[...]) """
import struct
from unicorn import *
from unicorn.x86_const import *

class Machine:
    def __init__(self, exe):
        d = open(exe, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        n = struct.unpack_from('<H', d, pe + 6)[0]; opt = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        size_img = struct.unpack_from('<I', d, pe + 24 + 56)[0]
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc = self.uc
        uc.mem_map(self.base, (size_img + 0xfff) & ~0xfff)
        uc.mem_write(self.base, d[:struct.unpack_from('<I', d, pe + 24 + 60)[0]])
        for i in range(n):
            o = pe + 24 + opt + i * 40
            vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
            uc.mem_write(self.base + va, d[ro:ro + min(rs, vs)])
        self.stack = 0x7f000000
        uc.mem_map(self.stack - 0x100000, 0x200000)
        self.heap = 0x10000000
        uc.mem_map(self.heap, 0x400000)
        uc.mem_map(0x7ff00000, 0x1000)   # fs:0 etc.
        uc.mem_map(0x70000000, 0x1000)   # Rueckkehr-Adresse
        uc.mem_write(0x70000000, b'\xf4')
        self.hooks = {}
        uc.hook_add(UC_HOOK_CODE, self._code)
        self.fsbase = 0x7ff00000
        self.fpcw = 0x27f   # x87-Steuerwort fuer call(): 0x27f = 53 Bit, 0x7f = 24 Bit (Einzelgenauigkeit)

    def _code(self, uc, addr, size, ud):
        b = bytes(uc.mem_read(addr, min(size, 7)))
        if b and b[0] == 0x64:   # fs:0-Zugriffe der SEH-Kette ueberspringen
            if b[1] == 0xa1: uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_EIP, addr + size)
            return
        h = self.hooks.get(addr)
        if h:
            h(self)

    def hook(self, addr, fn): self.hooks[addr] = fn
    def r32(self, a): return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]
    def rf(self, a): return struct.unpack('<f', bytes(self.uc.mem_read(a, 4)))[0]
    def w32(self, a, v): self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
    def wf(self, a, v): self.uc.mem_write(a, struct.pack('<f', v))
    def reg(self, r): return self.uc.reg_read(r)
    def arg(self, i): return self.r32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i)   # nur in Hooks am Funktionsanfang
    def ret_stdcall(self, nargs, eax=None):
        uc = self.uc
        esp = self.reg(UC_X86_REG_ESP); ra = self.r32(esp)
        uc.reg_write(UC_X86_REG_ESP, esp + 4 + 4 * nargs)
        if eax is not None: uc.reg_write(UC_X86_REG_EAX, eax)
        uc.reg_write(UC_X86_REG_EIP, ra)

    def call(self, addr, ecx=0, args=(), limit=2000000):
        uc = self.uc
        esp = self.stack
        for a in reversed(args):
            esp -= 4; uc.mem_write(esp, struct.pack('<I', a & 0xffffffff))
        esp -= 4; uc.mem_write(esp, struct.pack('<I', 0x70000000))
        uc.reg_write(UC_X86_REG_ESP, esp); uc.reg_write(UC_X86_REG_EBP, 0)
        uc.reg_write(UC_X86_REG_ECX, ecx)
        # x87 Praezision: 53/64 bit (Standard-FPU-CW 0x27f = 53 Bit wie von MSVC-Laufzeit gesetzt)
        try: uc.reg_write(UC_X86_REG_FPCW, self.fpcw)
        except Exception: pass
        uc.mem_write(self.fsbase, b'\xff\xff\xff\xff')
        try:
            uc.emu_start(addr, 0x70000000, count=limit)
        except UcError as e:
            raise RuntimeError('%s bei EIP=%x ESP=%x' % (e, uc.reg_read(UC_X86_REG_EIP), uc.reg_read(UC_X86_REG_ESP)))
        return uc.reg_read(UC_X86_REG_EAX)
