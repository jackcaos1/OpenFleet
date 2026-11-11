import sys,os,struct; sys.path.insert(0,os.path.expanduser('~/mnt/Open Armada/tools')); sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from path_run import *
from dumpq2 import setup,dump
from proto import *
def orig_state(m,p):
    d=dump(m,p); res=[]
    for rows,cols,s,data in d:
        cs=[struct.unpack_from('<7I',data,28*i) for i in range(rows*cols)]
        res.append(([c[0] for c in cs],[c[6]&1 for c in cs]))
    return res
def compare(q,st,label):
    bad=0
    for k in range(8):
        cs,sb=st[k]
        for a in range(len(cs)):
            if cs[a]!=q.cost[k][a] or sb[a]!=q.sub[k][a]:
                if bad<8: print(label,'diff level',k,'cell',a%q.cols[k],a//q.cols[k],'orig cost',s32(cs[a]),'sub',sb[a],'proto',s32(q.cost[k][a]),q.sub[k][a])
                bad+=1
    print(label,'diffs',bad)
if __name__=='__main__':
    m,p=setup()
    st=orig_state(m,p)   # nach ctor
    q=QG(4000,4000,50.0)
    compare(q,st,'ctor(ohne Rand)')
    for flag in (0,1):
        q=QG(4000,4000,50.0)
        B=550.0
        q.stamp(0,0,4000,B,IMP,flag); q.stamp(0,4000-B,4000,4000,IMP,flag); q.stamp(0,0,B,4000,IMP,flag); q.stamp(4000-B,0,4000,4000,IMP,flag)
        compare(q,st,'ctor mit Rand flag=%d'%flag)
