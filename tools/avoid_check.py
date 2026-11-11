#!/usr/bin/env python3
"""HoverAvoid (C++) gegen die Hilfsfunktionen des Originals (Armada.exe, Unicorn): 0x424860, 0x424ba0, 0x4250e0, 0x424b50, 0x425200, 0x425300.
Die Schleife/Filter aus 0x42ef70 sind hier in Python nachgebildet (aus dem Disassembly gelesen)."""
import sys, struct, math, subprocess
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import exe_run as r
F = lambda v: struct.unpack('<I', struct.pack('<f', v))[0]
f32 = lambda v: struct.unpack('<f', struct.pack('<f', v))[0]
txt = subprocess.run(['/tmp/avoid_dump'], capture_output=True, text=True).stdout.split('\n')
m = r.Machine('originalFiles/Armada.exe')
H = m.heap
def ret_cdecl_eax_arg0(mm):
    esp = mm.reg(r.UC_X86_REG_ESP); ra = mm.r32(esp); a0 = mm.r32(esp + 4)
    mm.uc.reg_write(r.UC_X86_REG_EAX, a0); mm.uc.reg_write(r.UC_X86_REG_ESP, esp + 4); mm.uc.reg_write(r.UC_X86_REG_EIP, ra)
m.hook(0x5bbb88, ret_cdecl_eax_arg0)   # __RTDynamicCast: hier immer "ist Zielklasse"
CR = H + 0x10000; CTX = H + 0x20000; TMP = H + 0x30000; CLS = H + 0x31000; OUT = H + 0x32000
def wv(a, v):
    for i, x in enumerate(v): m.wf(a + 4 * i, x)
def rv(a, n=3): return [m.rf(a + 4 * i) for i in range(n)]
i = 0; worst = 0; scenes = 0
while i < len(txt):
    if not txt[i].startswith('C'): i += 1; continue
    c = [float(x) for x in txt[i].split()[1:]]; n = int(c[14]); i += 1
    obs = []
    for k in range(n): obs.append(txt[i].split()[1:]); i += 1
    ref = [float(x) for x in txt[i].split()[1:]]; i += 1
    px, py, pz, fx, fz, speed, rad, prx, prz, dx, dy, dz = c[:12]
    for k in range(0x400): m.w32(CR + 4 * k, 0)
    wv(CR + 0x12c, [px, py, pz]); m.wf(CR + 0x138, speed); m.wf(CR + 0x60, rad); wv(CR + 0x80, [fx, 0, fz]); wv(CR + 0x49c, [prx, 0, prz])
    # Objekte
    ob_addr = []
    for k, o in enumerate(obs):
        a = H + 0x40000 + 0x1000 * k; ob_addr.append(a)
        for q in range(0x400): m.w32(a + 4 * q, 0)
        v = [float(x) for x in o[:8]]
        wv(a + 0x12c, v[0:3]); wv(a + 0x140, v[3:6]); m.wf(a + 0x138, v[6]); m.wf(a + 0x60, v[7])
        mobile, nodge, hasp = int(o[8]), int(o[9]), int(o[10])
        m.w32(a + 0xbc, 4 | (8 if mobile else 0))
        cls = CLS + 0x1000 * k; m.uc.mem_write(cls + 0x235, bytes([nodge])); m.w32(a + 0xe0, cls)
        if hasp:
            pa = a + 0x800; m.w32(a + 0xe4, pa); m.uc.mem_write(pa + 0xc0, b'\x01'); wv(pa + 0xb4, [float(x) for x in o[11:14]])
    d = [dx, dy, dz]
    R = f32(speed * 4.0)
    wv(TMP, d)
    m.call(0x425300, args=[OUT, TMP, F(speed)]); myv = rv(OUT)
    wv(TMP + 0x40, myv)
    for k in range(0x120 // 4): m.w32(CTX + 4 * k, 0)
    m.w32(CTX + 0x11c, CR)
    init = False
    for k, a in enumerate(ob_addr):
        o = [float(x) for x in obs[k][:8]]
        dist2 = f32(f32(f32((o[0]-px)**2) + f32((o[1]-py)**2)) + f32((o[2]-pz)**2))
        dist = 0.0 if dist2 <= 0 else math.sqrt(dist2)
        if (dist - rad) - o[7] > R: continue
        m.call(0x424860, args=[OUT, CR, TMP + 0x40, a]); t, m2, dd = rv(OUT)
        if t <= 0 or t > 4.0: continue
        if not init:
            l2 = dx*dx + dy*dy + dz*dz; inv = 1.0 / math.sqrt(l2)
            wv(TMP + 0x80, [dx * inv, dy * inv, dz * inv])
            m.call(0x424ba0, ecx=CTX, args=[CR + 0x80, TMP + 0x80]); init = True
        m.call(0x4250e0, ecx=CTX, args=[a])
    if init:
        pl2 = prx*prx + prz*prz; inv = 1.0 / math.sqrt(pl2) if pl2 > 0 else float('inf')
        pv = [f32(prx * inv), f32(0.0 * inv), f32(prz * inv)]
        wv(TMP + 0xc0, pv); m.call(0x424b50, ecx=CTX, args=[TMP + 0xc0])
        m.call(0x425200, ecx=CTX, args=[OUT]); got = rv(OUT)
    else: got = d
    err = max(abs(a - b) / (1 + abs(b)) for a, b in zip(got, ref)); worst = max(worst, err); scenes += 1
    if err > 1e-4: print('Abweichung', err, 'C++', ref, 'Original', got, 'init', init)
print('Avoid max rel. Abweichung', worst, 'Szenen', scenes)
