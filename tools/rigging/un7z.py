import sys, os, struct, lzma, zlib

class R:
    def __init__(s,b): s.b=b; s.p=0
    def byte(s): v=s.b[s.p]; s.p+=1; return v
    def read(s,n): v=s.b[s.p:s.p+n]; s.p+=n; return v
    def num(s):
        first=s.byte(); mask=0x80; val=0
        for i in range(8):
            if first&mask==0:
                high=first&(mask-1); val+=high<<(8*i); return val
            val|=s.byte()<<(8*i); mask>>=1
        return val
    def bits(s,n):
        out=[]; 
        allw=s.byte()
        if allw:
            return [True]*n
        cur=0;mask=0
        for i in range(n):
            if mask==0: cur=s.byte(); mask=0x80
            out.append(bool(cur&mask)); mask>>=1
        return out
    def bits_nodef(s,n):
        out=[];cur=0;mask=0
        for i in range(n):
            if mask==0: cur=s.byte(); mask=0x80
            out.append(bool(cur&mask)); mask>>=1
        return out

def read_streams_info(r):
    pack_pos=0; pack_sizes=[]; folders=[]; unpack_sizes=[]; nsub=[]; sub_sizes=[]
    while True:
        t=r.byte()
        if t==0: break
        if t==6:
            pack_pos=r.num(); n=r.num()
            while True:
                t2=r.byte()
                if t2==0: break
                if t2==9: pack_sizes=[r.num() for _ in range(n)]
                elif t2==0x0A:
                    d=r.bits(n)
                    for x in d:
                        if x: r.read(4)
        elif t==7:
            assert r.byte()==0x0B
            nf=r.num(); assert r.byte()==0
            for _ in range(nf):
                nc=r.num(); coders=[]; tin=0; tout=0
                for _ in range(nc):
                    flag=r.byte(); idsz=flag&0xF; cid=bytes(r.read(idsz))
                    nin=nout=1
                    if flag&0x10: nin=r.num(); nout=r.num()
                    props=b''
                    if flag&0x20: props=bytes(r.read(r.num()))
                    coders.append((cid,nin,nout,props)); tin+=nin; tout+=nout
                bps=[(r.num(),r.num()) for _ in range(tout-1)]
                npacked=tin-len(bps)
                if npacked==1:
                    bound={b[0] for b in bps}
                    packed=[i for i in range(tin) if i not in bound]
                else:
                    packed=[r.num() for _ in range(npacked)]
                folders.append(dict(coders=coders,bps=bps,packed=packed,tout=tout))
            assert r.byte()==0x0C
            for f in folders: f['unpack']=[r.num() for _ in range(f['tout'])]
            while True:
                t2=r.byte()
                if t2==0: break
                if t2==0x0A:
                    d=r.bits(len(folders))
                    for x in d:
                        if x: r.read(4)
        elif t==8:
            nsub=[1]*len(folders)
            while True:
                t2=r.byte()
                if t2==0: break
                if t2==0x0D: nsub=[r.num() for _ in folders]
                elif t2==9:
                    for fi,f in enumerate(folders):
                        n=nsub[fi]
                        if n==0: continue
                        tot=f['unpack'][-1] if f['tout']==1 else f['unpack'][f['tout']-1]
                        # main unpack size = size of the final (unbound) out stream
                        bound_out={b[1] for b in f['bps']}
                        main=[i for i in range(f['tout']) if i not in bound_out][0]
                        tot=f['unpack'][main]
                        ss=[r.num() for _ in range(n-1)]
                        sub_sizes.append(ss+[tot-sum(ss)])
                elif t2==0x0A:
                    cnt=sum(n for n in nsub)  # approximate; digests ignored
                    # digests only for streams with unknown crc
                    d=r.bits(cnt)
                    for x in d:
                        if x: r.read(4)
            if not sub_sizes:
                for fi,f in enumerate(folders):
                    bound_out={b[1] for b in f['bps']}
                    main=[i for i in range(f['tout']) if i not in bound_out][0]
                    sub_sizes.append([f['unpack'][main]])
    return dict(pack_pos=pack_pos,pack_sizes=pack_sizes,folders=folders,nsub=nsub,sub_sizes=sub_sizes)

