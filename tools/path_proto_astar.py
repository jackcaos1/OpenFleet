import sys,os,json,math
sys.path.insert(0,'/tmp')
import numpy as np
from astar import leaves, find, build
from proto import *
from c3 import newq, proto_world
F=np.float32
class N: pass
def search(L,ctr,adj,s,g,edgemode='d100'):
    n=len(L)
    nodes=[N() for _ in range(n)]
    for k,x in enumerate(nodes): x.id=-1; x.closed=0; x.f=F(0); x.g=F(0); x.par=None; x.k=k
    S=find(L,*s); G=find(L,*g)
    heap=[]   # Elemente: Knotenindex; Vergleich ueber aktuelles f
    def ecost(a,b):
        d=math.dist(ctr[a],ctr[b])
        if edgemode=='d100': return F(d*100.0)
    def h(a): return F(math.dist(ctr[a],ctr[G])*100.0)
    def push(k):
        heap.append(k); idx=len(heap)-1; val=k
        while idx>0:
            par=(idx-1)//2
            if nodes[heap[par]].f > nodes[val].f:
                heap[idx]=heap[par]; idx=par
            else: break
        heap[idx]=val
    def pop():
        top=heap[0]; last=heap.pop()
        ln=len(heap)
        if ln==0: return top
        # _Adjust_heap(first,0,ln,value=last)  (heap[ln] war der letzte; jetzt ist heap bereits um 1 kuerzer)
        hole=0; child=2*hole+2; 
        while child<ln:
            if nodes[heap[child]].f > nodes[heap[child-1]].f: child-=1
            heap[hole]=heap[child]; hole=child; child=2*child+2
        if child==ln:
            heap[hole]=heap[ln-1]; hole=ln-1
        idx=hole
        while idx>0:
            par=(idx-1)//2
            if nodes[heap[par]].f > nodes[last].f:
                heap[idx]=heap[par]; idx=par
            else: break
        heap[idx]=last
        return top
    sid=1
    def relax(k,p,e):
        x=nodes[k]
        if x.id!=sid: x.id=sid; x.closed=0; x.f=F(0); x.g=F(0); x.par=None
        if x.closed:
            hh=x.f-x.g
            fc=F(nodes[p].g+e+hh)
            if x.f > fc:
                x.g=F(nodes[p].g+e); x.f=fc; x.par=p; push(k); x.closed=0
            return
        if x.f==0:
            x.g=F(nodes[p].g+e); x.f=F(x.g+h(k)); x.par=p; push(k)
    relax(S,S,F(0)); cur=S; nodes[S].closed=1
    def expand(c):
        for v in adj[c]:
            if L[v][3]==0xffffffff: continue
            relax(v,c,ecost(c,v))
    expand(cur)
    while cur!=G:
        if not heap: return None
        cur=pop(); nodes[cur].closed=1; expand(cur)
    p=[G]
    while p[-1]!=S: p.append(nodes[p[-1]].par)
    p.reverse(); return p
def run(oracle,verbose=True):
    ok=0
    for sc in oracle:
        q=newq()
        for (x,z) in sc['fields']: proto_world(q,x,z,x,z,75.0,IMP)
        L,ctr,adj=build(q)
        s=(sc['start'][0]+2000.,sc['start'][1]+2000.); g=(sc['goal'][0]+2000.,sc['goal'][1]+2000.)
        p=search(L,ctr,adj,s,g)
        pts=[sc['start']]+[(ctr[i][0]-2000.,ctr[i][1]-2000.) for i in p[1:-1]]+[sc['goal']]
        want=[tuple(x) for x in sc['pts']]
        good=len(pts)==len(want) and all(abs(a[0]-b[0])<1 and abs(a[1]-b[1])<1 for a,b in zip(pts,want))
        ok+=good
        if not good and verbose: print(' MISMATCH',sc['fields'],sc['start'],sc['goal'],'\n   got ',[(round(a),round(b)) for a,b in pts],'\n   want',[(round(a),round(b)) for a,b in want])
    print(ok,'/',len(oracle))
if __name__=='__main__':
    run(json.load(open('/tmp/oracle_base.json')))
