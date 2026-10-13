import numpy as np
from rigtool import *

XC=-0.15
def bone(name,parent,joint,segs,r,k=4.0): return dict(name=name,parent=parent,joint=joint,segs=segs,r=r,k=k)

def make_skeleton():
    B=[]
    B.append(bone('hips',None,(XC,-0.06,0.0),[((XC,-0.12,0),(XC,0.08,0))],0.20))
    B.append(bone('spine','hips',(XC,0.08,0.0),[((XC,0.08,0),(XC,0.30,0))],0.22))
    B.append(bone('chest','spine',(XC,0.30,0.0),[((XC,0.30,0),(XC,0.50,0))],0.24))
    B.append(bone('neck','chest',(XC,0.52,0.02),[((XC,0.52,0.02),(XC,0.63,0.03))],0.10,6))
    B.append(bone('head','neck',(XC,0.63,0.03),[((XC,0.63,0.03),(XC,0.88,0.05))],0.17,6))
    # +x side (mace arm)
    B.append(bone('shoulder_px','chest',(0.07,0.46,0.0),[((0.07,0.46,0),(0.18,0.20,0))],0.12,5))
    B.append(bone('elbow_px','shoulder_px',(0.18,0.20,0.0),[((0.18,0.20,0),(0.21,-0.06,0.02))],0.11,5))
    B.append(bone('hand_px','elbow_px',(0.20,-0.07,0.02),[((0.20,-0.07,0.02),(0.21,-0.14,0.03))],0.08,6))
    # -x side (free arm)
    B.append(bone('shoulder_nx','chest',(-0.37,0.46,0.0),[((-0.37,0.46,0),(-0.44,0.20,0))],0.12,5))
    B.append(bone('elbow_nx','shoulder_nx',(-0.44,0.20,0.0),[((-0.44,0.20,0),(-0.38,-0.06,0.04))],0.11,5))
    B.append(bone('hand_nx','elbow_nx',(-0.38,-0.07,0.04),[((-0.38,-0.07,0.04),(-0.37,-0.14,0.05))],0.08,6))
    # legs
    B.append(bone('thigh_px','hips',(0.00,-0.08,0.0),[((0.00,-0.08,0),(0.05,-0.50,0.01))],0.13,5))
    B.append(bone('shin_px','thigh_px',(0.05,-0.50,0.01),[((0.05,-0.50,0.01),(0.07,-0.85,0))],0.10,5))
    B.append(bone('foot_px','shin_px',(0.07,-0.85,0.0),[((0.07,-0.88,-0.06),(0.08,-0.93,0.18))],0.09,5))
    B.append(bone('thigh_nx','hips',(-0.30,-0.08,0.0),[((-0.30,-0.08,0),(-0.35,-0.50,0.01))],0.13,5))
    B.append(bone('shin_nx','thigh_nx',(-0.35,-0.50,0.01),[((-0.35,-0.50,0.01),(-0.33,-0.85,0))],0.10,5))
    B.append(bone('foot_nx','shin_nx',(-0.33,-0.85,0.0),[((-0.33,-0.88,-0.06),(-0.32,-0.93,0.18))],0.09,5))
    return Skeleton(B)

MACE_LINE=[(-0.03,0.34,-0.37),(0.10,0.09,-0.25),(0.19,-0.05,-0.08),(0.20,-0.07,0.0),(0.33,-0.52,0.15),(0.385,-0.63,0.20)]

def masks_and_rigid(P,col=None):
    x=P[:,0]; y=P[:,1]; z=P[:,2]
    dx=x-XC
    side_px=smoothstep(-0.03,0.05,dx); side_nx=1-side_px
    m={}
    # legs: only below the crotch region, on their own side
    below=smoothstep(0.02,-0.16,y)   # 1 below y=-0.16
    for s,sm in (('px',side_px),('nx',side_nx)):
        for b in ('thigh','shin','foot'):
            m[f'{b}_{s}']=sm*np.maximum(below,0.0)
    # arms: own side, and away from torso core when below shoulders
    away=smoothstep(0.13,0.23,np.abs(dx))
    shoulder_zone=smoothstep(0.38,0.50,y)
    for s,sm in (('px',side_px),('nx',side_nx)):
        arm=sm*np.maximum(away,shoulder_zone*0.0+ (shoulder_zone>0.5)*0.0)
        arm=arm*smoothstep(-0.26,-0.14,y)   # arms never grab the legs/pants next to the hands
        if col is not None:
            c=col.astype(float); skin=((c[:,0]>150)&(c[:,0]-c[:,2]>40)&(c[:,0]>c[:,1]+20)).astype(float)
            low=smoothstep(0.02,-0.04,y)      # 1 near/below the hands
            arm=arm*((1-low)+low*skin)
        for b in ('shoulder','elbow','hand'):
            m[f'{b}_{s}']=arm
    # torso bones must not grab the arms sideways below the shoulders
    lateral=1-smoothstep(0.24,0.34,np.abs(dx))*(1-smoothstep(0.40,0.50,y))
    for b in ('hips','spine','chest'): m[b]=lateral
    # mace: rigid to hand_px
    d=np.full(len(P),1e9)
    for a,b in zip(MACE_LINE[:-1],MACE_LINE[1:]): d=np.minimum(d,seg_dist(P,a,b))
    head_c=np.array(MACE_LINE[-1])
    dh=np.linalg.norm(P-head_c,axis=1)
    shaft=(d<0.045)&((y<-0.095)|(z<-0.09))
    head=(dh<0.21)&(y<-0.38)
    rigid={'hand_px':shaft|head}
    return m,rigid
