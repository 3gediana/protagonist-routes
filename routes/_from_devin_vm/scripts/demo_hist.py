import struct, sys
NAMES = ['eat','drink','attack','collect','shelter','spear','sleep','grassdress',
         'furcloak','wear','blueprint','cyclebld','deposit','plant','water','feedtame',
         'NOOP','tendfire','cook','mine','axe','pickaxe','monument']
path = sys.argv[1]
with open(path,'rb') as f:
    magic = f.read(4)
    ver, od, cd = struct.unpack('<III', f.read(12))
    n = struct.unpack('<Q', f.read(8))[0]
    print(f"magic={magic} ver={ver} obs_dim={od} cont_dim={cd} count_in_header={n}")
    rec = od*4 + cd*4 + 4 + 4
    import os
    fsz = os.path.getsize(path)
    avail = (fsz - 24)//rec
    print(f"record_bytes={rec} file_records={avail}")
    hist = [0]*23
    # read disc_idx for each record by seeking past obs+cont
    skip = od*4 + cd*4
    step = max(1, avail//300000)  # sample up to ~300k records
    cnt = 0
    base = 24
    for i in range(0, avail, step):
        f.seek(base + i*rec + skip)
        b = f.read(4)
        if len(b) < 4: break
        d = struct.unpack('<I', b)[0]
        if 0 <= d < 23: hist[d]+=1
        cnt+=1
    print(f"sampled={cnt} (stride {step})")
    tot = sum(hist) or 1
    for i in range(23):
        if hist[i]>0:
            print(f"  {i:2d} {NAMES[i]:10s} {hist[i]:7d}  {100.0*hist[i]/tot:5.1f}%")
    dead = [NAMES[i] for i in range(23) if hist[i]==0]
    print("DEAD (0 in sample): " + ", ".join(dead))
