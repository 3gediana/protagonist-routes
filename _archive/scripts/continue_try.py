# -*- coding: utf-8 -*-
"""
继续尝试各种方法获取 Y 值
"""
import sys, time, hashlib, calendar, re
sys.path.insert(0, r'D:\claude-code\acm_signin_http')
from pathlib import Path
from sztu_acm_signin_http import (
    http_login_for_code, exchange_tokens, auth_headers,
    HWSYS_API_BASE, fetch_signintask_catalog, simple_encode,
)
import requests

USERNAME = '202500205105'
PASSWORD = '3gedian.A'

def try_submit(session, hwsys_token, task_id, y_val):
    """Try a Y value and return result"""
    machine_hash = hashlib.md5(f"{USERNAME}_android_chrome120".encode("utf-8")).hexdigest()
    payload = {
        'userx_key': USERNAME,
        'signin_status': 10,
        'signintask_id': task_id,
        'server_now_validate': simple_encode(str(y_val)),
        'hash_info': simple_encode(machine_hash),
    }
    r = session.post(
        f'{HWSYS_API_BASE}/signinstu_do',
        headers=auth_headers(hwsys_token),
        json=payload,
        timeout=10,
    )
    return r.json() if r.text else {}

def get_server_time_via_date_header(url):
    """Get server time from HTTP Date header"""
    try:
        r = requests.get(url, verify=False, timeout=10)
        date_str = r.headers.get('Date')
        if date_str:
            from email.utils import parsedate_to_datetime
            dt = parsedate_to_datetime(date_str)
            return int(calendar.timegm(dt.timetuple()) * 1000)
    except Exception as e:
        print(f'  Date header error: {e}')
    return None

def get_server_time_via_api(session, hwsys_token):
    """Try to get server time from any API"""
    # Some APIs might return server timestamp
    try:
        r = session.get(f'{HWSYS_API_BASE}/time', headers=auth_headers(hwsys_token), timeout=10)
        if r.status_code == 200:
            data = r.json()
            print(f'  /time response: {data}')
    except:
        pass
    return None

# Main
session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

code = http_login_for_code(session, USERNAME, PASSWORD, Path(r'D:\claude-code\acm_signin_http\test_traces'))
token_result = exchange_tokens(session, code)
hwsys_token = token_result['hwsys_token']

task_catalog = fetch_signintask_catalog(session, hwsys_token)
task = task_catalog['data'][0]
task_id = task['id']

print(f'Task: {task.get("title")} | ID: {task_id}')
print(f'Current local time: {time.strftime("%Y-%m-%d %H:%M:%S")}')
print()

# === ATTEMPT 1: Server time from Date header ===
print('[1] Getting server time from HTTP Date header...')
server_time_ms = get_server_time_via_date_header('https://acm.sztu.edu.cn:40130/api/')
if server_time_ms:
    print(f'    Server time (from Date header): {server_time_ms}')
    local_ms = int(time.time() * 1000)
    print(f'    Local time: {local_ms}')
    print(f'    Diff: {server_time_ms - local_ms}ms')
    
    # Try with server time
    print(f'    Trying submit with server time...')
    result = try_submit(session, hwsys_token, task_id, server_time_ms)
    print(f'    Result: {result}')
    if result.get('status_code') == 200:
        print('*** SUCCESS with server time! ***')
        sys.exit(0)
else:
    print('    No Date header available')
print()

# === ATTEMPT 2: POST to now_validate ===
print('[2] Trying POST to now_validate...')
try:
    r = session.post(
        f'{HWSYS_API_BASE}/other/now_validate/{task_id}',
        headers=auth_headers(hwsys_token),
        timeout=10,
    )
    print(f'    Status: {r.status_code}')
    print(f'    Body: {r.text[:200]}')
except Exception as e:
    print(f'    Error: {e}')
print()

# === ATTEMPT 3: Check if there's a time endpoint ===
print('[3] Checking /time endpoint...')
try:
    r = session.get(f'{HWSYS_API_BASE}/time', headers=auth_headers(hwsys_token), timeout=10)
    print(f'    Status: {r.status_code}')
    print(f'    Body: {r.text[:200]}')
except Exception as e:
    print(f'    Error: {e}')
print()

# === ATTEMPT 4: Try different now_validate paths ===
print('[4] Trying different now_validate paths...')
paths = [
    f'{HWSYS_API_BASE}/now_validate/{task_id}',
    f'{HWSYS_API_BASE}/validate/{task_id}',
    f'{HWSYS_API_BASE}/other/validate/{task_id}',
    f'{HWSYS_API_BASE}/signin/now_validate/{task_id}',
    f'{HWSYS_API_BASE}/signintask/now_validate/{task_id}',
]
for url in paths:
    try:
        r = session.get(url, headers=auth_headers(hwsys_token), timeout=10)
        if r.status_code != 404:
            print(f'    {r.status_code} | {url}')
            print(f'         {r.text[:100]}')
    except Exception as e:
        pass
print()

# === ATTEMPT 5: Try using task creation time as base ===
print('[5] Trying task creation/updated time as Y base...')
created_at = task.get('created_at', '')
updated_at = task.get('updated_at', '')
print(f'    created_at: {created_at}')
print(f'    updated_at: {updated_at}')

# === ATTEMPT 6: Search response headers for clues ===
print('[6] Checking response headers from signinstu_do for clues...')
machine_hash = hashlib.md5(f"{USERNAME}_android_chrome120".encode("utf-8")).hexdigest()
payload = {
    'userx_key': USERNAME,
    'signin_status': 10,
    'signintask_id': task_id,
    'server_now_validate': simple_encode(str(int(time.time() * 1000))),
    'hash_info': simple_encode(machine_hash),
}
r = session.post(
    f'{HWSYS_API_BASE}/signinstu_do',
    headers=auth_headers(hwsys_token),
    json=payload,
    timeout=10,
)
print(f'    Response headers: {dict(r.headers)}')
print(f'    Response body: {r.text}')
print()

# === ATTEMPT 7: Try with y = empty or specific values ===
print('[7] Trying edge cases...')
edge_cases = ['', '0', '1', 'null', 'undefined', 'test']
for y in edge_cases:
    result = try_submit(session, hwsys_token, task_id, y)
    print(f'    Y="{y}" -> {result}')

print()
print('All attempts completed.')
