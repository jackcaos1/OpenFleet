#!/usr/bin/env python3
"""Vergleicht HoverSimulate (C++, tools/hover_dump) mit dem Original (Armada.exe @0x459440, Unicorn)."""
import sys, struct, math, subprocess
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import exe_run as r

lines = subprocess.run(['/tmp/hover_dump'], capture_output=True, text=True).stdout.split('\n')
K = {}; steps = []
for ln in lines:
    p = ln.split()
    if not p: continue
    if p[0] == 'K': K[p[1]] = float(p[2])
    elif p[0] == 'S': S0 = [float(x) for x in p[1:]]
    elif p[0] == 'C': steps.append([[float(x) for x in p[1:]], None])
    elif p[0] == 'R': steps[-1][1] = [float(x) for x in p[1:]]

m = r.Machine('originalFiles/Armada.exe')
O = m.heap; ctl = O + 0x2000; cls = O + 0x3000; lst = O + 0x4000; mat = O + 0x5000
m.w32(O, 0x5cbfec); m.w32(O + 0xe0, cls); m.w32(O + 0x494, ctl); m.w32(O + 0xd0, lst); m.w32(lst, lst)
m.uc.mem_write(O + 0x518, b'\x01')
for k, off in [('k6a8',0x6a8),('k6ac',0x6ac),('k6b4',0x6b4),('k6b8',0x6b8),('k6c0',0x6c0),('k6c8',0x6c8),('k6cc',0x6cc),('k6d4',0x6d4),
               ('k6dc',0x6dc),('k6f0',0x6f0),('k6fc',0x6fc),('k700',0x700),('k708',0x708),('k710',0x710),('k718',0x718)]:
    m.wf(O + off, K[k])
m.wf(O + 0x4bc, 1.0); m.w32(O + 0x4c0, 0x501502f9)
px, py, pz, pitch, yaw, roll, vx, vz = S0
# Matrix aus Euler (wie 0x55d080) - wir rufen die Originalfunktion
def build(out):
    m.call(0x55d080, args=[out] + [struct.unpack('<I', struct.pack('<f', v))[0] for v in (pitch, yaw, roll, px, py, pz)])
m.w32(O, 0x5cbfec)
build(mat)
for i in range(12): m.w32(O + 0x68 + 4 * i, m.r32(mat + 4 * i))
m.wf(O + 0x768, pitch); m.wf(O + 0x76c, yaw); m.wf(O + 0x770, roll)
m.wf(O + 0x140, vx); m.wf(O + 0x148, vz); m.wf(O + 0x138, math.sqrt(vx*vx+vz*vz))
# Hooks
def ret_cdecl(mm):
    esp = mm.reg(r.UC_X86_REG_ESP); ra = mm.r32(esp)
    mm.uc.reg_write(r.UC_X86_REG_ESP, esp + 4); mm.uc.reg_write(r.UC_X86_REG_EIP, ra)
for a in (0x4ff960, 0x4ff9a0, 0x4ffb40): m.hook(a, ret_cdecl)
def ceil_hook(mm):
    esp = mm.reg(r.UC_X86_REG_ESP)
    v = struct.unpack('<d', bytes(mm.uc.mem_read(esp, 8)))[0]
    mm.uc.reg_write(r.UC_X86_REG_EAX, int(math.ceil(v))); mm.uc.reg_write(r.UC_X86_REG_ESP, esp + 8); mm.uc.reg_write(r.UC_X86_REG_EIP, 0x459552)
m.hook(0x459544, ceil_hook)
def setmat(mm):
    p = mm.arg(0); this = mm.reg(r.UC_X86_REG_ECX)
    for i in range(12): mm.w32(this + 0x68 + 4 * i, mm.r32(p + 4 * i))
    mm.ret_stdcall(1)
m.hook(0x4718e0, setmat)
# asin-Thunk (0x5bbb9a, CRT-Import) durch x87-Code ersetzen: asin(x)=atan2(x,sqrt(1-x^2))
_asin = O + 0x7000
m.uc.mem_write(_asin, bytes.fromhex('d9c0d8c8d9e8dee1d9fad9f3c3'))
m.uc.mem_write(0x5bbb9a, b'\xe9' + struct.pack('<i', _asin - (0x5bbb9a + 5)))

