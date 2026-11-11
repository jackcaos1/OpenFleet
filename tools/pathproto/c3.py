from cmp import *
import random
def proto_world(q,x0,z0,x1,z1,r,cost,flag=1,minx=-1500.,minz=-1500.,W=4000.,H=4000.):
    ax=x0-r-minx+500.; az=z0-r-minz+500.; bx=x1+r-minx+500.; bz=z1+r-minz+500.
    ax=0. if ax<0 else (W if ax>=W else ax); az=0. if az<0 else (H if az>=H else az)
    bx=0. if bx<0 else (W if bx>=W else bx); bz=0. if bz<0 else (H if bz>=H else bz)
    q.stamp(ax,az,bx,bz,cost,flag)
def newq():
    q=QG(4000,4000,50.0); B=550.0
    q.stamp(0,0,4000,B,IMP,1); q.stamp(0,4000-B,4000,4000,IMP,1); q.stamp(0,0,B,4000,IMP,1); q.stamp(4000-B,0,4000,4000,IMP,1)
    return q
import sys
random.seed(int(sys.argv[1]) if len(sys.argv)>1 else 1)
for t in range(5):
    m,p=setup(); q=newq(); ops=[]
    for n in range(random.randint(1,3)):
        x=random.uniform(-1300,1300); z=random.uniform(-1300,1300); r=random.choice([75.0,40.0,115.0,200.0]); c=random.choice([IMP,300,201])
        ops.append((round(x),round(z),r,c-(1<<32) if c==IMP else c))
        rr=fbits(r); cp=m.alloc(4); m.w32(cp,c)
        m.call(0x409900,ecx=p,args=[fbits(x),fbits(z),fbits(x),fbits(z),rr,rr,cp])
        proto_world(q,x,z,x,z,r,c)
    compare(q,orig_state(m,p),'t%d %s'%(t,ops))
