# -*- coding: utf-8 -*-
import sys, requests, re, json
sys.path.insert(0, r'D:\claude-code\acm_signin_http')
from pathlib import Path
from sztu_acm_signin_http import (
    http_login_for_code, exchange_tokens, auth_headers,
    HWSYS_API_BASE, fetch_signin_catalog, fetch_signintask_catalog,
    simple_encode, build_signin_payload, submit_signin,
)

USERNAME = '202500205105'
PASSWORD = '3gedian.A'

session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

code = http_login_for_code(session, USERNAME, PASSWORD, Path(r'D:\claude-code\acm_signin_http\test_traces'))
token_result = exchange_tokens(session, code)
hwsys_token = token_result['hwsys_token']

# Get all tasks
print('=== Signintask Catalog ===')
task_catalog = fetch_signintask_catalog(session, hwsys_token)
for t in task_catalog.get('data', [])[:3]:
    print('ID:', t.get('id'))
    print('Title:', t.get('title'))
    print('SigninID:', t.get('signin_id'))
    print('Keys:', list(t.keys()))
    print('---')

signintask_id = task_catalog['data'][0]['id']
signin_id = task_catalog['data'][0]['signin_id']

print('\n=== Trying various endpoints for Y value ===')
print('Task ID:', signintask_id)

endpoints = [
    (HWSYS_API_BASE + '/signintask/' + signintask_id, True),
    (HWSYS_API_BASE + '/other/now_validate/' + signintask_id, True),
    (HWSYS_API_BASE + '/signin/' + signin_id, True),
    (HWSYS_API_BASE + '/signinstu_all_self?signintask_id=' + signintask_id, True),
]

for url, needs_auth in endpoints:
    try:
        headers = auth_headers(hwsys_token) if needs_auth else {}
        r = session.get(url, headers=headers, timeout=15)
        print('\n' + url)
        print('  Status:', r.status_code)
        if r.text and len(r.text) < 2000:
            print('  Body:', r.text[:800])
        else:
            print('  Body length:', len(r.text))
    except Exception as e:
        print('\n' + url + ' -> Error:', e)

# Also try public pages without auth
print('\n=== Public pages (no auth) ===')
public_urls = [
    'https://acm.sztu.edu.cn/signinop?tid=' + signintask_id,
    'https://acm.sztu.edu.cn:40130/signinop?tid=' + signintask_id,
]
for url in public_urls:
    try:
        r = session.get(url, timeout=15)
        print('\n' + url)
        print('  Status:', r.status_code)
        print('  Body first 500:', r.text[:500])
    except Exception as e:
        print('\n' + url + ' -> Error:', e)
