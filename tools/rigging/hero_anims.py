import numpy as np, math
from rigtool import *

def S(t,T): return math.sin(2*math.pi*t/T)
def C(t,T): return math.cos(2*math.pi*t/T)

def idle(t,T=2.4):
    ph=2*math.pi*t/T
    return P(rot={'spine':(1.2*math.sin(ph),0,0),'chest':(1.0*math.sin(ph+0.4),0.8*math.sin(ph*0.5),0),
                  'head':(-1.2*math.sin(ph+0.8),1.5*math.sin(ph*0.5),0),
                  'shoulder_nx':(0,0,-2.0*math.sin(ph+0.3)),'shoulder_px':(0,0,2.0*math.sin(ph+0.3)),
                  'elbow_nx':(-4+1.5*math.sin(ph),0,0),'elbow_px':(-6+1.5*math.sin(ph),0,0)},
             trans={'hips':(0,0.005*math.sin(ph),0)},
             scale={'chest':1.0+0.012*math.sin(ph)})

def walk(t,T=0.75):
    ph=2*math.pi*t/T
    s=math.sin(ph); c=math.cos(ph)
    def knee(sign):  # flex while the leg swings forward
        return 38*max(0.0,sign*c)**1.2+6
    thigh_px=-30*s; thigh_nx=30*s
    shin_px=knee(1); shin_nx=knee(-1)
    return P(rot={'hips':(0,5*s,2*c*0),'spine':(2,-4*s,1.5*s),'chest':(0,-3*s,0),'head':(0,2*s,0),
                  'thigh_px':(thigh_px,0,0),'thigh_nx':(thigh_nx,0,0),
                  'shin_px':(shin_px,0,0),'shin_nx':(shin_nx,0,0),
                  'foot_px':(-0.35*(thigh_px+shin_px),0,0),'foot_nx':(-0.35*(thigh_nx+shin_nx),0,0),
                  'shoulder_nx':(-18*s,0,-3),'shoulder_px':(8*s,0,3),
                  'elbow_nx':(-12-8*max(0,s),0,0),'elbow_px':(-20,0,0)},
             trans={'hips':(0.012*s,-0.018*abs(c)+0.01,0)})

# ---- attacks: the mace arm is *_px.  rotation conventions verified in preview ----
def attack_1(t):  # downward chop, 0.36s, impact ~0.10s
    K=[(0.00,P()),
       (0.05,P(rot={'shoulder_px':(-140,0,10),'elbow_px':(-45,0,0),'chest':(-6,-22,0),'spine':(-8,0,0),'hips':(0,-10,0),'head':(-4,8,0)},
                trans={'hips':(0,0.02,-0.03)})),
       (0.11,P(rot={'shoulder_px':(-38,0,0),'elbow_px':(-12,0,0),'chest':(10,24,0),'spine':(16,0,0),'hips':(0,12,0),'head':(6,-8,0),
                    'thigh_px':(-18,0,0),'shin_px':(14,0,0),'thigh_nx':(12,0,0)},
                trans={'hips':(0,-0.02,0.12)})),
       (0.20,P(rot={'shoulder_px':(-30,0,0),'elbow_px':(-18,0,0),'chest':(8,18,0),'spine':(12,0,0),'hips':(0,8,0),
                    'thigh_px':(-14,0,0),'shin_px':(10,0,0)},trans={'hips':(0,-0.015,0.09)})),
       (0.36,P())]
    return keyed(t,K)

def attack_2(t):  # horizontal sweep, 0.38s
    K=[(0.00,P()),
       (0.06,P(rot={'shoulder_px':(0,40,100),'elbow_px':(-20,0,0),'chest':(0,-34,0),'spine':(4,0,0),'hips':(0,-14,0)},trans={'hips':(0,0,-0.03)})),
       (0.13,P(rot={'shoulder_px':(0,-75,95),'elbow_px':(-8,0,0),'chest':(2,38,0),'spine':(6,0,0),'hips':(0,18,0),'head':(0,-14,0),
                    'thigh_nx':(-16,0,0),'thigh_px':(14,0,0),'shin_nx':(14,0,0)},trans={'hips':(0,-0.01,0.08)})),
       (0.24,P(rot={'shoulder_px':(0,-85,85),'elbow_px':(-10,0,0),'chest':(0,32,0),'hips':(0,14,0)},trans={'hips':(0,0,0.06)})),
       (0.38,P())]
    return keyed(t,K)

def attack_3(t):  # heavy overhead slam with lunge, 0.50s
    K=[(0.00,P()),
       (0.10,P(rot={'shoulder_px':(-160,0,12),'elbow_px':(-55,0,0),'shoulder_nx':(-30,0,-10),'chest':(-14,-12,0),'spine':(-12,0,0),'head':(-8,0,0),
                    'thigh_px':(8,0,0),'thigh_nx':(-10,0,0)},trans={'hips':(0,0.05,-0.06)})),
       (0.17,P(rot={'shoulder_px':(-28,0,0),'elbow_px':(-8,0,0),'shoulder_nx':(-40,0,-6),'chest':(16,10,0),'spine':(26,0,0),'head':(10,0,0),
                    'thigh_px':(-38,0,0),'shin_px':(32,0,0),'thigh_nx':(14,0,0),'shin_nx':(10,0,0),'foot_px':(8,0,0)},
                trans={'hips':(0,-0.10,0.22)})),
       (0.30,P(rot={'shoulder_px':(-24,0,0),'elbow_px':(-10,0,0),'chest':(14,8,0),'spine':(22,0,0),'thigh_px':(-34,0,0),'shin_px':(28,0,0),'thigh_nx':(12,0,0)},
                trans={'hips':(0,-0.09,0.20)})),
       (0.50,P())]
    return keyed(t,K)

