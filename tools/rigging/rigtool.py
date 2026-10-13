import numpy as np, json, struct, math
from glbio import *

# ---------------------------------------------------------------- math
def smoothstep(e0,e1,x):
    t=np.clip((x-e0)/(e1-e0),0,1); return t*t*(3-2*t)

def seg_dist(P,a,b):
    a=np.asarray(a,float); b=np.asarray(b,float); ab=b-a
    t=np.clip(((P-a)@ab)/max(ab@ab,1e-9),0,1)
    return np.linalg.norm(P-(a+t[:,None]*ab),axis=1)

def rot_x(d):
    c,s=math.cos(math.radians(d)),math.sin(math.radians(d)); return np.array([[1,0,0],[0,c,-s],[0,s,c]])
def rot_y(d):
    c,s=math.cos(math.radians(d)),math.sin(math.radians(d)); return np.array([[c,0,s],[0,1,0],[-s,0,c]])
def rot_z(d):
    c,s=math.cos(math.radians(d)),math.sin(math.radians(d)); return np.array([[c,-s,0],[s,c,0],[0,0,1]])
def euler(rx=0,ry=0,rz=0):  # R = Ry * Rx * Rz  (Godot default YXZ)
    return rot_y(ry)@rot_x(rx)@rot_z(rz)
def mat_to_quat(R):
    t=np.trace(R)
    if t>0:
        s=math.sqrt(t+1)*2; w=0.25*s; x=(R[2,1]-R[1,2])/s; y=(R[0,2]-R[2,0])/s; z=(R[1,0]-R[0,1])/s
    elif R[0,0]>R[1,1] and R[0,0]>R[2,2]:
        s=math.sqrt(1+R[0,0]-R[1,1]-R[2,2])*2; w=(R[2,1]-R[1,2])/s; x=0.25*s; y=(R[0,1]+R[1,0])/s; z=(R[0,2]+R[2,0])/s
    elif R[1,1]>R[2,2]:
        s=math.sqrt(1+R[1,1]-R[0,0]-R[2,2])*2; w=(R[0,2]-R[2,0])/s; x=(R[0,1]+R[1,0])/s; y=0.25*s; z=(R[1,2]+R[2,1])/s
    else:
        s=math.sqrt(1+R[2,2]-R[0,0]-R[1,1])*2; w=(R[1,0]-R[0,1])/s; x=(R[0,2]+R[2,0])/s; y=(R[1,2]+R[2,1])/s; z=0.25*s
    q=np.array([x,y,z,w]); return q/np.linalg.norm(q)

# ---------------------------------------------------------------- skeleton
class Skeleton:
    def __init__(self,bones):
        # bones: list of dict(name,parent,joint,segs,r,k)
        self.bones=bones; self.names=[b['name'] for b in bones]
        self.idx={n:i for i,n in enumerate(self.names)}
        for b in bones: b['joint']=np.asarray(b['joint'],float)
    def parent(self,i):
        p=self.bones[i]['parent']; return None if p is None else self.idx[p]
    def local_t(self,i):
        p=self.parent(i); j=self.bones[i]['joint']
        return j if p is None else j-self.bones[p]['joint']
    def pose_matrices(self,pose):
        """pose: dict(rot={bone:(rx,ry,rz)}, trans={bone:(dx,dy,dz)}, scale={bone:s})"""
        n=len(self.bones); G=[None]*n
        for i,b in enumerate(self.bones):
            M=np.eye(4)
            R=euler(*pose.get('rot',{}).get(b['name'],(0,0,0)))
            s=pose.get('scale',{}).get(b['name'],1.0)
            M[:3,:3]=R*s
            M[:3,3]=self.local_t(i)+np.array(pose.get('trans',{}).get(b['name'],(0,0,0)))
            p=self.parent(i)
            G[i]=M if p is None else G[p]@M
        return G
    def skin_matrices(self,pose):
        G=self.pose_matrices(pose); out=[]
        for i,b in enumerate(self.bones):
            inv=np.eye(4); inv[:3,3]=-b['joint']
            out.append(G[i]@inv)
        return np.array(out)

def compute_weights(P,sk,masks=None,rigid=None,top=4,kdefault=4.0):
    """envelope weights. masks: bone->(N,) multiplier. rigid: bone->bool mask (weight 1 to that bone)."""
    N=len(P); nb=len(sk.bones); raw=np.zeros((N,nb),np.float32)
    for i,b in enumerate(sk.bones):
        d=np.full(N,1e9,np.float32)
        for (a,c) in b['segs']: d=np.minimum(d,seg_dist(P,a,c).astype(np.float32))
        k=b.get('k',kdefault)
        w=np.exp(-k*(d/b['r'])**2)
        if masks and b['name'] in masks: w=w*masks[b['name']]
        raw[:,i]=w
    if rigid:
        for bone,m in rigid.items():
            raw[m,:]=0; raw[m,sk.idx[bone]]=1.0
    # fallback for vertices where all weights ~0: nearest bone by distance
    s=raw.sum(1)
    bad=s<1e-6
    if bad.any():
        for i,b in enumerate(sk.bones):
            d=np.full(N,1e9,np.float32)
            for (a,c) in b['segs']: d=np.minimum(d,seg_dist(P,a,c).astype(np.float32))
            if i==0: best=d.copy(); bi=np.zeros(N,int)
            else:
                m=d<best; best[m]=d[m]; bi[m]=i
        raw[bad,bi[bad]]=1.0
    # top-4
    idxs=np.argsort(-raw,axis=1)[:,:top]
    w=np.take_along_axis(raw,idxs,1)
    w[w<0.02*w[:,:1]]=0
    w=w/w.sum(1,keepdims=True)
    return idxs.astype(np.uint16),w.astype(np.float32)

