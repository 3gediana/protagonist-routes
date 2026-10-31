# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, r'D:\claude-code\acm_signin_http')

import requests, time, hashlib
from pathlib import Path

from sztu_acm_signin_http import (
    http_login_for_code,
    exchange_tokens,
    fetch_signintask_catalog,
    build_signin_payload,
    submit_signin,
    HWSYS_API_BASE,
    auth_headers,
)

USERNAME = "202500205105"
PASSWORD = "3gedian.A"

session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

trace_dir = Path(r'D:\claude-code\acm_signin_http\test_traces')

print("[1] Logging in...")
code = http_login_for_code(session, USERNAME, PASSWORD, trace_dir)
print(f"    Got code: {code[:30]}...")

print("[2] Exchanging tokens...")
token_result = exchange_tokens(session, code)
hwsys_token = token_result["hwsys_token"]
print(f"    Got hwsys_token: {hwsys_token[:30]}...")

print("[3] Fetching task catalog...")
task_catalog = fetch_signintask_catalog(session, hwsys_token)
if not task_catalog or not task_catalog.get("data"):
    print("    No tasks available")
    sys.exit(1)

task = task_catalog["data"][0]
signintask_id = task["id"]
print(f"    Task: {task.get('title')} | ID: {signintask_id}")

print("[4] Building payload with LOCAL timestamp Y...")
payload = build_signin_payload(USERNAME, signintask_id)
print(f"    Y (raw timestamp): {payload['server_now_validate']}")
print(f"    Hash: {payload['hash_info']}")

print("[5] Submitting...")
result = submit_signin(session, hwsys_token, payload)
print(f"    Result: {result}")

if result and result.get("status_code") == 200:
    print("\n*** SUCCESS ***")
else:
    print(f"\n*** FAILED: {result} ***")