m.hook(0x465cc0, lambda mm: mm.ret_stdcall(1))
maxerr = 0
for i, (c, ref) in enumerate(steps):
    m.wf(ctl, c[0]); m.wf(ctl + 4, c[1]); m.wf(ctl + 8, c[2]); m.wf(ctl + 0xc, c[3])
    m.call(0x459440, ecx=O, args=[struct.unpack('<I', struct.pack('<f', c[4]))[0]])
    got = [m.rf(O + 0x8c), m.rf(O + 0x90), m.rf(O + 0x94), m.rf(O + 0x140), m.rf(O + 0x144), m.rf(O + 0x148), m.rf(O + 0x768), m.rf(O + 0x76c), m.rf(O + 0x770), m.rf(O + 0x14c), m.rf(O + 0x150), m.rf(O + 0x154)]
    err = max(abs(a - b) / (1 + abs(b)) for a, b in zip(got, ref))
    maxerr = max(maxerr, err)
    if i<3: print(i,[round(a-b,6) for a,b in zip(got,ref)])
    if err > 1e-3 and i < 0: print('step', i, 'Abweichung', err, '\n orig', got, '\n cpp ', ref); break
print('max rel. Abweichung', maxerr, 'Schritte', len(steps))

# Servo-Abgleich
vs = []; ws = []
for ln in lines:
    p = ln.split()
    if p and p[0] == 'V': vs.append([float(x) for x in p[1:]])
    if p and p[0] == 'W': ws.append([float(x) for x in p[1:]])
for k in ('k724','k728'): pass
F = lambda v: struct.unpack('<I', struct.pack('<f', v))[0]
# Instanz-Koeffizienten setzen (Klasse aus C++-Dump nicht ausgegeben -> aus Dump 'Q'-Zeilen)
Q = {}
for ln in lines:
    p = ln.split()
    if p and p[0] == 'Q': Q[p[1]] = float(p[2])
for off, key in [(0x724,'k724'),(0x728,'k728'),(0x72c,'k72c'),(0x730,'k730'),(0x734,'k734'),(0x738,'k738'),(0x73c,'k73c'),(0x740,'k740'),(0x744,'k744'),
                 (0x748,'k748'),(0x74c,'k74c'),(0x750,'k750'),(0x754,'k754'),(0x758,'k758'),(0x75c,'k75c'),(0x760,'k760'),(0x764,'k764'),(0x4b4,'k4b4'),(0x4b8,'k4b8')]:
    m.wf(O + off, Q[key])
m.wf(O + 0x4ac, Q['warp_ratio'])
worst = 0
for v, w in zip(vs, ws):
    px, py, pz, pitch, yaw, roll, vx, vy, vz, wx, wy, wz, arg, tx, ty, tz, tvx, tvz, wp = v
    build(mat)  # nur um Matrix aus Euler zu bekommen
    m.call(0x55d080, args=[mat] + [F(x) for x in (pitch, yaw, roll, px, py, pz)])
    for i in range(12): m.w32(O + 0x68 + 4 * i, m.r32(mat + 4 * i))
    for off, val in [(0x768,pitch),(0x76c,yaw),(0x770,roll),(0x140,vx),(0x144,vy),(0x148,vz),(0x14c,wx),(0x150,wy),(0x154,wz),(0x90,py),(0x138,math.sqrt(vx*vx+vy*vy+vz*vz))]:
        m.wf(O + off, val)
    outs = []
    m.wf(ctl, 0); m.wf(ctl+4, 0); m.wf(ctl+8, 0)
    m.call(0x458b80, ecx=O, args=[F(arg)]); outs.append(m.rf(ctl))
    m.call(0x458ca0, ecx=O, args=[F(arg*0.4)]); outs.append(m.rf(ctl+4))
    m.call(0x458d60, ecx=O, args=[F(arg*0.4)]); outs.append(m.rf(ctl+4)); outs.append(m.rf(ctl+8))
    m.call(0x458aa0, ecx=O, args=[F(arg*5)]); outs.append(m.rf(ctl+4))
    # PD: Zielpositionen als Vektoren im Speicher
    tp = O + 0x6000; tvp = O + 0x6020
    for i, x in enumerate((tx, ty, tz)): m.wf(tp + 4 * i, x)
    for i, x in enumerate((tvx, 0, tvz)): m.wf(tvp + 4 * i, x)
    for i, x in enumerate((px, py, pz)): m.wf(O + 0x12c + 4 * i, x)
    m.w32(O, O + 0x6100); m.w32(O + 0x6100 + 0xdc, 0x458990)
    m.call(0x4657b0, ecx=O, args=[tp, tvp, int(wp)]); outs.append(m.rf(ctl + 0xc))
    err = max(abs(a - b) / (1 + abs(b)) for a, b in zip(outs, w))
    worst = max(worst, err)
    if err > 1e-3: print('Servo-Abweichung', err, outs, w)
print('Servo max rel. Abweichung', worst, 'Faelle', len(vs))
