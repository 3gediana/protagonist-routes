import os
b=os.environ["DEVIN_SSH_KEY_SANS"].strip()
b="".join(b.replace("-----BEGIN OPENSSH PRIVATE KEY-----","").replace("-----END OPENSSH PRIVATE KEY-----","").split())
lines=[b[i:i+70] for i in range(0,len(b),70)]
out="-----BEGIN OPENSSH PRIVATE KEY-----\n"+"\n".join(lines)+"\n-----END OPENSSH PRIVATE KEY-----\n"
os.makedirs(os.path.expanduser("~/.ssh"),exist_ok=True)
p=os.path.expanduser("~/.ssh/devin_key")
open(p,"w").write(out); os.chmod(p,0o600)
print("ok")
