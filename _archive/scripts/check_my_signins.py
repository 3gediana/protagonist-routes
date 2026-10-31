# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, r'D:\claude-code\acm_signin_http')
from pathlib import Path
from sztu_acm_signin_http import http_login_for_code, exchange_tokens, fetch_signin_catalog, fetch_signintask_catalog
import requests, datetime

USERNAME = '202500205105'
PASSWORD = '3gedian.A'

session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

code = http_login_for_code(session, USERNAME, PASSWORD, Path(r'D:\claude-code\acm_signin_http\test_traces'))
token_result = exchange_tokens(session, code)
hwsys_token = token_result['hwsys_token']

print('=== My Signins (/api/signin) ===')
signin_catalog = fetch_signin_catalog(session, hwsys_token)
for i, s in enumerate(signin_catalog.get('data', [])[:10]):
    title = s.get('title', '')
    teacher = s.get('teacher', '')
    white_list = s.get('white_list', [])
    in_white = USERNAME in str(white_list)
    clss = s.get('clss', '')
    print(f'[{i}] {title}')
    print(f'    teacher: {teacher}')
    print(f'    white_list count: {len(white_list)}')
    print(f'    I am in white_list: {in_white}')
    print(f'    class: {clss}')
    print()

print('\n=== My Signintasks (/api/signintask) that match my white_list ===')
task_catalog = fetch_signintask_catalog(session, hwsys_token)
now = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8)))

# Collect my signin IDs
my_signin_ids = [s.get('id') for s in signin_catalog.get('data', [])]

matched = []
for t in task_catalog.get('data', []):
    signin_id = t.get('signin_id')
    if signin_id in my_signin_ids:
        start_str = t.get('time_start', '')
        end_str = t.get('time_end', '')
        is_active = False
        if start_str and end_str:
            try:
                st = datetime.datetime.fromisoformat(start_str.replace('Z', '+00:00'))
                et = datetime.datetime.fromisoformat(end_str.replace('Z', '+00:00'))
                is_active = st <= now <= et
            except:
                pass
        matched.append((t, is_active))

print(f'Found {len(matched)} tasks linked to my signins\n')
for i, (t, active) in enumerate(matched[:10]):
    title = t.get('title', '')
    task_id = t.get('id', '')
    status = 'ACTIVE NOW' if active else 'not active'
    print(f'[{i}] {title} | {status}')
    print(f'    task_id: {task_id}')
    print(f'    signin_id: {t.get("signin_id")}')
    print()
