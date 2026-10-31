import struct, sys, os
# Patch n_rec header field (offset 16, uint64) of a DPB1 demo file that was
# killed mid-stream (count left at 0). count = (filesize - 24) // record_bytes.
# record_bytes = obs_dim*4 + cont_dim*4 + 8 (read from header). The BC loader
# tolerates a torn trailing record (fread short-read breaks the loop).
path = sys.argv[1]
with open(path, 'r+b') as f:
    magic = f.read(4)
    assert magic == b'DPB1', f"bad magic {magic}"
    ver, od, cd = struct.unpack('<III', f.read(12))
    old = struct.unpack('<Q', f.read(8))[0]
    rec = od*4 + cd*4 + 8
    fsz = os.path.getsize(path)
    n = (fsz - 24) // rec
    f.seek(16)
    f.write(struct.pack('<Q', n))
    print(f"{path}: obs_dim={od} rec_bytes={rec} old_count={old} -> patched_count={n} (filesize={fsz})")
