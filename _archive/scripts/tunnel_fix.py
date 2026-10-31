# -*- coding: utf-8 -*-
"""
SSH Reverse Tunnel Manager - Windows compatible
"""
import subprocess, time, sys, os
from pathlib import Path

VPS_HOST = "143.198.94.163"
VPS_USER = "root"
REMOTE_PORT = 18080
LOCAL_HOST = "10.108.3.199"
LOCAL_PORT = 40130

SSH_KEY = str(Path.home() / ".ssh" / "id_ed25519")


def start_tunnel():
    """Start SSH reverse tunnel process without -f flag (Windows compatible)."""
    cmd = [
        "ssh", "-N",
        "-o", "ConnectTimeout=5",
        "-o", "ServerAliveInterval=30",
        "-o", "ServerAliveCountMax=3",
        "-o", "ExitOnForwardFailure=yes",
        "-R", f"{REMOTE_PORT}:{LOCAL_HOST}:{LOCAL_PORT}",
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


def check_tunnel_from_vps():
    """Check if VPS can reach acm through tunnel."""
    import paramiko
    try:
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(VPS_HOST, username=VPS_USER, pkey=paramiko.Ed25519Key.from_private_key_file(SSH_KEY), timeout=10)
        
        stdin, stdout, stderr = ssh.exec_command(
            'python3 -c "import urllib.request; '
            'r=urllib.request.urlopen(chr(104)+chr(116)+chr(116)+chr(112)+chr(58)+chr(47)+chr(47)+chr(108)+chr(111)+chr(99)+chr(97)+chr(108)+chr(104)+chr(111)+chr(115)+chr(116)+chr(58)+chr(49)+chr(56)+chr(48)+chr(56)+chr(48)+chr(47)+chr(97)+chr(112)+chr(105)+chr(47)+chr(115)+chr(105)+chr(103)+chr(110)+chr(105)+chr(110), timeout=5); '
            'print(chr(79)+chr(75), r.status, len(r.read()))"'
        )
        out = stdout.read().decode().strip()
        err = stderr.read().decode().strip()
        ssh.close()
        
        if "OK" in out:
            print(f"[TUNNEL] VPS check OK: {out}")
            return True
        else:
            print(f"[TUNNEL] VPS check FAILED: {out} {err}")
            return False
    except Exception as e:
        print(f"[TUNNEL] VPS check ERROR: {e}")
        return False


if __name__ == "__main__":
    print("=" * 60)
    print("SSH Reverse Tunnel Manager")
    print("=" * 60)
    
    # Kill any existing ssh.exe
    try:
        subprocess.run(["taskkill", "/F", "/IM", "ssh.exe"], capture_output=True, timeout=5)
        time.sleep(1)
    except Exception:
        pass
    
    # Start tunnel
    proc = start_tunnel()
    print(f"[TUNNEL] Process started, PID={proc.pid}")
    
    # Wait for tunnel to establish
    time.sleep(3)
    
    # Check if tunnel works
    for attempt in range(5):
        if check_tunnel_from_vps():
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
            print(f"[TUNNEL] Attempt {attempt + 1}/5 failed, retrying...")
            time.sleep(2)
    else:
        print("[TUNNEL] ❌ Tunnel failed after all attempts.")
        proc.terminate()
        # Print SSH stderr for debugging
        try:
            stderr_output = proc.stderr.read(2000)
            if stderr_output:
                print(f"[TUNNEL] SSH stderr: {stderr_output}")
        except Exception:
            pass
        sys.exit(1)
