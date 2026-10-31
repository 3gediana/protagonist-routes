# -*- coding: utf-8 -*-
"""
SSH Reverse Tunnel using paramiko - with DEBUG logging
"""
import paramiko, socket, select, threading, time, sys, logging
from pathlib import Path

# Enable DEBUG logging
paramiko.common.logging.basicConfig(level=paramiko.common.DEBUG)

VPS_HOST = "143.198.94.163"
VPS_USER = "root"
REMOTE_PORT = 18080
TARGET_HOST = "10.108.3.199"
TARGET_PORT = 40130
SSH_KEY = str(Path.home() / ".ssh" / "id_ed25519")


def forward_tunnel(channel, target_host, target_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((target_host, target_port))
        print(f"[TUNNEL] Connected to {target_host}:{target_port} for channel {channel.get_id()}")
    except Exception as e:
        print(f"[TUNNEL] Failed to connect to {target_host}:{target_port}: {e}")
        channel.close()
        return

    while True:
        r, w, x = select.select([sock, channel], [], [], 0.1)
        if sock in r:
            data = sock.recv(16384)
            if len(data) == 0:
                break
            channel.send(data)
        if channel in r:
            data = channel.recv(16384)
            if len(data) == 0:
                break
            sock.send(data)
    
    channel.close()
    sock.close()
    print(f"[TUNNEL] Channel {channel.get_id()} closed")


def start_tunnel():
    print(f"[TUNNEL] Connecting to {VPS_HOST}...")
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    
    pkey = paramiko.Ed25519Key.from_private_key_file(SSH_KEY)
    ssh.connect(VPS_HOST, username=VPS_USER, pkey=pkey, timeout=10)
    
    transport = ssh.get_transport()
    print(f"[TUNNEL] SSH connected, requesting port forward on {REMOTE_PORT}")
    
    transport.request_port_forward("", REMOTE_PORT)
    print(f"[TUNNEL] Port forward requested: remote:{REMOTE_PORT} -> local:{TARGET_HOST}:{TARGET_PORT}")
    
    print("[TUNNEL] Waiting for connections...")
    while True:
        channel = transport.accept(1000)
        if channel is None:
            continue
        print(f"[TUNNEL] New channel accepted: {channel.get_id()}")
        threading.Thread(
            target=forward_tunnel,
            args=(channel, TARGET_HOST, TARGET_PORT),
            daemon=True
        ).start()


if __name__ == "__main__":
    try:
        start_tunnel()
    except KeyboardInterrupt:
        print("\n[TUNNEL] Stopped by user.")
    except Exception as e:
        print(f"[TUNNEL] Error: {e}")
        import traceback
        traceback.print_exc()
