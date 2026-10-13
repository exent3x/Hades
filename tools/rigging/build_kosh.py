import numpy as np, sys, json, os
sys.path.insert(0,'/home/claude/tools')
from glbio import *; from rigtool import *; import kosh_rig, kosh_anims
SRC='/mnt/user-data/uploads/Meshy_AI_Bonebound_Warlock_1009140338_texture.glb'
js,binc,pos,uv,nrm,idx,img=load_mesh(SRC); col=vertex_colors(uv,img)
sword=np.load('/home/claude/prev/kosh_sword_mask.npy')
sk=kosh_rig.make_skeleton(); m,rigid=kosh_rig.masks_and_rigid(pos,col)
J,W=compute_weights(pos,sk,m,rigid)
q=np.round(pos*1e4).astype(np.int64); _,first,inv=np.unique(q,axis=0,return_index=True,return_inverse=True); inv=inv.reshape(-1)
J=J[first][inv]; W=W[first][inv]
tri=idx.reshape(-1,3); tri_sw=sword[tri].all(1)
def sub(mask,name,skinned,P=None,N=None):
    t=tri[mask]; used,remap=np.unique(t,return_inverse=True)
    d=dict(name=name,pos=(pos if P is None else P)[used],nrm=(nrm if N is None else N)[used],uv=uv[used],idx=remap.reshape(-1,3))
    if skinned: d['J']=J[used]; d['W']=W[used]
    return d
body=sub(~tri_sw,'Body',True)
# --- sword to canonical frame: tip along +Z, flat blade normal along +Y, centre at origin ---
sp=pos[sword]; c0=sp.mean(0); u,s,vt=np.linalg.svd(sp-c0,full_matrices=False); ax=vt[0]
t=(sp-c0)@ax; rad=np.linalg.norm((sp-c0)-np.outer(t,ax),axis=1)
lo=rad[t<np.percentile(t,12)].mean(); hi=rad[t>np.percentile(t,88)].mean()
tip_dir=ax if hi<lo else -ax          # the tip end is the thin one
tt=(sp-c0)@tip_dir; centre=c0+tip_dir*((tt.max()+tt.min())/2)
zf=tip_dir/np.linalg.norm(tip_dir)
n0=np.array([0.0,0.0,1.0]); yf=n0-zf*(n0@zf); yf/=np.linalg.norm(yf); xf=np.cross(yf,zf)
Rm=np.stack([xf,yf,zf],1)             # columns
Pw=(pos-centre)@Rm; Nw=nrm@Rm
sw=sub(tri_sw,'Sword',False,Pw,Nw)
L=float(Pw[sword][:,2].max()-Pw[sword][:,2].min()); wdt=float(Pw[sword][:,0].max()-Pw[sword][:,0].min())
print('sword length',L,'width',wdt,'tip dir',tip_dir.round(3))
write_rigged('/home/claude/out/project/art/models/koschei.glb',js,binc,sk,[body,sw],kosh_anims.ANIMS)
info={'model_min_y':float(pos[~sword][:,1].min()),'model_height':float(pos[~sword][:,1].max()-pos[~sword][:,1].min()),'sword_length':L,'sword_width':wdt}
print(info); json.dump(info,open('/home/claude/out/koschei_info.json','w'))
print('koschei.glb',os.path.getsize('/home/claude/out/project/art/models/koschei.glb')//1024,'KB')
# render canonical sword
from PIL import Image
sidx=np.where(sword)[0]
v=[render_points(Pw[sidx],col[sidx],yaw,0,size=300,span=1.4,center=np.zeros(3),pt=3) for yaw in (0,90)]
Image.fromarray(np.hstack(v)).save('/home/claude/prev/kosh_sword.png')
