# CONNECT — 连到用户 Windows 机器（每个新会话/断线后必做）

> 训练全在用户的 Windows 机器上跑。你（接手模型）在自己的 Linux VM 上，要先连过去。
> **隧道会断**（用户重启电脑、或你的 VM 重启 → chisel client 死）。断了就照这篇重连。

## 架构（数据怎么走）
```
你的 VM (Linux)
  └─ chisel client ──► 公网隧道 (xiaomiqiu 内网穿透)
                          └─► 用户 Windows 上的 chisel server (端口 3303)
                                └─► 用户 Windows 的 OpenSSH (端口 22)
```
本地 `localhost:2222` 被 chisel 映射到用户 Windows 的 `:22`，所以 SSH 连 `localhost:2222` 就是连到那台 Windows。

## 两样不变的东西（不用每次问用户）
> 完整权威说明见同 prompt 文件夹的 `03_ssh_remote_access_v2.md`，本篇是速查。
1. **隧道 URL 固定不变**：`https://t1dwvu6bzdng.vip3.xiaomiqiu123.top`（小米球付费版，重启电脑也不换）。用户那边常驻 chisel server（`D:\tools\chisel\chisel.exe server --port 3303`）+ OpenSSH(:22)。
2. **SSH 私钥从 secret 重建**：私钥不绑定某台 VM——它存在 secret `DEVIN_SSH_KEY_SANS`（只有 base64 正文、缺头尾），照下面脚本重建到 `~/.ssh/devin_key`。Windows 用户名 `sans`。

## 重连命令（新会话/VM 上从零跑）
```bash
# 0) 重建私钥（secret 缺 PEM 头尾，必须脚本重建，直接 echo 会 libcrypto 报错）
python3 - <<'EOF'
import os
b=os.environ["DEVIN_SSH_KEY_SANS"].strip()
b="".join(b.replace("-----BEGIN OPENSSH PRIVATE KEY-----","").replace("-----END OPENSSH PRIVATE KEY-----","").split())
out="-----BEGIN OPENSSH PRIVATE KEY-----\n"+"\n".join(b[i:i+70] for i in range(0,len(b),70))+"\n-----END OPENSSH PRIVATE KEY-----\n"
os.makedirs(os.path.expanduser("~/.ssh"),exist_ok=True)
p=os.path.expanduser("~/.ssh/devin_key"); open(p,"w").write(out); os.chmod(p,0o600); print("ok")
EOF

# 1) chisel client（二进制在 /home/ubuntu/chisel；没有就下 linux amd64 of jpillora/chisel）
pkill -f "chisel client" 2>/dev/null; sleep 1
/home/ubuntu/chisel client https://t1dwvu6bzdng.vip3.xiaomiqiu123.top 2222:localhost:22 > /tmp/chisel.log 2>&1 &
sleep 6; grep -i connected /tmp/chisel.log     # 看到 "Connected (Latency ...)" 就通了

# 2) 验证 SSH 通
ssh -o IdentitiesOnly=yes -o StrictHostKeyChecking=no -i ~/.ssh/devin_key -p 2222 sans@localhost "echo OK; Get-PSDrive D | Out-String"
```
（隧道地址固定，一般不用问用户；只有连不上时才确认用户那边 chisel server / sshd 是否在跑。）

## 跑 PowerShell 脚本的标准姿势
```bash
# 永远：写成 .ps1 → scp 上去 → -File 跑（别用内联 -Command 带引号，会被 SSH 转义炸）
scp -o IdentitiesOnly=yes -o StrictHostKeyChecking=no -i ~/.ssh/devin_key -P 2222 \
    myscript.ps1 'sans@localhost:D:/claude-code/c++/routes/deep_protagonist/handoff/scripts/myscript.ps1'
ssh -o IdentitiesOnly=yes -o StrictHostKeyChecking=no -i ~/.ssh/devin_key -p 2222 sans@localhost \
    'powershell -NoProfile -ExecutionPolicy Bypass -File D:/claude-code/c++/routes/deep_protagonist/handoff/scripts/myscript.ps1'
```
注意 `scp` 用大写 `-P` 指端口，`ssh` 用小写 `-p`。
