import sys,json,math
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from astar2 import *
import astar2
from proto import *
def leaves_merged(q):
    out=[]
    def rec(k,i,j):
        a=q.idx(k,i,j)
        if q.sub[k][a] and k<q.nl-1:
            kids=[];pos=[]
            for dj in (0,1):
                for di in (0,1):
                    ci,cj=2*i+di,2*j+dj
                    if ci<q.cols[k+1] and cj<q.rows[k+1]: kids.append(rec(k+1,ci,cj)); pos.append((ci,cj))
            if len(kids)==4 and all(isinstance(x,int) for x in kids) and len(set(kids))==1: return kids[0]
            for (ci,cj),x in zip(pos,kids):
                if isinstance(x,int): out.append((k+1,ci,cj,x,ci*q.sizes[k+1],(ci+1)*q.sizes[k+1],cj*q.sizes[k+1],(cj+1)*q.sizes[k+1]))
            return [1]
        return q.cost[k][a]
    r=rec(0,0,0)
    if isinstance(r,int): out.append((0,0,0,r,0,q.sizes[0],0,q.sizes[0]))
    return out
import astar
astar.leaves=leaves_merged; astar2.build.__globals__['leaves']=leaves_merged
def gate(A,B):
    ox0=max(A[4],B[4]); ox1=min(A[5],B[5]); oz0=max(A[6],B[6]); oz1=min(A[7],B[7])
    if ox0==ox1 and oz0<oz1: return ((ox0,oz0),(ox0,oz1))
    if oz0==oz1 and ox0<ox1: return ((ox0,oz0),(ox1,oz0))
    return None
def funnel(L,cells,s,g):
    gates=[gate(L[cells[i]],L[cells[i+1]]) for i in range(len(cells)-1)]
    layers=[[s]]+[[a,b] for a,b in gates]+[[g]]
    # Dijkstra ueber Schichten (Gewicht = Strecke)
    best=[[0.0]];par=[[None]]
    for t in range(1,len(layers)):
        bt=[];pt=[]
        for p in layers[t]:
            cand=[(best[t-1][k]+math.dist(layers[t-1][k],p),k) for k in range(len(layers[t-1]))]
            c,k=min(cand); bt.append(c); pt.append(k)
        best.append(bt);par.append(pt)
    k=0;path=[g];t=len(layers)-1
    while t>0:
        k=par[t][0] if t==len(layers)-1 else k
        # rueckwaerts
        break
    # sauberer Rueckwaertslauf
    pts=[];k=0
    for t in range(len(layers)-1,0,-1):
        pts.append(layers[t][k]); k=par[t][k]
    pts.append(s); pts.reverse(); return pts
def collinear_cut(pts):
    out=[pts[0]]
    for i in range(1,len(pts)-1):
        a=out[-1];b=pts[i];c=pts[i+1]
        cr=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
        if abs(cr)>1e-6: out.append(b)
    out.append(pts[-1]); return out
if __name__=='__main__':
    orc=json.load(open('/tmp/oracle_rub.json'))
    for sc in orc:
        q=newq()
        for (x,z) in sc['fields']: proto_world(q,x,z,x,z,75.0,IMP)
        L,ctr,adj=build(q)
        s=(sc['start'][0]+2000.,sc['start'][1]+2000.); g=(sc['goal'][0]+2000.,sc['goal'][1]+2000.)
        p=astar2.search(L,ctr,adj,s,g)
        pts=funnel(L,p,s,g)
        out=[(round(a-2000),round(b-2000)) for a,b in collinear_cut(pts)]
        want=[(round(a),round(b)) for a,b in sc['pts']]
        print('OK ' if out==want else 'DIF', sc['start'],sc['goal'],'\n   got ',out,'\n   want',want)
