import math
IMP=0xffffffff
def s32(v): return v-(1<<32) if v&0x80000000 else v
class QG:
    def __init__(s, W, H, res, default=100):
        s.default=default; s.margin=0.0
        s.nl=8
        s.sizes=[res*2**(7-k) for k in range(8)]
        s.cols=[int(math.ceil(W/z)) for z in s.sizes]; s.rows=[int(math.ceil(H/z)) for z in s.sizes]
        s.cost=[[default]*(s.cols[k]*s.rows[k]) for k in range(8)]
        s.sub=[[0]*(s.cols[k]*s.rows[k]) for k in range(8)]
    def idx(s,k,i,j): return j*s.cols[k]+i
    def want(s, old, new, flag):
        # Ueberschreibregel (0x40c831..0x40c87a)
        o=s32(old); n=s32(new)
        if o < n and old!=IMP: return True
        if new==IMP: return True
        if old==s.default or not flag: return True
        return False
    def stamp(s,x0,z0,x1,z1,cost,flag=1,k=0,i=0,j=0):
        if i<0 or j<0 or i>=s.cols[k] or j>=s.rows[k]: return
        sz=s.sizes[k]; cx0=i*sz; cz0=j*sz; cx1=cx0+sz; cz1=cz0+sz
        m=s.margin
        a=s.idx(k,i,j)
        full = not (x0-m>cx0 or x1+m<cx1 or z0-m>cz0 or z1+m<cz1)
        if full and not s.sub[k][a]:
            if s.want(s.cost[k][a],cost,flag): s.cost[k][a]=cost
            return
        # partiell (oder unterteilt)
        ix0=max(x0,cx0); iz0=max(z0,cz0); ix1=min(x1,cx1); iz1=min(z1,cz1)
        if not (ix0<=ix1 and iz0<=iz1): return
        if k==s.nl-1:
            if s.want(s.cost[k][a],cost,flag): s.cost[k][a]=cost
            return
        if not s.sub[k][a]:
            # nur unterteilen wenn ueberhaupt gestempelt werden wuerde? (spaeter pruefen)
            s.sub[k][a]=1
            for dj in (0,1):
                for di in (0,1):
                    ci=2*i+di; cj=2*j+dj
                    if ci<s.cols[k+1] and cj<s.rows[k+1]:
                        s.cost[k+1][s.idx(k+1,ci,cj)]=s.cost[k][a]
        for dj in (0,1):
            for di in (0,1):
                s.stamp(x0,z0,x1,z1,cost,flag,k+1,2*i+di,2*j+dj)
