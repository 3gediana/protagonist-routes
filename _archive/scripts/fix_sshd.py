# -*- coding: utf-8 -*-
import subprocess, sys

VPS_HOST = "143.198.94.163"
VPS_USER = "root"

# Create remote script
remote_script = """#!/bin/bash
echo "AllowTcpForwarding yes" >> /etc/ssh/sshd_config
systemctl reload sshd
echo "SSH config reloaded with AllowTcpForwarding=yes"
"""

with open("D:/fix_sshd_remote.sh", "w") as f:
    f.write(remote_script)

# SCP to VPS
scp_result = subprocess.run(
    ["scp", "D:/fix_sshd_remote.sh", f"{VPS_USER}@{VPS_HOST}:/tmp/fix_sshd.sh"],
    capture_output=True, text=True
)
print("SCP stdout:", scp_result.stdout)
print("SCP stderr:", scp_result.stderr)

# Execute on VPS
ssh_result = subprocess.run(
    ["ssh", f"{VPS_USER}@{VPS_HOST}", "bash", "/tmp/fix_sshd.sh"],
    capture_output=True, text=True
)
print("SSH stdout:", ssh_result.stdout)
print("SSH stderr:", ssh_result.stderr)
