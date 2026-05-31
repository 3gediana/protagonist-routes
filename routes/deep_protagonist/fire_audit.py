import struct, sys, glob, os
# DPB1 demo bin: magic[4] ver(u32) od(u32) cd(u32) count(u64), then records:
# od*float obs + cd*float cont + didx(u32) + done(u32)
def audit(path):
    with open(path,'rb') as f:
        data=f.read()
    magic=data[:4]
    if magic!=b'DPB1':
        return dict(err=f"bad magic {magic}")
    ver,od,cd=struct.unpack_from('<III',data,4)
    count=struct.unpack_from('<Q',data,16)[0]
    rec=od*4+cd*4+8
    body=data[24:]
    n=len(body)//rec
    didx_off=od*4+cd*4
    counts={}
    for i in range(n):
        base=i*rec
        didx=struct.unpack_from('<I',body,base+didx_off)[0]
        counts[didx]=counts.get(didx,0)+1
    return dict(od=od,cd=cd,hdr_count=count,recs=n,a17=counts.get(17,0),
                a2=counts.get(2,0), a4=counts.get(4,0), a6=counts.get(6,0),
                top=sorted(counts.items(),key=lambda x:-x[1])[:6])
if __name__=="__main__":
    paths=[]
    for a in sys.argv[1:]:
        paths+=sorted(glob.glob(a))
    print(f"{'file':<28}{'recs':>8}{'a17(fire)':>10}{'a2(atk)':>8}{'a4(shel)':>9}{'a6(sleep)':>10}")
    for p in paths:
        r=audit(p)
        if 'err' in r: print(p, r['err']); continue
        print(f"{os.path.basename(p):<28}{r['recs']:>8}{r['a17']:>10}{r['a2']:>8}{r['a4']:>9}{r['a6']:>10}   top={r['top']}")
