import numpy as np, math
from rigtool import *
S=math.sin

def idle(t,T=2.6):
    ph=2*math.pi*t/T
    return P(rot={'chest':(1.5*S(ph),0.8*S(ph*0.5),0),'spine':(-1.0*S(ph),0,0),'head':(-1.5*S(ph+0.7),3*S(ph*0.5),0),
                  'shoulder_nx':(0,2*S(ph+0.4),2.0*S(ph)),'elbow_nx':(0,0,2*S(ph+0.9)),'hand_nx':(0,0,3*S(ph+1.4)),
                  'shoulder_px':(0,0,1.5*S(ph+0.6)),'hand_px':(2*S(ph),0,0)},
             trans={'hips':(0,0.006*S(ph),0)},scale={'chest':1.0+0.008*S(ph)})

def walk(t,T=0.9):
    ph=2*math.pi*t/T; s=S(ph); c=math.cos(ph)
    def knee(sign): return 32*max(0.0,sign*c)**1.2+8
    return P(rot={'hips':(0,6*s,0),'chest':(2,-5*s,1.5*s),'head':(0,3*s,0),
                  'thigh_nx':(22*s,0,0),'thigh_px':(-22*s,0,0),
                  'shin_nx':(knee(-1),0,0),'shin_px':(knee(1),0,0),
                  'foot_nx':(-0.3*(22*s+knee(-1)),0,0),'foot_px':(0.3*(22*s-knee(1)),0,0),
                  'shoulder_nx':(0,8*s,2),'hand_nx':(0,0,4*s),'shoulder_px':(0,0,3*s)},
             trans={'hips':(0.012*s,-0.016*abs(c)+0.008,0)})

CLAW_WIND=P(rot={'shoulder_nx':(0,-55,-35),'elbow_nx':(0,0,-10),'hips':(0,-18,0),'chest':(-4,-14,0),'spine':(-4,0,0),'head':(0,10,0)},trans={'hips':(0,0.02,-0.04)})
CLAW_HIT=P(rot={'shoulder_nx':(0,70,12),'elbow_nx':(0,0,6),'hips':(0,24,0),'chest':(10,18,0),'spine':(8,0,0),'head':(4,-10,0),'thigh_px':(-18,0,0),'shin_px':(14,0,0)},trans={'hips':(0,-0.03,0.10)})

def claw(t):   # windup_end 0.45, strike peak 0.55
    return keyed(t,[(0,P()),(0.45,CLAW_WIND),(0.55,CLAW_HIT),(0.75,CLAW_HIT),(1.1,P())])

def combo(t):  # windup_end 0.40; strike peaks 0.50 / 0.80 / 1.10
    back=P(rot={'shoulder_nx':(0,-20,15),'hips':(0,-8,0),'chest':(2,-8,0)},trans={'hips':(0,0,0.04)})
    wind2=P(rot={'shoulder_nx':(0,-45,-40),'hips':(0,-14,0),'chest':(-2,-12,0)},trans={'hips':(0,0.01,0.05)})
    hit2=P(rot={'shoulder_nx':(0,62,-12),'hips':(0,20,0),'chest':(8,14,0),'thigh_nx':(-16,0,0)},trans={'hips':(0,-0.02,0.12)})
    wind3=P(rot={'shoulder_nx':(0,-50,-55),'shoulder_px':(0,0,40),'hips':(0,-20,0),'chest':(-8,-16,0),'head':(-6,0,0)},trans={'hips':(0,0.04,0.08)})
    hit3=P(rot={'shoulder_nx':(0,78,25),'shoulder_px':(0,0,-25),'hips':(0,28,0),'chest':(18,20,0),'spine':(10,0,0),'thigh_px':(-26,0,0),'shin_px':(22,0,0)},trans={'hips':(0,-0.06,0.2)})
    return keyed(t,[(0,P()),(0.40,CLAW_WIND),(0.50,CLAW_HIT),(0.62,back),(0.70,wind2),(0.80,hit2),(0.92,wind3),(1.10,hit3),(1.25,hit3),(1.7,P())])