def lbs(P,J,W,skin_mats):
    out=np.zeros_like(P)
    Ph=np.concatenate([P,np.ones((len(P),1),np.float32)],1)
    for k in range(J.shape[1]):
        M=skin_mats[J[:,k]]            # (N,4,4)
        out+=W[:,k:k+1]*np.einsum('nij,nj->ni',M,Ph)[:,:3]
    return out

# ---------------------------------------------------------------- animation helpers
def ease(t): return t*t*(3-2*t)

def keyed(t,keys):
    """keys: list of (time, pose) with pose dict(rot,trans,scale). linear-eased interpolation of every channel."""
    if t<=keys[0][0]: return keys[0][1]
    if t>=keys[-1][0]: return keys[-1][1]
    for (t0,p0),(t1,p1) in zip(keys[:-1],keys[1:]):
        if t0<=t<=t1:
            u=ease((t-t0)/max(t1-t0,1e-6)); return blend(p0,p1,u)
    return keys[-1][1]

def blend(p0,p1,u):
    out={'rot':{},'trans':{},'scale':{}}
    for ch,zero in (('rot',(0,0,0)),('trans',(0,0,0))):
        names=set(p0.get(ch,{}))|set(p1.get(ch,{}))
        for n in names:
            a=np.array(p0.get(ch,{}).get(n,zero),float); b=np.array(p1.get(ch,{}).get(n,zero),float)
            out[ch][n]=tuple(a+(b-a)*u)
    names=set(p0.get('scale',{}))|set(p1.get('scale',{}))
    for n in names:
        a=p0.get('scale',{}).get(n,1.0); b=p1.get('scale',{}).get(n,1.0); out['scale'][n]=a+(b-a)*u
    return out

def P(rot=None,trans=None,scale=None): return {'rot':rot or {}, 'trans':trans or {}, 'scale':scale or {}}

# ---------------------------------------------------------------- GLB writer
class GLBWriter:
    def __init__(self,src_js,src_bin):
        self.js={'asset':{'version':'2.0','generator':'claude-rigtool'},'scene':0,'scenes':[{'nodes':[]}],
                 'nodes':[],'meshes':[],'skins':[],'animations':[],'accessors':[],'bufferViews':[],'buffers':[{}],
                 'materials':src_js['materials'],'textures':src_js['textures'],'samplers':src_js.get('samplers',[]),'images':[]}
        self.bin=bytearray()
        # copy images
        for im in src_js['images']:
            bv=src_js['bufferViews'][im['bufferView']]
            data=bytes(src_bin[bv.get('byteOffset',0):bv.get('byteOffset',0)+bv['byteLength']])
            vi=self.view(data)
            self.js['images'].append({'mimeType':im['mimeType'],'bufferView':vi})
    def view(self,data,target=None):
        while len(self.bin)%4: self.bin.append(0)
        off=len(self.bin); self.bin+=data
        bv={'buffer':0,'byteOffset':off,'byteLength':len(data)}
        if target: bv['target']=target
        self.js['bufferViews'].append(bv); return len(self.js['bufferViews'])-1
    def accessor(self,arr,ctype,atype,target=None,minmax=False,normalized=False):
        vi=self.view(arr.tobytes(),target)
        a={'bufferView':vi,'componentType':ctype,'count':int(arr.shape[0]),'type':atype}
        if minmax: a['min']=[float(x) for x in arr.min(0)]; a['max']=[float(x) for x in arr.max(0)]
        if normalized: a['normalized']=True
        self.js['accessors'].append(a); return len(self.js['accessors'])-1
    def add_node(self,node):
        self.js['nodes'].append(node); return len(self.js['nodes'])-1
    def save(self,path):
        self.js['buffers'][0]={'byteLength':len(self.bin)}
        js=json.dumps(self.js,separators=(',',':')).encode()
        while len(js)%4: js+=b' '
        b=bytes(self.bin)
        while len(b)%4: b+=b'\0'
        total=12+8+len(js)+8+len(b)
        with open(path,'wb') as f:
            f.write(struct.pack('<4sII',b'glTF',2,total))
            f.write(struct.pack('<II',len(js),0x4E4F534A)); f.write(js)
            f.write(struct.pack('<II',len(b),0x004E4942)); f.write(b)

