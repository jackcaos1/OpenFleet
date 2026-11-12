#!/usr/bin/env python3
"""Hilfsmodul: bildet ein Craft-Objekt im Speicher der Armada.exe nach und ruft die Gesundheitsfunktionen des Originals mit Unicorn auf
(Trefferwurf 0x464dd0, Schaden 0x465100, Systemschaden 0x467b70, Reparatur 0x4683d0, Schild 0x466b00, Besatzung 0x466ca0, Bildschritt 0x465e80).
Benutzung: o = Ship(machine, spec); o.hit(idx, damage, ...); o.state()"""
import struct, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import exe_run as r
from unicorn.x86_const import *

# Reihenfolge der Bloecke im Objekt: Schild, Antrieb, Waffen, Lebenserhaltung, Sensoren (je 0x38 Byte ab +0x4e0)
BLK = 0x4e0
FILE_ORDER = ['shield', 'engines', 'weapons', 'life', 'sensors']

def ret_cdecl(mm):
    esp = mm.reg(UC_X86_REG_ESP); ra = mm.r32(esp)
    mm.uc.reg_write(UC_X86_REG_ESP, esp + 4); mm.uc.reg_write(UC_X86_REG_EIP, ra)

def ret_n(mm, n):
    esp = mm.reg(UC_X86_REG_ESP); ra = mm.r32(esp)
    mm.uc.reg_write(UC_X86_REG_ESP, esp + 4 + 4 * n); mm.uc.reg_write(UC_X86_REG_EIP, ra)

