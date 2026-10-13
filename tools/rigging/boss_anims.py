import numpy as np, math
from rigtool import *

def idle(t,T=3.0):
    ph=2*math.pi*t/T; s=math.sin
    return P(rot={'body':(0.8*s(ph),0,0),'chest':(1.2*s(ph),0,0.8*s(ph*0.5)),'head':(-1.5*s(ph+0.6),3*s(ph*0.5),0),
                  'shoulder_px':(0,0,1.5*s(ph+0.4)),'forearm_px':(0,0,2.0*s(ph+0.9)),'hand_px':(0,0,2.5*s(ph+1.4)),
                  'shoulder_nx':(0,0,-1.5*s(ph+0.2)),'forearm_nx':(0,0,-1.5*s(ph+0.8))},
             trans={'chest':(0,0.004*s(ph),0)},scale={'chest':1.0+0.010*s(ph)})

def walk(t,T=1.3):
    ph=2*math.pi*t/T; s=math.sin(ph); c=math.cos(ph)
    return P(rot={'body':(2.0*abs(c),0,4.0*s),'chest':(0,-3*s,-3.0*s),'head':(0,-4*s,2*s),
                  'shoulder_nx':(10*s,0,-2),'forearm_nx':(0,0,-3*abs(s)),
                  'shoulder_px':(-5*s,0,2+3*math.sin(2*ph)),'forearm_px':(0,0,2*math.sin(2*ph+0.5)),'hand_px':(0,0,3*math.sin(2*ph+1.0))},
             trans={'body':(0.02*s,-0.012*abs(s),0)})

# --- timelines for attacks: [0 .. windup_end] is scrubbed by the C++ windup progress, the rest plays on execute ---
def melee(t):   # windup_end = 0.75
    K=[(0.00,P()),
       (0.45,P(rot={'shoulder_px':(0,25,70),'forearm_px':(0,0,15),'hand_px':(0,0,10),'chest':(-6,18,0),'body':(-3,0,0),'head':(0,-8,0)},trans={'chest':(0,0.03,0)})),
       (0.75,P(rot={'shoulder_px':(0,35,85),'forearm_px':(0,0,18),'hand_px':(0,0,12),'chest':(-10,24,0),'body':(-4,0,0),'head':(-4,-10,0),'shoulder_nx':(0,0,-20)},trans={'chest':(0,0.04,-0.02)})),
       (0.88,P(rot={'shoulder_px':(0,-70,-20),'forearm_px':(0,0,-10),'hand_px':(0,0,-8),'chest':(12,-26,0),'body':(6,0,0),'head':(8,12,0),'shoulder_nx':(0,0,10)},trans={'chest':(0,-0.03,0.09)})),
       (1.40,P())]
    return keyed(t,K)

def throw(t):   # windup_end = 0.90, release ~0.97
    K=[(0.00,P()),
       (0.45,P(rot={'shoulder_px':(0,45,60),'forearm_px':(0,0,12),'chest':(-8,22,0),'head':(0,-8,0)},trans={'chest':(0,0.02,0)})),
       (0.90,P(rot={'shoulder_px':(0,55,95),'forearm_px':(0,0,15),'hand_px':(0,0,10),'chest':(-12,28,0),'head':(-6,-10,0),'shoulder_nx':(0,0,-25)},trans={'chest':(0,0.04,-0.02)})),
       (1.00,P(rot={'shoulder_px':(0,-55,40),'forearm_px':(0,0,-8),'chest':(10,-24,0),'head':(6,10,0)},trans={'chest':(0,-0.02,0.07)})),
       (1.70,P())]
    return keyed(t,K)

def cast(t):    # windup_end = 0.70, hold, then lower
    up=P(rot={'shoulder_nx':(0,0,-80),'forearm_nx':(0,0,-14),'shoulder_px':(0,0,105),'forearm_px':(0,0,12),'hand_px':(0,0,10),
              'chest':(-10,0,0),'body':(-3,0,0),'head':(-16,0,0)},trans={'chest':(0,0.07,0)})
    K=[(0.00,P()),(0.70,up),(1.00,up),(1.60,P())]
    return keyed(t,K)

def slam(t):    # windup_end = 0.70 (arms up), slam down on execute
    up=P(rot={'shoulder_nx':(0,0,-85),'forearm_nx':(0,0,-14),'shoulder_px':(0,0,110),'forearm_px':(0,0,12),
              'chest':(-12,0,0),'body':(-4,0,0),'head':(-14,0,0)},trans={'chest':(0,0.07,0)})
    down=P(rot={'shoulder_nx':(0,0,-5),'shoulder_px':(0,0,-20),'forearm_px':(0,0,-10),'chest':(18,0,0),'body':(8,0,0),'head':(14,0,0)},trans={'chest':(0,-0.05,0.07)})
    K=[(0.00,P()),(0.70,up),(0.80,down),(1.30,P())]
    return keyed(t,K)

def grab(t):    # windup_end = 0.50
    wide=P(rot={'shoulder_nx':(-20,0,-85),'shoulder_px':(0,40,85),'chest':(-8,-12,0),'head':(-8,0,0)},trans={'chest':(0,0.03,-0.03)})
    clap=P(rot={'shoulder_nx':(-75,0,-10),'shoulder_px':(0,-80,25),'chest':(16,0,0),'body':(6,0,0),'head':(8,0,0)},trans={'chest':(0,-0.03,0.10)})
    K=[(0.00,P()),(0.50,wide),(0.62,clap),(1.20,P())]
    return keyed(t,K)

def hit(t):
    K=[(0.00,P()),(0.07,P(rot={'chest':(-8,0,3),'head':(-10,0,0),'body':(-3,0,0)},trans={'chest':(0,0,-0.03)},scale={'chest':0.98})),(0.35,P())]
    return keyed(t,K)

def death(t):   # 2.2s: collapses forward
    K=[(0.00,P()),
       (0.40,P(rot={'chest':(-6,0,0),'head':(-10,0,0),'shoulder_px':(0,0,25),'shoulder_nx':(0,0,-25)},trans={'chest':(0,0.02,-0.03)})),
       (1.10,P(rot={'body':(14,0,3),'chest':(26,0,4),'head':(28,0,6),'shoulder_px':(0,0,-30),'forearm_px':(0,0,-14),'shoulder_nx':(0,0,12),'forearm_nx':(0,0,8)},
               trans={'body':(0,-0.06,0.05),'chest':(0,-0.04,0.06)},scale={'chest':0.93})),
       (2.20,P(rot={'body':(24,0,4),'chest':(38,0,6),'head':(40,0,8),'shoulder_px':(0,0,-42),'forearm_px':(0,0,-18),'hand_px':(0,0,-14),'shoulder_nx':(0,0,16),'forearm_nx':(0,0,12)},
               trans={'body':(0,-0.12,0.08),'chest':(0,-0.08,0.10)},scale={'chest':0.86}))]
    return keyed(t,K)

ANIMS={'idle':(3.0,idle,True),'walk':(1.3,walk,True),'melee':(1.4,melee,False),'throw':(1.7,throw,False),'cast':(1.6,cast,False),
       'slam':(1.3,slam,False),'grab':(1.2,grab,False),'hit':(0.35,hit,False),'death':(2.2,death,False)}