def dash(t,T=0.2):
    return P(rot={'spine':(24,0,0),'chest':(10,0,0),'head':(-14,0,0),
                  'shoulder_px':(34,0,8),'shoulder_nx':(40,0,-8),'elbow_px':(-30,0,0),'elbow_nx':(-30,0,0),
                  'thigh_px':(-48+6*S(t,T),0,0),'shin_px':(52,0,0),'thigh_nx':(34,0,0),'shin_nx':(26,0,0),'foot_nx':(-14,0,0)},
             trans={'hips':(0,-0.08,0.05)})

def throw(t):  # stone throw with the free arm (nx), release ~0.12s, 0.38s total
    K=[(0.00,P()),
       (0.07,P(rot={'shoulder_nx':(70,0,-35),'elbow_nx':(-80,0,0),'chest':(0,-26,0),'spine':(-4,0,0),'hips':(0,-8,0),'shoulder_px':(-20,0,10)})),
       (0.13,P(rot={'shoulder_nx':(-105,0,-10),'elbow_nx':(-10,0,0),'chest':(4,30,0),'spine':(8,0,0),'hips':(0,12,0),'head':(0,-10,0),
                    'thigh_nx':(14,0,0),'thigh_px':(-16,0,0),'shin_px':(12,0,0)},trans={'hips':(0,-0.01,0.05)})),
       (0.24,P(rot={'shoulder_nx':(-85,0,-6),'elbow_nx':(-14,0,0),'chest':(3,22,0),'hips':(0,8,0)},trans={'hips':(0,0,0.03)})),
       (0.38,P())]
    return keyed(t,K)

def hit(t):
    K=[(0.00,P()),
       (0.06,P(rot={'spine':(-14,0,0),'chest':(-6,0,0),'head':(-16,0,6),'shoulder_nx':(0,0,-16),'shoulder_px':(0,0,16),'thigh_px':(-10,0,0),'thigh_nx':(8,0,0)},
                trans={'hips':(0,-0.02,-0.06)})),
       (0.28,P())]
    return keyed(t,K)

def stun(t,T=0.9):
    s=math.sin(2*math.pi*t/T)
    return P(rot={'spine':(14,4*s,0),'chest':(6,-3*s,0),'head':(18,10*s,5*s),'hips':(0,0,2*s),
                  'shoulder_px':(10,0,6),'shoulder_nx':(10,0,-6),'elbow_px':(-18,0,0),'elbow_nx':(-18,0,0),
                  'thigh_px':(-8,0,0),'shin_px':(14,0,0),'thigh_nx':(-8,0,0),'shin_nx':(14,0,0)},
             trans={'hips':(0.015*s,-0.05,0)})

def death(t):  # 1.3s, topples backwards and stays down
    K=[(0.00,P()),
       (0.25,P(rot={'spine':(-10,0,0),'head':(-14,0,0),'thigh_px':(-20,0,0),'shin_px':(30,0,0),'thigh_nx':(-14,0,0),'shin_nx':(26,0,0),
                    'shoulder_px':(18,0,10),'shoulder_nx':(18,0,-10)},trans={'hips':(0,-0.12,-0.04)})),
       (0.70,P(rot={'hips':(-55,0,0),'spine':(-8,0,0),'head':(-10,0,0),'thigh_px':(-48,0,0),'shin_px':(70,0,0),'thigh_nx':(-30,0,0),'shin_nx':(58,0,0),
                    'shoulder_px':(20,0,24),'shoulder_nx':(24,0,-24)},trans={'hips':(0,-0.40,-0.30)})),
       (1.30,P(rot={'hips':(-86,0,0),'spine':(-4,0,0),'head':(-6,0,0),'thigh_px':(-8,0,0),'shin_px':(8,0,0),'thigh_nx':(-6,0,0),'shin_nx':(10,0,0),
                    'shoulder_px':(10,0,32),'shoulder_nx':(14,0,-32),'elbow_px':(-10,0,0),'elbow_nx':(-12,0,0)},trans={'hips':(0,-0.74,-0.47)}))]
    return keyed(t,K)

ANIMS={
 'idle':(2.4,idle,True),'walk':(0.75,walk,True),
 'attack_1':(0.36,attack_1,False),'attack_2':(0.38,attack_2,False),'attack_3':(0.50,attack_3,False),
 'dash':(0.2,dash,True),'throw':(0.38,throw,False),'hit':(0.28,hit,False),'stun':(0.9,stun,True),'death':(1.3,death,False),
}