def write_rigged(path,src_js,src_bin,sk,meshes,animations,fps=30,model_name='Model'):
    """meshes: list of dict(name,pos,nrm,uv,idx,J,W). animations: dict name->(duration,fn(t)->pose,loop)"""
    w=GLBWriter(src_js,src_bin)
    # skeleton nodes
    arm=w.add_node({'name':'Armature','children':[]})
    joint_nodes=[]
    for i,b in enumerate(sk.bones):
        n={'name':b['name'],'translation':[float(x) for x in sk.local_t(i)]}
        joint_nodes.append(w.add_node(n))
    for i,b in enumerate(sk.bones):
        p=sk.parent(i)
        if p is None: w.js['nodes'][arm]['children'].append(joint_nodes[i])
        else: w.js['nodes'][joint_nodes[p]].setdefault('children',[]).append(joint_nodes[i])
    # inverse bind matrices (column-major)
    ibm=[]
    for b in sk.bones:
        m=np.eye(4,dtype=np.float32); m[:3,3]=-b['joint']; ibm.append(m.T.reshape(-1))
    ibm=np.array(ibm,np.float32)
    iacc=w.accessor(ibm.reshape(-1,16),5126,'MAT4')
    w.js['skins'].append({'joints':joint_nodes,'inverseBindMatrices':iacc,'skeleton':arm,'name':'Skin'})
    scene_nodes=[arm]
    for m in meshes:
        pos=m['pos'].astype(np.float32); nrm=m['nrm'].astype(np.float32); uv=m['uv'].astype(np.float32)
        attrs={'POSITION':w.accessor(pos,5126,'VEC3',34962,minmax=True),
               'NORMAL':w.accessor(nrm,5126,'VEC3',34962),
               'TEXCOORD_0':w.accessor(uv,5126,'VEC2',34962)}
        if m.get('J') is not None:
            attrs['JOINTS_0']=w.accessor(m['J'].astype(np.uint16),5123,'VEC4',34962)
            attrs['WEIGHTS_0']=w.accessor(m['W'].astype(np.float32),5126,'VEC4',34962)
        ia=w.accessor(m['idx'].astype(np.uint32).reshape(-1),5125,'SCALAR',34963)
        w.js['meshes'].append({'name':m['name'],'primitives':[{'attributes':attrs,'indices':ia,'material':0,'mode':4}]})
        mi=len(w.js['meshes'])-1
        node={'name':m['name'],'mesh':mi}
        if m.get('J') is not None: node['skin']=0
        scene_nodes.append(w.add_node(node))
    w.js['scenes'][0]['nodes']=scene_nodes
    # animations
    for name,(dur,fn,loop) in animations.items():
        n=int(round(dur*fps))
        times=np.array([i/fps for i in range(n+1)],np.float32)
        poses=[fn(min(t,dur)) for t in times]
        samplers=[];channels=[]
        tacc=w.accessor(times.reshape(-1,1)[:,0:1].reshape(-1,1),5126,'SCALAR',minmax=True)
        # SCALAR accessor needs 1-D data
        for bi,b in enumerate(sk.bones):
            rots=np.array([mat_to_quat(euler(*p.get('rot',{}).get(b['name'],(0,0,0)))) for p in poses],np.float32)
            anim_rot=any(abs(r).sum()>1e-6 for r in [np.array(p.get('rot',{}).get(b['name'],(0,0,0))) for p in poses])
            if anim_rot:
                # ensure quaternion continuity
                for i in range(1,len(rots)):
                    if np.dot(rots[i],rots[i-1])<0: rots[i]=-rots[i]
                oa=w.accessor(rots,5126,'VEC4')
                samplers.append({'input':tacc,'output':oa,'interpolation':'LINEAR'})
                channels.append({'sampler':len(samplers)-1,'target':{'node':joint_nodes[bi],'path':'rotation'}})
            has_t=any(b['name'] in p.get('trans',{}) for p in poses)
            if has_t:
                tr=np.array([sk.local_t(bi)+np.array(p.get('trans',{}).get(b['name'],(0,0,0))) for p in poses],np.float32)
                oa=w.accessor(tr,5126,'VEC3')
                samplers.append({'input':tacc,'output':oa,'interpolation':'LINEAR'})
                channels.append({'sampler':len(samplers)-1,'target':{'node':joint_nodes[bi],'path':'translation'}})
            has_s=any(b['name'] in p.get('scale',{}) for p in poses)
            if has_s:
                sc=np.array([[p.get('scale',{}).get(b['name'],1.0)]*3 for p in poses],np.float32)
                oa=w.accessor(sc,5126,'VEC3')
                samplers.append({'input':tacc,'output':oa,'interpolation':'LINEAR'})
                channels.append({'sampler':len(samplers)-1,'target':{'node':joint_nodes[bi],'path':'scale'}})
        w.js['animations'].append({'name':name,'samplers':samplers,'channels':channels})
    w.save(path)
    return w
