import sys,json,math,heapq
sys.path.insert(0,'/tmp')
from gates import *
def seg_hits(p,q,a,b,eps=1e-6):
    # Schneidet Strecke pq das (geschlossene) Tor ab?
    def cr(o,u,v): return (u[0]-o[0])*(v[1]-o[1])-(u[1]-o[1])*(v[0]-o[0])
    d1=cr(p,q,a); d2=cr(p,q,b)
    if d1>eps and d2>eps: return False
    if d1<-eps and d2<-eps: return False
    # a,b liegen auf verschiedenen Seiten (oder auf der Geraden) -> Gerade pq schneidet; pruefen ob im Bereich von pq
    d3=cr(a,b,p); d4=cr(a,b,q)
    if d3>eps and d4>eps: return False
    if d3<-eps and d4<-eps: return False
    return True
def visible_path(s,gates,g):
    nodes=[(s,-1)]
    for i,(a,b) in enumerate(gates):
        nodes.append((a,i)); nodes.append((b,i))
    nodes.append((g,len(gates)))
    n=len(nodes); dist=[1e18]*n; par=[-1]*n; dist[0]=0
    pq=[(0.0,0)]
    done=set()
    while pq:
        d,u=heapq.heappop(pq)
        if u in done: continue
        done.add(u)
        if u==n-1: break
        pu,iu=nodes[u]
        for v in range(1,n):
            pv,iv=nodes[v]
            if iv<=iu: continue
            if all(seg_hits(pu,pv,*gates[k]) for k in range(iu+1,iv)):
                nd=d+math.dist(pu,pv)
                if nd<dist[v]-1e-9: dist[v]=nd; par[v]=u; heapq.heappush(pq,(nd,v))
    p=[];u=n-1
    while u!=-1: p.append(nodes[u][0]); u=par[u]
    p.reverse(); return p
if __name__=='__main__':
    orc=json.load(open('/tmp/oracle_rub.json'))
    ok=0
    for sc in orc:
        q=newq()
        for (x,z) in sc['fields']: proto_world(q,x,z,x,z,75.0,IMP)
        L,ctr,adj=build(q)
        s=(sc['start'][0]+2000.,sc['start'][1]+2000.); g=(sc['goal'][0]+2000.,sc['goal'][1]+2000.)
        p=astar2.search(L,ctr,adj,s,g)
        gs=[gate(L[p[i]],L[p[i+1]]) for i in range(len(p)-1)]
        pts=visible_path(s,gs,g)
        out=[(round(a-2000),round(b-2000)) for a,b in collinear_cut(pts)]
        want=[(round(a),round(b)) for a,b in sc['pts']]
        ok+=out==want
        print('OK ' if out==want else 'DIF', sc['start'],sc['goal'],'' if out==want else '\n   got %s\n   want %s'%(out,want))
    print(ok,'/10')
