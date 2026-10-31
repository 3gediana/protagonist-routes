# -*- coding: utf-8 -*-
"""
SSH Reverse Tunnel using system SSH client (Windows compatible)
"""
import subprocess, time, sys, os
from pathlib import Path

VPS_HOST = "143.198.94.163"
VPS_USER = "root"
REMOTE_PORT = 18080
TARGET_HOST = "10.108.3.199"
TARGET_PORT = 40130
SSH_KEY = str(Path.home() / ".ssh" / "id_ed25519")


def start_tunnel():
    """Start SSH reverse tunnel without -f flag."""
    # Kill any existing ssh.exe first
    try:
        subprocess.run(["taskkill", "/F", "/IM", "ssh.exe"], capture_output=True, timeout=5)
        time.sleep(1)
    except Exception:
        pass
    
    cmd = [
        "ssh", "-N",
        "-o", "ConnectTimeout=5",
        "-o", "ServerAliveInterval=30",
        "-o", "ServerAliveCountMax=3",
        "-o", "ExitOnForwardFailure=yes",
        "-R", f"{REMOTE_PORT}:{TARGET_HOST}:{TARGET_PORT}",
        "-i", SSH_KEY,
        f"{VPS_USER}@{VPS_HOST}"
    ]
    print(f"[TUNNEL] Starting: {' '.join(cmd)}")
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        creationflags=subprocess.CREATE_NO_WINDOW
    )
    return proc


def verify_tunnel():
    """Verify tunnel by running a test on VPS."""
    print("[TUNNEL] Verifying tunnel...")
    import paramiko
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    pkey = paramiko.Ed25519Key.from_private_key_file(SSH_KEY)
    ssh.connect(VPS_HOST, username=VPS_USER, pkey=pkey, timeout=10)
    
    # Test with correct Host header
    test_script = '''
import urllib.request
req = urllib.request.Request("http://127.0.0.1:18080/api/signin")
req.add_header("Host", "acm.sztu.edu.cn:40130")
try:
    r = urllib.request.urlopen(req, timeout=5)
    data = r.read()
    print("HTTP OK:", r.status, len(data))
except Exception as e:
    print("HTTP ERROR:", type(e).__name__, e)
'''
    stdin, stdout, stderr = ssh.exec_command(f'python3 -c "{test_script}"')
    out = stdout.read().decode().strip()
    err = stderr.read().decode().strip()
    ssh.close()
    
    print(f"[TUNNEL] VPS output: {out}")
    if err:
        print(f"[TUNNEL] VPS stderr: {err}")
    
    return "HTTP OK" in out


if __name__ == "__main__":
    print("=" * 60)
    print("SSH Reverse Tunnel - System SSH Client")
    print("=" * 60)
    
    proc = start_tunnel()
    print(f"[TUNNEL] Process started, PID={proc.pid}")
    
    # Wait for tunnel to establish
    time.sleep(3)
    
    # Verify
    for attempt in range(3):
        if verify_tunnel():
            print("[TUNNEL] ✅ Tunnel is working!")
            print("\nTunnel is running. Press Ctrl+C to stop.")
            try:
                while True:
                    time.sleep(1)
            except KeyboardInterrupt:
                proc.terminate()
                print("\n[TUNNEL] Stopped.")
            break
        else:
            print(f"[TUNNEL] Attempt {attempt + 1}/3 failed, retrying...")
            time.sleep(2)
    else:
        print("[TUNNEL] ❌ Tunnel failed.")
        proc.terminate()
        sys.exit(1)
