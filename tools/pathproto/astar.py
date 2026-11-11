import sys,os,json,math,heapq
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from proto import *
from c3 import newq, proto_world
def leaves(q):
    out=[]
    def rec(k,i,j):
        if i>=q.cols[k] or j>=q.rows[k]: return
        a=q.idx(k,i,j)
        if q.sub[k][a] and k<q.nl-1:
            for dj in (0,1):
                for di in (0,1): rec(k+1,2*i+di,2*j+dj)
        else:
            sz=q.sizes[k]; out.append((k,i,j,q.cost[k][a],i*sz,(i+1)*sz,j*sz,(j+1)*sz))
    rec(0,0,0); return out
def build(q,costmode='avg'):
    L=leaves(q); n=len(L)
    ctr=[((l[4]+l[5])/2,(l[6]+l[7])/2) for l in L]
    adj=[[] for _ in range(n)]
    # Nachbarschaft ueber geteilte Kante (O(n^2) reicht fuer Test)
    for a in range(n):
        for b in range(a+1,n):
            A=L[a];B=L[b]
            ox=min(A[5],B[5])-max(A[4],B[4]); oz=min(A[7],B[7])-max(A[6],B[6])
            if (ox==0 and oz>0) or (oz==0 and ox>0):
                adj[a].append(b); adj[b].append(a)
    return L,ctr,adj
def find(L,x,z):
    best=None
    for idx,l in enumerate(L):
        if l[4]<=x<l[5] and l[6]<=z<l[7]: best=idx
    return best
def search(L,ctr,adj,s,g,costmode,tie):
    S=find(L,*s); G=find(L,*g)
    def cost(a,b):
        d=math.dist(ctr[a],ctr[b]); ca=L[a][3]; cb=L[b][3]
        if costmode=='avg': return d*(ca+cb)/2
        if costmode=='dst': return d*cb
        if costmode=='src': return d*ca
    imp=0xffffffff
    gs={S:0.0}; par={}; cnt=0
    op=[(0.0,0,S)]; closed=set()
    while op:
        f,_,u=heapq.heappop(op)
        if u in closed: continue
        closed.add(u)
        if u==G: break
        for v in adj[u]:
            if L[v][3]==imp: continue
            ng=gs[u]+cost(u,v)
            if v not in gs or ng<gs[v]:
                gs[v]=ng; par[v]=u; cnt+=1
                h=math.dist(ctr[v],ctr[G])*100.0
                heapq.heappush(op,(ng+h, cnt if tie=='fifo' else -cnt, v))
    if G not in par and G!=S: return None
    p=[G]
    while p[-1]!=S: p.append(par[p[-1]])
    p.reverse()
    return p
def run(oracle,costmode,tie):
    ok=0
    for sc in oracle:
        q=newq()
        for (x,z) in sc['fields']: proto_world(q,x,z,x,z,75.0,IMP)
        L,ctr,adj=build(q)
        s=(sc['start'][0]+2000.,sc['start'][1]+2000.); g=(sc['goal'][0]+2000.,sc['goal'][1]+2000.)
        p=search(L,ctr,adj,s,g,costmode,tie)
        pts=[sc['start']]+[(ctr[i][0]-2000.,ctr[i][1]-2000.) for i in p[1:-1]]+[sc['goal']]
        want=[tuple(x) for x in sc['pts']]
        good=len(pts)==len(want) and all(abs(a[0]-b[0])<1 and abs(a[1]-b[1])<1 for a,b in zip(pts,want))
        ok+=good
        if not good: print(' MISMATCH',sc['fields'],sc['start'],sc['goal'],'\n   got ',[(round(a),round(b)) for a,b in pts],'\n   want',[(round(a),round(b)) for a,b in want])
    print(costmode,tie,ok,'/',len(oracle))
if __name__=='__main__':
    orc=json.load(open('/tmp/oracle_base.json'))
    for cm in ('avg','dst','src'):
        for tie in ('fifo',):
            run(orc,cm,tie)
