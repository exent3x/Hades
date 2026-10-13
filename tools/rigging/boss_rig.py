import numpy as np
from rigtool import *
XC=-0.22
def bone(name,parent,joint,segs,r,k=4.0): return dict(name=name,parent=parent,joint=joint,segs=segs,r=r,k=k)
def make_skeleton():
    B=[]
    B.append(bone('root',None,(XC,-0.47,0.0),[((XC,-0.52,0),(XC,-0.38,0))],0.20,4))
    B.append(bone('body','root',(XC,-0.35,0.0),[((XC,-0.35,0),(XC,-0.05,0))],0.36,5))
    B.append(bone('chest','body',(XC,-0.05,0.0),[((XC,-0.05,0),(XC,0.22,0))],0.38,5))
    B.append(bone('head','chest',(XC,0.24,0.04),[((XC,0.22,0.04),(XC,0.50,0.04))],0.33,6))
    B.append(bone('shoulder_nx','chest',(-0.42,0.06,0.0),[((-0.42,0.06,0),(-0.52,-0.12,0))],0.13,5))
    B.append(bone('forearm_nx','shoulder_nx',(-0.52,-0.12,0.0),[((-0.52,-0.12,0),(-0.52,-0.36,0.02))],0.12,5))
    B.append(bone('shoulder_px','chest',(-0.03,0.02,0.0),[((-0.03,0.02,0),(0.28,-0.02,0))],0.14,5))
    B.append(bone('forearm_px','shoulder_px',(0.28,-0.02,0.0),[((0.28,-0.02,0),(0.55,-0.14,0))],0.14,5))
    B.append(bone('hand_px','forearm_px',(0.55,-0.14,0.0),[((0.55,-0.14,0),(0.82,-0.36,0))],0.14,5))
    return Skeleton(B)

def spruce_mask(P,col):
    x,y=P[:,0],P[:,1]; c=col.astype(float)
    green=(c[:,1]>c[:,0]+8)&(c[:,1]>c[:,2]+8)
    return ((x>0.34)&(y>-0.015))|((x>0.40)&(y>-0.12)&green&(y>-0.02-0.0))|((x>0.36)&(y>-0.06)&green)

def masks_and_rigid(P,col):
    x,y,z=P[:,0],P[:,1],P[:,2]; dx=x-XC
    m={}
    # trunk must not grab arms sideways
    m['body']=smoothstep(-0.43,-0.33,y)*(1-smoothstep(0.30,0.42,np.abs(dx))*smoothstep(0.1,-0.1,y))
    m['chest']=(1-smoothstep(0.30,0.42,np.abs(dx))*smoothstep(0.12,-0.12,y))
    side_nx=smoothstep(0.0,-0.18,dx); side_px=smoothstep(0.05,0.30,dx)
    for b in ('shoulder','forearm'):
        m[b+'_nx']=side_nx*smoothstep(0.12,0.24,np.abs(dx))
    m['shoulder_px']=side_px; m['forearm_px']=side_px; m['hand_px']=side_px
    sp=spruce_mask(P,col)
    rigid={'hand_px':sp,'root':(y<-0.43)&~sp}
    return m,rigid,sp
