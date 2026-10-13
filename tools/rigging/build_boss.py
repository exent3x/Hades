import numpy as np, sys, json
sys.path.insert(0,'/home/claude/tools')
from glbio import *; from rigtool import *; import boss_rig, boss_anims
SRC='/mnt/user-data/uploads/Meshy_AI_Mushroom_Crowned_Fore_1009135856_texture.glb'
js,binc,pos,uv,nrm,idx,img=load_mesh(SRC)
col=vertex_colors(uv,img); sk=boss_rig.make_skeleton()
m,rigid,sp=boss_rig.masks_and_rigid(pos,col)
J,W=compute_weights(pos,sk,m,rigid)
q=np.round(pos*1e4).astype(np.int64)
_,first,inv=np.unique(q,axis=0,return_index=True,return_inverse=True); inv=inv.reshape(-1)
J=J[first][inv]; W=W[first][inv]; sp=sp[first][inv]       # seam-safe
tri=idx.reshape(-1,3)
tri_sp=sp[tri].sum(1)>=2
def sub(mask,name):
    t=tri[mask]; used,remap=np.unique(t,return_inverse=True)
    return dict(name=name,pos=pos[used],nrm=nrm[used],uv=uv[used],idx=remap.reshape(-1,3),J=J[used],W=W[used])
body=sub(~tri_sp,'Body'); spr=sub(tri_sp,'Spruce')
print('body verts',len(body['pos']),'spruce verts',len(spr['pos']))
write_rigged('/home/claude/out/project/art/models/leshy.glb',js,binc,sk,[body,spr],boss_anims.ANIMS)
# pivot info for the thrown spruce: base centre of the spruce mesh in model space
sp_pos=spr['pos']; ymin=sp_pos[:,1].min(); low=sp_pos[sp_pos[:,1]<ymin+0.04]
info={'base':[float(low[:,0].mean()),float(ymin),float(low[:,2].mean())],'height':float(sp_pos[:,1].max()-ymin),
      'model_min_y':float(pos[:,1].min()),'model_height':float(pos[:,1].max()-pos[:,1].min())}
print(info); json.dump(info,open('/home/claude/out/spruce_info.json','w'))
import os; print('leshy.glb',os.path.getsize('/home/claude/out/project/art/models/leshy.glb')//1024,'KB')
