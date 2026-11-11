"""Zufallsszenarien gegen den ORIGINAL-Pfadfinder (Emulation). Aufruf: path_gen_rand.py SEED ANZAHL AUS.json"""
import sys,os,json,random; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import path_run
from path_run import *
seed=int(sys.argv[1]); cnt=int(sys.argv[2]); out=sys.argv[3]
orig=path_run.PathMachine.__init__
def init(self,*a,**k):
    orig(self,*a,**k); self.w32(0x5fb87c,1)
path_run.PathMachine.__init__=init
rnd=random.Random(seed); res=[]
for _ in range(cnt):
    nf=rnd.randint(1,int(os.environ.get("MAXF","4")))
    f=[(round(rnd.uniform(-1000,1000),1),round(rnd.uniform(-1000,1000),1)) for _ in range(nf)]
    def pt():
        while True:
            p=(round(rnd.uniform(-1300,1300),1),round(rnd.uniform(-1300,1300),1))
            if all(max(abs(p[0]-a),abs(p[1]-b))>130 for a,b in f): return p
    s=pt(); g=pt()
    m,path,pts=run_scenario(f,(s[0],30.,s[1]),(g[0],30.,g[1]),soft=bool(int(os.environ.get('SOFT','0'))))
    res.append(dict(fields=f,start=s,goal=g,pts=[(a,c) for a,b,c in pts]))
    print(f,s,g,len(pts),flush=True)
json.dump(res,open(out,'w'))