def decode_folder(f, packs):
    # simple chains: apply coder that consumes the pack stream first, then the others
    coders=f['coders']
    data=packs[0]
    order=list(range(len(coders)))
    # coder with in-index == packed[0] is first
    inidx=0; first=None
    for ci,(cid,nin,nout,props) in enumerate(coders):
        if f['packed'][0] in range(inidx,inidx+nin): first=ci
        inidx+=nin
    chain=[first]
    bp={b[0]:b[1] for b in f['bps']}  # inIndex -> outIndex
    cur=first
    while True:
        # find in-index of cur coder
        inidx=sum(coders[i][1] for i in range(cur))
        # out index of cur
        outidx=sum(coders[i][2] for i in range(cur))
        nxt=None
        for (i_in,i_out) in f['bps']:
            if i_out==outidx:
                # coder that owns i_in
                acc=0
                for ci,(cid,nin,nout,props) in enumerate(coders):
                    if acc<=i_in<acc+nin: nxt=ci
                    acc+=nin
        if nxt is None: break
        chain.append(nxt); cur=nxt
    for ci in chain:
        cid,nin,nout,props=coders[ci]
        if cid==b'\x21':
            ds=props[0]; dict_size=(2|(ds&1))<<(ds//2+11) if ds<40 else 0xFFFFFFFF
            dec=lzma.LZMADecompressor(format=lzma.FORMAT_RAW,filters=[{'id':lzma.FILTER_LZMA2,'dict_size':dict_size}])
            data=dec.decompress(data)
        elif cid==b'\x03\x01\x01':
            d=props[0]; lc=d%9; d//=9; lp=d%5; pb=d//5
            dict_size=struct.unpack('<I',props[1:5])[0]
            dec=lzma.LZMADecompressor(format=lzma.FORMAT_RAW,filters=[{'id':lzma.FILTER_LZMA1,'dict_size':dict_size,'lc':lc,'lp':lp,'pb':pb}])
            data=dec.decompress(data)
        elif cid==b'\x03\x03\x01\x03':
            dec=lzma.LZMADecompressor(format=lzma.FORMAT_RAW,filters=[{'id':lzma.FILTER_X86}]); data=dec.decompress(data)
        elif cid==b'\x00':
            pass
        else:
            raise Exception('unsupported coder %r'%cid)
    return data

def main(path,out):
    b=open(path,'rb').read()
    assert b[:6]==b"7z\xbc\xaf\x27\x1c"
    off,size,crc=struct.unpack('<QQI',b[12:32])
    hdr=b[32+off:32+off+size]
    r=R(hdr)
    t=r.byte()
    while t==0x17:
        si=read_streams_info(r)
        pos=32+si['pack_pos']; packs=[]
        for sz in si['pack_sizes']: packs.append(b[pos:pos+sz]); pos+=sz
        hdr=decode_folder(si['folders'][0],packs)
        r=R(hdr); t=r.byte()
    assert t==1
    si=None; names=[]; empty_stream=[]; empty_file=[]
    while True:
        t=r.byte()
        if t==0: break
        if t==2: # archive properties
            while True:
                t2=r.byte()
                if t2==0:break
                r.read(r.num())
        elif t==3 or t==4:
            si=read_streams_info(r)
        elif t==5: # files info (handled below)
            nfiles=r.num()
            while True:
                pt=r.byte()
                if pt==0: break
                sz=r.num(); body=r.read(sz)
                if pt==0x0E: empty_stream=R(body).bits_nodef(nfiles)
                elif pt==0x0F:
                    ne=sum(empty_stream); empty_file=R(body).bits_nodef(ne)
                elif pt==0x11:
                    assert body[0]==0
                    raw=bytes(body[1:]).decode('utf-16le').split('\x00')
                    names=raw[:nfiles]
    if si is None: si=dict(pack_pos=0,pack_sizes=[],folders=[],nsub=[],sub_sizes=[])
    # unpack folders
    pos=32+si['pack_pos']; pidx=0; filedata=[]
    for fi,f in enumerate(si['folders']):
        npk=len(f['packed']); packs=[]
        for _ in range(npk):
            sz=si['pack_sizes'][pidx]; packs.append(b[pos:pos+sz]); pos+=sz; pidx+=1
        data=decode_folder(f,packs)
        o=0
        for sz in si['sub_sizes'][fi]:
            filedata.append(data[o:o+sz]); o+=sz
    if not empty_stream: empty_stream=[False]*len(names)
    k=0; ef=0; n=0
    for i,name in enumerate(names):
        full=os.path.join(out,name)
        if empty_stream[i]:
            isfile = (empty_file[ef] if ef<len(empty_file) else False); ef+=1
            if isfile:
                os.makedirs(os.path.dirname(full) or out,exist_ok=True); open(full,'wb').close()
            else: os.makedirs(full,exist_ok=True)
        else:
            os.makedirs(os.path.dirname(full) or out,exist_ok=True)
            open(full,'wb').write(filedata[k]); k+=1; n+=1
    print('files extracted:',n,'of',len(names))
main(sys.argv[1],sys.argv[2])
