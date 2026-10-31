# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, r'D:\claude-code\acm_signin_http')
from pathlib import Path
from sztu_acm_signin_http import (
    http_login_for_code, exchange_tokens,
    fetch_signintask_catalog, fetch_signin_catalog,
    discover_tasks_for_user,
)
import requests

USERNAME = '202500205105'
PASSWORD = '3gedian.A'

session = requests.Session()
session.verify = False
requests.packages.urllib3.disable_warnings()

code = http_login_for_code(session, USERNAME, PASSWORD, Path(r'D:\claude-code\acm_signin_http\test_traces'))
token_result = exchange_tokens(session, code)
hwsys_token = token_result['hwsys_token']

print('=== Signintask Catalog (all tasks) ===')
task_catalog = fetch_signintask_catalog(session, hwsys_token)
for i, t in enumerate(task_catalog.get('data', [])):
    print(f'{i}: {t.get("title")} | ID: {t.get("id")}')

print('\n=== Discover tasks for user ===')
discovered = discover_tasks_for_user(session, hwsys_token, USERNAME)
for i, t in enumerate(discovered):
    print(f'{i}: {t.get("title")} | ID: {t.get("id")}')