def lunge(t):  # windup_end 0.55; dash right after
    crouch=P(rot={'shoulder_nx':(0,-60,-20),'chest':(14,-10,0),'spine':(6,0,0),'thigh_nx':(-26,0,0),'shin_nx':(34,0,0),'thigh_px':(-26,0,0),'shin_px':(34,0,0),'hips':(0,-14,0)},trans={'hips':(0,-0.07,-0.10)})
    ext=P(rot={'shoulder_nx':(0,48,6),'elbow_nx':(0,0,8),'chest':(22,10,0),'spine':(8,0,0),'head':(10,0,0),'thigh_nx':(14,0,0),'shin_nx':(10,0,0),'thigh_px':(-34,0,0),'shin_px':(30,0,0),'hips':(0,14,0),'shoulder_px':(0,0,-30)},trans={'hips':(0,-0.04,0.22)})
    return keyed(t,[(0,P()),(0.55,crouch),(0.62,ext),(1.0,ext),(1.4,P())])

def summon(t):  # windup_end 1.0
    up=P(rot={'shoulder_px':(-25,0,75),'shoulder_nx':(0,-30,-45),'elbow_nx':(0,0,-12),'chest':(-12,0,0),'head':(-16,0,0),'thigh_nx':(8,0,0),'thigh_px':(8,0,0)},trans={'hips':(0,0.035,0)})
    slam=P(rot={'shoulder_px':(0,0,-18),'shoulder_nx':(0,20,18),'chest':(16,0,0),'head':(12,0,0),'thigh_nx':(-14,0,0),'shin_nx':(12,0,0)},trans={'hips':(0,-0.05,0.05)})
    return keyed(t,[(0,P()),(0.5,up),(1.0,up),(1.12,slam),(1.3,slam),(2.0,P())])

def storm(t):  # windup_end 0.8
    up=P(rot={'shoulder_nx':(0,-35,-60),'elbow_nx':(0,0,-14),'shoulder_px':(-10,0,82),'chest':(-10,0,0),'head':(-12,0,0)},trans={'hips':(0,0.03,0)})
    down=P(rot={'shoulder_nx':(0,40,25),'shoulder_px':(0,0,-25),'chest':(18,0,0),'head':(12,0,0),'thigh_nx':(-16,0,0),'thigh_px':(-16,0,0)},trans={'hips':(0,-0.06,0.06)})
    return keyed(t,[(0,P()),(0.8,up),(0.9,down),(1.1,down),(1.6,P())])

def nova(t):   # windup_end 0.9
    gather=P(rot={'shoulder_nx':(0,55,10),'shoulder_px':(0,0,-45),'chest':(16,0,0),'head':(14,0,0),'thigh_nx':(-20,0,0),'shin_nx':(26,0,0),'thigh_px':(-20,0,0),'shin_px':(26,0,0)},trans={'hips':(0,-0.09,0)})
    burst=P(rot={'shoulder_nx':(0,-75,-25),'shoulder_px':(0,0,70),'chest':(-14,0,0),'head':(-18,0,0),'thigh_nx':(10,0,0),'thigh_px':(10,0,0)},trans={'hips':(0,0.04,0)})
    return keyed(t,[(0,P()),(0.9,gather),(1.0,burst),(1.25,burst),(1.8,P())])

def point(t):  # windup_end 0.5  (book arm fires)
    wind=P(rot={'shoulder_px':(-15,0,40),'shoulder_nx':(0,-35,-15),'chest':(-4,0,0),'head':(-4,0,0)})
    fire=P(rot={'shoulder_px':(-70,0,12),'shoulder_nx':(0,-40,-10),'chest':(6,-8,0),'hips':(0,-10,0),'head':(4,0,0)},trans={'hips':(0,0,0.04)})
    return keyed(t,[(0,P()),(0.5,wind),(0.58,fire),(0.8,fire),(1.1,P())])

def curse(t):  # windup_end 0.7
    ph=t*40
    wind=P(rot={'shoulder_nx':(0,35,-45),'shoulder_px':(-10,0,58),'chest':(-6,0,0),'head':(8,0,0)})
    shake=P(rot={'shoulder_nx':(0,35,-45+3*S(ph)),'shoulder_px':(-10,0,58+3*S(ph+1)),'chest':(-6,0,0),'head':(8,0,0)})
    rel=P(rot={'shoulder_nx':(0,62,-12),'shoulder_px':(-45,0,20),'chest':(8,10,0),'head':(4,0,0)},trans={'hips':(0,0,0.04)})
    return keyed(t,[(0,P()),(0.35,wind),(0.7,shake),(0.8,rel),(1.0,rel),(1.4,P())])

