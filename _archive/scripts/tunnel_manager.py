# -*- coding: utf-8 -*-
"""
SSH 反向隧道管理器 + 签到调度系统
"""
import time, json, os, sys, subprocess, threading, signal
from pathlib import Path

VPS_HOST = "143.198.94.163"
VPS_USER = "root"
REMOTE_PORT = 18080
LOCAL_HOST = "10.108.3.199"  # acm.sztu.edu.cn 的内网 IP
LOCAL_PORT = 40130

TUNNEL_CHECK_INTERVAL = 30  # seconds


def ensure_tunnel_alive():
    """Check if SSH reverse tunnel is alive, restart if not."""
    # Check if ssh.exe is running with correct arguments
    try:
        result = subprocess.run(
            ["powershell", "-Command",
             "Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'ssh.exe'} | Select-Object CommandLine"],
            capture_output=True, text=True, timeout=5
        )
        output = result.stdout
        if "18080" in output and VPS_HOST in output:
            print(f"[TUNNEL] SSH tunnel already running")
            return True
    except Exception as e:
        print(f"[TUNNEL] Check failed: {e}")

    # Kill any existing ssh.exe
    try:
        subprocess.run(["taskkill", "/F", "/IM", "ssh.exe"], capture_output=True, timeout=5)
    except Exception:
        pass

    # Start new tunnel using Python sshtunnel
    print(f"[TUNNEL] Starting new SSH reverse tunnel...")
    try:
        from sshtunnel import SSHTunnelForwarder
        server = SSHTunnelForwarder(
            (VPS_HOST, 22),
            ssh_username=VPS_USER,
            ssh_pkey=str(Path.home() / ".ssh" / "id_ed25519"),
            remote_bind_address=(LOCAL_HOST, LOCAL_PORT),
            local_bind_address=("127.0.0.1", 0),  # auto-assign local port
        )
        server.start()
        print(f"[TUNNEL] Started on local port {server.local_bind_port}")
        return server
    except Exception as e:
        print(f"[TUNNEL] Failed to start: {e}")
        return None


if __name__ == "__main__":
    server = ensure_tunnel_alive()
    if server:
        print("Tunnel is running. Press Ctrl+C to stop.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            server.stop()
            print("Tunnel stopped.")
    else:
        print("Failed to start tunnel.")
        sys.exit(1)
