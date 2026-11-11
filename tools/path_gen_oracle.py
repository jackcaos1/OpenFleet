import sys,os,json; sys.path.insert(0,os.path.expanduser('~/mnt/Open Armada/tools'))
import path_run
from path_run import *
rub=int(sys.argv[1]); out=sys.argv[2]
orig=path_run.PathMachine.__init__
def init(self,*a,**k):
    orig(self,*a,**k); self.w32(0x5fb87c,rub)
path_run.PathMachine.__init__=init
sc=[([(0,0)],(-600,0),(600,0)),([(0,0)],(-600,-300),(600,300)),([(0,0)],(-100,0),(100,0)),([(0,0)],(-600,200),(600,200)),
    ([(0,0)],(0,-600),(0,600)),([(0,0),(300,100)],(-600,0),(700,0)),([(0,0),(0,300)],(-400,150),(400,150)),
    ([(100,-200),(-300,200)],(-700,-700),(700,700)),([(0,0)],(-300,-300),(300,300)),([(-200,0),(200,0)],(0,-500),(0,500))]
res=[]
for f,s,g in sc:
    m,path,pts=run_scenario(f,(s[0],30.,s[1]),(g[0],30.,g[1]))
    res.append(dict(fields=f,start=s,goal=g,pts=[(a,c) for a,b,c in pts]))
    print(f,s,g,[(round(a),round(c)) for a,b,c in pts],flush=True)
json.dump(res,open(out,'w'))
