# -*- coding: utf-8 -*-
import requests
import time
import re
from urllib.parse import urlencode, parse_qs, urlparse

USERNAME = "202500205105"
PASSWORD = "3gedian.A"

HWSYS_API_BASE = "https://acm.sztu.edu.cn:40130/api"
AUTH_URL = "https://auth.sztu.edu.cn"

session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

def simple_encode(value):
    value = str(value).upper()
    alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    digits = "0123456789"
    out = "HW" + value
    shift = len(out) % 26
    result = []
    for ch in out:
        if ch in alpha:
            result.append(alpha[(alpha.index(ch) + shift) % 26])
        elif ch in digits:
            result.append(digits[(digits.index(ch) + shift) % 10])
        else:
            result.append(ch)
    return "".join(result)

try:
    # Step 1: Get login page
    print("[1] Getting auth page...")
    r = session.get(f"{AUTH_URL}/authserver/login", timeout=15)
    lt_match = re.search(r'name="lt" value="([^"]+)"', r.text)
    execution_match = re.search(r'name="execution" value="([^"]+)"', r.text)
    
    if not lt_match or not execution_match:
        print("Failed to get lt/execution tokens")
        exit(1)
    
    lt = lt_match.group(1)
    execution = execution_match.group(1)
    print(f"    lt={lt[:20]}..., execution={execution[:20]}...")
    
    # Step 2: Login
    print("[2] Posting login...")
    login_data = {
        "username": USERNAME,
        "password": PASSWORD,
        "lt": lt,
        "execution": execution,
        "_eventId": "submit",
        "rmShown": "1"
    }
    r = session.post(f"{AUTH_URL}/authserver/login", data=login_data, timeout=15, allow_redirects=True)
    print(f"    Status: {r.status_code}, URL: {r.url[:80]}")
    
    # Check for code in URL
    parsed = urlparse(r.url)
    code = parse_qs(parsed.query).get('code', [None])[0]
    
    if not code:
        # Try to find code in response
        code_match = re.search(r'code=([^&\s]+)', r.text)
        if code_match:
            code = code_match.group(1)
    
    if not code:
        print("Failed to get auth code")
        print(f"Response preview: {r.text[:500]}")
        exit(1)
    
    print(f"    Got code: {code[:20]}...")
    
    # Step 3: Exchange for hwsys token
    print("[3] Exchanging for hwsys token...")
    hwsys_token_url = f"{HWSYS_API_BASE}/sso_callback"
    r = session.get(hwsys_token_url, params={"code": code}, timeout=15)
    print(f"    Status: {r.status_code}")
    hwsys_data = r.json() if r.text else {}
    hwsys_token = hwsys_data.get("data", {}).get("token") if isinstance(hwsys_data, dict) else None
    
    if not hwsys_token:
        print(f"Failed to get hwsys token. Response: {r.text[:500]}")
        exit(1)
    
    print(f"    Got hwsys_token: {hwsys_token[:20]}...")
    
    # Step 4: Get signin tasks
    print("[4] Getting signin tasks...")
    session.headers.update({"Authorization": f"Bearer {hwsys_token}"})
    r = session.get(f"{HWSYS_API_BASE}/signin", timeout=15)
    print(f"    Status: {r.status_code}")
    tasks = r.json() if r.text else {}
    
    if not tasks or not tasks.get("data"):
        print(f"No tasks found. Response: {r.text[:500]}")
        exit(1)
    
    task = tasks["data"][0]
    task_id = task.get("id")
    print(f"    Task ID: {task_id}")
    
    # Step 5: Get now_validate (Y value)
    print("[5] Getting now_validate (Y value)...")
    r = session.get(f"{HWSYS_API_BASE}/signintask/{task_id}", timeout=15)
    print(f"    Status: {r.status_code}")
    wall_data = r.json() if r.text else {}
    
    y_raw = None
    if wall_data and "data" in wall_data:
        y_raw = wall_data["data"].get("server_now_validate")
    
    if not y_raw:
        # Try direct now_validate endpoint
        r2 = session.get(f"{HWSYS_API_BASE}/other/now_validate/{task_id}", timeout=15)
        print(f"    Alt endpoint status: {r2.status_code}")
        if r2.text:
            alt_data = r2.json() if r2.text.startswith("{") else {}
            if isinstance(alt_data, dict):
                y_raw = alt_data.get("server_now_validate") or alt_data.get("data", {}).get("server_now_validate")
    
    if not y_raw:
        print("Failed to get Y value")
        exit(1)
    
    print(f"    Raw Y value: {y_raw}")
    
    # Step 6: Encode Y
    print("[6] Encoding Y with Simple-Encode...")
    y_encoded = simple_encode(y_raw)
    print(f"    Encoded Y: {y_encoded}")
    
    # Step 7: Submit signin
    print("[7] Submitting signin...")
    machine_hash = simple_encode(f"{USERNAME}_android_chrome120")
    payload = {
        "userx_key": USERNAME,
        "signin_status": 10,
        "signintask_id": task_id,
        "server_now_validate": y_encoded,
        "hash_info": machine_hash
    }
    r = session.post(f"{HWSYS_API_BASE}/signinstu_do", json=payload, timeout=15)
    print(f"    Status: {r.status_code}")
    print(f"    Response: {r.text}")
    
except Exception as e:
    print(f"Error: {type(e).__name__}: {e}")
    import traceback
    traceback.print_exc()