def shield(t):  # windup_end 0.6
    cross=P(rot={'shoulder_nx':(0,52,10),'elbow_nx':(0,0,0),'shoulder_px':(0,0,-55),'chest':(18,0,0),'spine':(6,0,0),'head':(14,0,0),'thigh_nx':(-12,0,0),'shin_nx':(14,0,0),'thigh_px':(-12,0,0),'shin_px':(14,0,0)},trans={'hips':(0,-0.05,0)})
    return keyed(t,[(0,P()),(0.6,cross),(1.1,cross),(1.4,P())])

def jump_crouch(t):  # 0.5 (windup scrub)
    crouch=P(rot={'shoulder_nx':(0,-50,-10),'shoulder_px':(0,0,-30),'chest':(20,0,0),'spine':(8,0,0),'head':(10,0,0),'thigh_nx':(-40,0,0),'shin_nx':(60,0,0),'thigh_px':(-40,0,0),'shin_px':(60,0,0)},trans={'hips':(0,-0.14,-0.04)})
    return keyed(t,[(0,P()),(0.5,crouch)])

def jump_air(t):  # 0.8 (scrubbed by flight progress)
    launch=P(rot={'shoulder_nx':(0,-35,-40),'shoulder_px':(0,0,60),'chest':(-6,0,0),'head':(-8,0,0),'thigh_nx':(-30,0,0),'shin_nx':(50,0,0),'thigh_px':(-30,0,0),'shin_px':(50,0,0)})
    peak=P(rot={'shoulder_nx':(0,-20,-30),'shoulder_px':(0,0,50),'chest':(4,0,0),'thigh_nx':(-45,0,0),'shin_nx':(70,0,0),'thigh_px':(-45,0,0),'shin_px':(70,0,0)})
    land=P(rot={'shoulder_nx':(0,30,10),'shoulder_px':(0,0,-10),'chest':(14,0,0),'thigh_nx':(-20,0,0),'shin_nx':(10,0,0),'thigh_px':(-20,0,0),'shin_px':(10,0,0)})
    return keyed(t,[(0,launch),(0.35,peak),(0.8,land)])

def jump_land(t):  # 0.45
    imp=P(rot={'shoulder_nx':(0,50,15),'shoulder_px':(0,0,-30),'chest':(20,0,0),'spine':(8,0,0),'head':(12,0,0),'thigh_nx':(-34,0,0),'shin_nx':(50,0,0),'thigh_px':(-34,0,0),'shin_px':(50,0,0)},trans={'hips':(0,-0.12,0.03)})
    return keyed(t,[(0,imp),(0.1,imp),(0.45,P())])

def hit(t):
    return keyed(t,[(0,P()),(0.07,P(rot={'chest':(-10,0,3),'head':(-12,0,0),'spine':(-5,0,0),'shoulder_nx':(0,-10,0)},trans={'hips':(0,0,-0.04)})),(0.32,P())])

def death(t):  # 2.0
    return keyed(t,[(0,P()),
        (0.35,P(rot={'chest':(-8,0,0),'head':(-14,0,0),'shoulder_nx':(0,-20,-20),'shoulder_px':(0,0,30)},trans={'hips':(0,0.02,-0.04)})),
        (1.0,P(rot={'hips':(8,0,0),'chest':(22,0,0),'spine':(14,0,0),'head':(30,0,0),'thigh_nx':(-40,0,0),'shin_nx':(70,0,0),'thigh_px':(-40,0,0),'shin_px':(70,0,0),'shoulder_nx':(0,20,30),'shoulder_px':(0,0,-40)},trans={'hips':(0,-0.30,0.0)})),
        (2.0,P(rot={'hips':(14,0,0),'chest':(34,0,0),'spine':(20,0,0),'head':(40,0,0),'thigh_nx':(-56,0,0),'shin_nx':(100,0,0),'thigh_px':(-56,0,0),'shin_px':(100,0,0),'shoulder_nx':(0,30,50),'shoulder_px':(0,0,-60)},trans={'hips':(0,-0.46,0.02)}))])

ANIMS={'idle':(2.6,idle,True),'walk':(0.9,walk,True),'claw':(1.1,claw,False),'combo':(1.7,combo,False),'lunge':(1.4,lunge,False),
       'summon':(2.0,summon,False),'storm':(1.6,storm,False),'nova':(1.8,nova,False),'point':(1.1,point,False),'curse':(1.4,curse,False),
       'shield':(1.4,shield,False),'jump_crouch':(0.5,jump_crouch,False),'jump_air':(0.8,jump_air,False),'jump_land':(0.45,jump_land,False),
       'hit':(0.32,hit,False),'death':(2.0,death,False)}