class Ship:
    def __init__(self, exe='originalFiles/Armada.exe'):
        m = self.m = r.Machine(exe)
        H = m.heap
        self.O = H                      # Craft-Objekt
        self.cls = H + 0x4000           # Klasse (Einheiten-ODF)
        self.race = H + 0x5000          # Rasse
        self.lst = H + 0x6000           # leere Liste (Effekte)
        self.ctl = H + 0x7000           # Steuerblock
        O = self.O
        m.w32(O, 0x5ccdec)
        m.w32(O + 0xe0, self.cls); m.w32(O + 0x1a4, self.race); m.w32(O + 0x494, self.ctl)
        m.w32(O + 0xd0, self.lst); m.w32(self.lst, self.lst)
        m.w32(O + 0x620, 0xffffffff); m.w32(O + 0x690, 0xffffffff); m.w32(O + 0x194, 5)
        m.w32(self.cls + 0x90, 0)       # Name fuer Protokoll
        self.roll = 0
        import os
        m.fpcw = int(os.environ.get('FPCW', '0x7f'), 16)   # Einzelgenauigkeit wie Direct3D 7 sie beim Geraetestart setzt (Vorgabe); FPCW=0x27f fuer 53 Bit
        # Hilfsfunktionen und Aussenwelt ausblenden
        for a in (0x4ff960, 0x4ff9a0, 0x4ffb40):   # Protokoll
            m.hook(a, ret_cdecl)
        m.hook(0x5014d0, self._rand)               # Zufall: gewuerfelte Zahl vorgeben (cdecl, 1 Argument)
        m.hook(0x46d060, lambda mm: (mm.uc.reg_write(UC_X86_REG_EAX, 1), ret_cdecl(mm)))   # Team des lokalen Spielers = 1
        for a in (0x5070c0, 0x447870):          # thiscall, Aufgeraeumt vom Aufgerufenen
            m.hook(a, lambda mm: ret_n(mm, 1))
        m.hook(0x516be0, ret_cdecl)              # cdecl: Effekt entfernen
        m.hook(0x516540, lambda mm: (mm.uc.reg_write(UC_X86_REG_EAX, 0xffffffff), ret_cdecl(mm)))   # Effekt erzeugen (cdecl)
        m.hook(0x515d50, ret_cdecl)
        m.hook(0x468900, lambda mm: mm.ret_stdcall(0))   # Schild-Aus-Effekt (thiscall ohne Argumente)
        m.hook(0x4688c0, lambda mm: mm.ret_stdcall(0))
        m.hook(0x468990, self._explode)                  # Explode: nur Merker setzen
        self.exploded_calls = 0
        self._brk = H + 0x30000     # einfacher Speicher fuer operator new (0x5bb998, cdecl)
        def _new(mm):
            size = mm.arg(0)
            mm.uc.reg_write(UC_X86_REG_EAX, self._brk); self._brk += (size + 15) & ~15
            ret_cdecl(mm)
        m.hook(0x5bb998, _new)
        # _ftol (0x5bbb00, CRT-Thunk): auf 0 abschneiden wie die MSVC-Laufzeit (Rundung "zur Null")
        ftol = H + 0x9000
        code = bytes.fromhex('83ec08' 'd97c2404' '668b442404' '660d000c' '66890424' 'd92c24' 'df3c24' '8b0424' 'd96c2404' '83c408' 'c3')
        m.uc.mem_write(ftol, code)
        m.uc.mem_write(0x5bbb00, b'\xe9' + struct.pack('<i', ftol - (0x5bbb00 + 5)))

    def _rand(self, mm):
        mm.uc.reg_write(UC_X86_REG_EAX, self.roll)
        ret_cdecl(mm)

    def _explode(self, mm):
        m = self.m
        O = m.reg(UC_X86_REG_ECX)
        self.exploded_calls += 1
        m.uc.mem_write(O + 0x1bf, b'\x01')
        mm.ret_stdcall(0)

    # ----- Zustand schreiben/lesen -----
    def setup(self, spec, race, shield_delay=3.0, life_loss=2.0):
        m = self.m; O = self.O
        m.wf(O + 0x470, spec['shield_max']); m.wf(O + 0x46c, spec['shield_max']); m.wf(O + 0x468, 1.0)
        m.wf(O + 0x4c8, spec['crew_max']); m.wf(O + 0x4d8, spec['crew_max'])
        m.wf(self.cls + 0x21c, shield_delay); m.wf(self.cls + 0x220, spec['shield_rate']); m.wf(self.cls + 0x3c8, life_loss)
        m.wf(O + 0x5f8, spec['hull_pct']); m.wf(O + 0x5fc, spec.get('crit_pct', 0.0)); m.wf(self.cls + 0x3d0, spec.get('crit_pct', 0.0))
        for off, key in ((0x1f4, 'repair_strength'), (0x204, 'crew_yellow'), (0x208, 'crew_red'), (0x8, 'repair_yellow'), (0xc, 'repair_red'),
                         (0x10, 'weapon_yellow'), (0x14, 'weapon_red')):
            m.wf(self.race + off, race[key])
        for i, name in enumerate(FILE_ORDER):
            s = spec['sys'][name]
            b = O + BLK + 0x38 * i
            pts = s['points']
            m.uc.mem_write(b, bytes([1 if pts != 0 else 0]))
            m.w32(b + 4, pts); m.w32(b + 8, pts)
            m.wf(b + 0xc, s['hit_pct']); m.wf(b + 0x10, s['hit_pct'])
            m.wf(b + 0x14, s['crew_loss'])
            per = s['repair_time'] / pts if pts > 0 else s['repair_time']
            m.wf(b + 0x18, per); m.wf(b + 0x1c, per); m.wf(b + 0x20, 0.0)
            m.wf(b + 0x24, 0.5); m.wf(b + 0x28, 0.0); m.wf(b + 0x2c, 0.0)
            m.uc.mem_write(b + 0x30, b'\x00')
            m.w32(b + 0x34, self.lst)   # leere Effektliste

    def block(self, name): return self.O + BLK + 0x38 * FILE_ORDER.index(name)

    def state(self):
        m = self.m; O = self.O
        st = {'shield': m.rf(O + 0x46c), 'delay': m.rf(O + 0x4c4), 'crew': m.rf(O + 0x4d8), 'carry': m.rf(O + 0x600),
              'crit': m.rf(O + 0x5fc), 'derelict': m.r32(O + 0x4dc) & 0xff, 'exploded': m.r32(O + 0x1bf) & 0xff, 'sys': {}}
        for name in FILE_ORDER:
            b = self.block(name)
            st['sys'][name] = {'active': m.r32(b) & 0xff, 'points': struct.unpack('<i', struct.pack('<I', m.r32(b + 8)))[0],
                               'hit': m.rf(b + 0x10), 'per': m.rf(b + 0x1c), 'count': m.rf(b + 0x20), 'timer': m.rf(b + 0x2c)}
        return st

    # ----- Aufrufe -----
    def call(self, addr, args=(), ecx=None):
        import os
        if os.environ.get('TRACE') and not hasattr(self, '_ring'):
            from unicorn import UC_HOOK_CODE
            self._ring = []
            self.m.uc.hook_add(UC_HOOK_CODE, lambda uc, a, sz, ud: (self._ring.append(a), self._ring.__delitem__(0) if len(self._ring) > 30 else None))
        try:
            return self.m.call(addr, ecx=self.O if ecx is None else ecx, args=list(args))
        except RuntimeError:
            if hasattr(self, '_ring'): print('letzte Adressen:', [hex(x) for x in self._ring])
            raise

def f2i(x): return struct.unpack('<I', struct.pack('<f', x))[0]
