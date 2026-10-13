import numpy as np, sys
sys.path.insert(0,'/home/claude/tools')
from glbio import *; from rigtool import *; import hero_rig, hero_anims
SRC='/mnt/user-data/uploads/Meshy_AI_The_Iron_Wanderer_1009140630_texture.glb'
js,binc,pos,uv,nrm,idx,img=load_mesh(SRC)
col=vertex_colors(uv,img); sk=hero_rig.make_skeleton()
m,rigid=hero_rig.masks_and_rigid(pos,col)
J,W=compute_weights(pos,sk,m,rigid)
# seam-safe: duplicates with equal position share weights of the first occurrence
q=np.round(pos*1e4).astype(np.int64)
_,first,inv=np.unique(q,axis=0,return_index=True,return_inverse=True)
inv=inv.reshape(-1); J=J[first][inv]; W=W[first][inv]
mesh=dict(name='Hero',pos=pos,nrm=nrm,uv=uv,idx=idx.reshape(-1,3),J=J,W=W)
write_rigged('/home/claude/out/project/art/models/hero.glb',js,binc,sk,[mesh],hero_anims.ANIMS)
import os; print('hero.glb',os.path.getsize('/home/claude/out/project/art/models/hero.glb')//1024,'KB')
