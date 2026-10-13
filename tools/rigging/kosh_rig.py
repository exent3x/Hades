import numpy as np
from rigtool import *
def bone(name,parent,joint,segs,r,k=4.0): return dict(name=name,parent=parent,joint=joint,segs=segs,r=r,k=k)
def make_skeleton():
    B=[]
    B.append(bone('hips',None,(-0.02,0.02,-0.18),[((-0.02,-0.04,-0.18),(-0.02,0.10,-0.18))],0.22,4))
    B.append(bone('spine','hips',(0.0,0.12,-0.18),[((0.0,0.12,-0.18),(0.02,0.30,-0.15))],0.22,4))
    B.append(bone('chest','spine',(0.02,0.30,-0.15),[((0.02,0.30,-0.15),(0.04,0.50,-0.10))],0.26,4))
    B.append(bone('neck','chest',(0.08,0.52,-0.03),[((0.08,0.52,-0.03),(0.11,0.60,0.0))],0.10,6))
    B.append(bone('head','neck',(0.11,0.60,0.0),[((0.11,0.60,0.0),(0.13,0.72,0.06))],0.17,6))
    B.append(bone('shoulder_nx','chest',(-0.22,0.48,-0.08),[((-0.22,0.48,-0.08),(-0.46,0.36,0.0))],0.11,5))
    B.append(bone('elbow_nx','shoulder_nx',(-0.46,0.36,0.0),[((-0.46,0.36,0.0),(-0.64,0.27,0.12))],0.09,5))
    B.append(bone('hand_nx','elbow_nx',(-0.64,0.27,0.14),[((-0.64,0.27,0.14),(-0.75,0.10,0.2))],0.12,5))
    B.append(bone('shoulder_px','chest',(0.25,0.44,-0.10),[((0.25,0.44,-0.10),(0.40,0.16,0.0))],0.11,5))
    B.append(bone('elbow_px','shoulder_px',(0.40,0.16,0.0),[((0.40,0.16,0.0),(0.52,0.30,0.06))],0.09,5))
    B.append(bone('hand_px','elbow_px',(0.52,0.30,0.06),[((0.52,0.30,0.06),(0.55,0.34,0.10))],0.12,5))
    B.append(bone('thigh_nx','hips',(-0.14,0.0,-0.18),[((-0.14,0.0,-0.18),(-0.30,-0.22,-0.10))],0.13,5))
    B.append(bone('shin_nx','thigh_nx',(-0.30,-0.22,-0.10),[((-0.30,-0.22,-0.10),(-0.42,-0.50,-0.05))],0.11,5))
    B.append(bone('foot_nx','shin_nx',(-0.42,-0.50,-0.05),[((-0.45,-0.55,-0.12),(-0.48,-0.68,0.0))],0.16,5))
    B.append(bone('thigh_px','hips',(0.14,0.0,-0.18),[((0.14,0.0,-0.18),(0.34,-0.22,-0.05))],0.13,5))
    B.append(bone('shin_px','thigh_px',(0.34,-0.22,-0.05),[((0.34,-0.22,-0.05),(0.45,-0.50,-0.10))],0.11,5))
    B.append(bone('foot_px','shin_px',(0.45,-0.50,-0.10),[((0.47,-0.55,-0.12),(0.52,-0.68,0.03))],0.16,5))
    return Skeleton(B)

def masks_and_rigid(P,col):
    x,y,z=P[:,0],P[:,1],P[:,2]; m={}
    side_nx=smoothstep(0.03,-0.07,x); side_px=1-side_nx
    below=smoothstep(0.02,-0.12,y)
    for s,sm in (('px',side_px),('nx',side_nx)):
        for b in ('thigh','shin','foot'): m[f'{b}_{s}']=sm*below
    arm_nx=side_nx*smoothstep(-0.12,-0.28,x)
    arm_px=side_px*smoothstep(0.18,0.30,x)
    for b in ('shoulder','elbow','hand'):
        m[f'{b}_nx']=arm_nx; m[f'{b}_px']=arm_px
    lateral=1-smoothstep(0.30,0.45,np.abs(x))*smoothstep(0.38,0.12,y)
    for b in ('hips','spine','chest'): m[b]=lateral
    bright=col.astype(float).mean(1)
    rigid={'hand_nx':(x<-0.70)|((x<-0.60)&(y>-0.2))|((x<-0.55)&(bright>95)),
           'hand_px':(x>0.42)&(y>0.22)&(y<0.46)}
    return m,rigid
