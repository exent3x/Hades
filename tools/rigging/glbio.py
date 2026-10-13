import struct, json, io
import numpy as np
from PIL import Image

def load_glb(path):
    b=open(path,'rb').read()
    off=12; js=None; binc=None
    while off<len(b):
        ln,ty=struct.unpack('<II',b[off:off+8]); data=b[off+8:off+8+ln]; off+=8+ln
        if ty==0x4E4F534A: js=json.loads(data)
        elif ty==0x004E4942: binc=bytearray(data)
    return js,binc

CT={5120:np.int8,5121:np.uint8,5122:np.int16,5123:np.uint16,5125:np.uint32,5126:np.float32}
NC={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
def accessor(js,binc,i):
    a=js['accessors'][i]; bv=js['bufferViews'][a['bufferView']]
    dt=CT[a['componentType']]; n=NC[a['type']]
    off=bv.get('byteOffset',0)+a.get('byteOffset',0)
    stride=bv.get('byteStride',0)
    cnt=a['count']
    if stride and stride!=np.dtype(dt).itemsize*n:
        arr=np.frombuffer(binc,dtype=np.uint8,count=stride*cnt,offset=off).reshape(cnt,stride)[:,:np.dtype(dt).itemsize*n].copy().view(dt)
    else:
        arr=np.frombuffer(binc,dtype=dt,count=cnt*n,offset=off).copy()
    return arr.reshape(cnt,n) if n>1 else arr

def load_mesh(path):
    js,binc=load_glb(path)
    p=js['meshes'][0]['primitives'][0]
    pos=accessor(js,binc,p['attributes']['POSITION']).astype(np.float32)
    uv=accessor(js,binc,p['attributes']['TEXCOORD_0']).astype(np.float32)
    nrm=accessor(js,binc,p['attributes']['NORMAL']).astype(np.float32)
    idx=accessor(js,binc,p['indices']).astype(np.int64)
    tex=js['textures'][js['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
    bv=js['bufferViews'][js['images'][tex]['bufferView']]
    img=Image.open(io.BytesIO(bytes(binc[bv.get('byteOffset',0):bv.get('byteOffset',0)+bv['byteLength']]))).convert('RGB')
    return js,binc,pos,uv,nrm,idx,img

def vertex_colors(uv,img):
    a=np.asarray(img); H,W=a.shape[:2]
    x=np.clip((uv[:,0]%1.0)*(W-1),0,W-1).astype(int); y=np.clip((uv[:,1]%1.0)*(H-1),0,H-1).astype(int)
    return a[y,x]

def render_points(pos,col,yaw_deg=0,pitch_deg=0,size=520,pad=0.06,span=None,center=None,pt=2):
    yaw=np.radians(yaw_deg); pit=np.radians(pitch_deg)
    Ry=np.array([[np.cos(yaw),0,np.sin(yaw)],[0,1,0],[-np.sin(yaw),0,np.cos(yaw)]])
    Rx=np.array([[1,0,0],[0,np.cos(pit),-np.sin(pit)],[0,np.sin(pit),np.cos(pit)]])
    p=(pos-(center if center is not None else pos.mean(0)))@Ry.T@Rx.T
    if span is None: span=np.abs(p[:,:2]).max()*2*(1+pad)
    sx=((p[:,0]/span+0.5)*size).astype(int); sy=((0.5-p[:,1]/span)*size).astype(int)
    order=np.argsort(p[:,2])  # far(-z? ) first; camera looks toward -z, so larger z is nearer
    img=np.full((size,size,3),235,np.uint8)
    for dx in range(pt):
        for dy in range(pt):
            xx=np.clip(sx[order]+dx,0,size-1); yy=np.clip(sy[order]+dy,0,size-1)
            img[yy,xx]=col[order]
    return img
