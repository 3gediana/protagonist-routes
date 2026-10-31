# -*- coding: utf-8 -*-
import sys, time, requests, hashlib
sys.path.insert(0, r'D:\claude-code\acm_signin_http')
from pathlib import Path
from sztu_acm_signin_http import (
    http_login_for_code, exchange_tokens, auth_headers,
    HWSYS_API_BASE, fetch_signintask_catalog, simple_encode,
)

USERNAME = '202500205105'
PASSWORD = '3gedian.A'

session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

code = http_login_for_code(session, USERNAME, PASSWORD, Path(r'D:\claude-code\acm_signin_http\test_traces'))
token_result = exchange_tokens(session, code)
hwsys_token = token_result['hwsys_token']

task_catalog = fetch_signintask_catalog(session, hwsys_token)
task = task_catalog['data'][0]
signintask_id = task['id']

utc_ms = int(time.time() * 1000)
beijing_ms = utc_ms + 8 * 3600 * 1000

candidates = [utc_ms, beijing_ms]
for base in [0, 5000, 10000, 15000, 20000, 25000]:
    for ref in [utc_ms, beijing_ms]:
        aligned = (ref // 4500) * 4500 + base
        candidates.extend([aligned - 4500, aligned, aligned + 4500])

candidates = sorted(set(candidates))

print(f'Testing {len(candidates)} candidates...')
for ts in candidates:
    machine_hash = hashlib.md5(f"{USERNAME}_android_chrome120".encode("utf-8")).hexdigest()
    payload = {
        'userx_key': USERNAME,
        'signin_status': 10,
        'signintask_id': signintask_id,
        'server_now_validate': simple_encode(str(ts)),
        'hash_info': simple_encode(machine_hash),
    }
    r = session.post(f'{HWSYS_API_BASE}/signinstu_do', headers=auth_headers(hwsys_token), json=payload, timeout=10)
    result = r.json() if r.text else {}
    status = result.get('status_code')
    if status == 200:
        print(f'SUCCESS with ts={ts}')
        print(f'  Y = {simple_encode(str(ts))}')
        print(f'  Detail: {result.get("detail", "")}')
        break
    elif status != 406:
        print(f'Unexpected status {status}: {result}')
else:
    print('All failed.')
    print('Last error:', result)
