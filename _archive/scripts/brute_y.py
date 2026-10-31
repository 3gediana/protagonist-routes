# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, r'D:\claude-code\acm_signin_http')

import requests, time
from pathlib import Path

from sztu_acm_signin_http import (
    http_login_for_code,
    exchange_tokens,
    fetch_signintask_catalog,
    auth_headers,
    HWSYS_API_BASE,
    simple_encode,
)

USERNAME = "202500205105"
PASSWORD = "3gedian.A"

session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

trace_dir = Path(r'D:\claude-code\acm_signin_http\test_traces')

print("[1] Logging in...")
code = http_login_for_code(session, USERNAME, PASSWORD, trace_dir)
token_result = exchange_tokens(session, code)
hwsys_token = token_result["hwsys_token"]

print("[2] Fetching task catalog...")
task_catalog = fetch_signintask_catalog(session, hwsys_token)
task = task_catalog["data"][0]
signintask_id = task["id"]
print(f"    Task: {task.get('title')} | ID: {signintask_id}")

# Build a payload with a given timestamp
def build_payload_with_timestamp(ts_ms):
    machine_hash = __import__('hashlib').md5(f"{USERNAME}_android_chrome120".encode("utf-8")).hexdigest()
    return {
        "userx_key": USERNAME,
        "signin_status": 10,
        "signintask_id": signintask_id,
        "server_now_validate": simple_encode(str(ts_ms)),
        "hash_info": simple_encode(machine_hash),
    }

# Brute force around current time
now_ms = int(time.time() * 1000)
print(f"\n[3] Brute forcing Y values around {now_ms}...")
print(f"    Current time: {time.strftime('%H:%M:%S')}")

# Try window: ±30 seconds in 100ms steps, and also ±5 minutes in 1s steps
ranges = []
# Fine search around now
for offset in range(-30000, 30001, 100):
    ranges.append(now_ms + offset)
# Also try aligned to 4.5s intervals from various baselines
for base in [0, 5000, 10000, 15000, 20000]:
    t = (now_ms // 4500) * 4500 + base
    ranges.extend([t - 4500, t, t + 4500, t + 9000])

# Remove duplicates and sort
ranges = sorted(set(ranges))

print(f"    Testing {len(ranges)} timestamp candidates...")

found = False
for i, ts in enumerate(ranges):
    payload = build_payload_with_timestamp(ts)
    resp = session.post(
        f"{HWSYS_API_BASE}/signinstu_do",
        headers=auth_headers(hwsys_token),
        json=payload,
        timeout=10,
    )
    result = resp.json() if resp.text else {}
    status = result.get("status_code", 0)
    detail = result.get("detail", "")
    
    if status == 200:
        print(f"\n*** FOUND VALID Y at ts={ts} ***")
        print(f"    Y = {simple_encode(str(ts))}")
        print(f"    Detail: {detail}")
        found = True
        break
    elif status != 406:
        print(f"\n    Unexpected status {status}: {detail}")
    
    if i % 100 == 0:
        print(f"    Tested {i}/{len(ranges)}...")
    
    time.sleep(0.05)  # Rate limit

if not found:
    print("\n*** No valid Y found in search range ***")
    print("    This likely means the QR code is not active, or the timestamp logic is different.")
