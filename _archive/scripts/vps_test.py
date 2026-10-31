# -*- coding: utf-8 -*-
import time, requests
requests.packages.urllib3.disable_warnings()

print("=== VPS -> acm.sztu.edu.cn ===")
for i in range(3):
    t0 = time.time()
    try:
        r = requests.get("https://acm.sztu.edu.cn:40130/api/signin", verify=False, timeout=10)
        t1 = time.time()
        print(f"  {i+1}: {(t1-t0)*1000:.0f}ms (status={r.status_code})")
    except Exception as e:
        print(f"  {i+1}: ERROR {e}")
    time.sleep(0.5)

print("\n=== VPS login chain ===")
import sys
sys.path.insert(0, "/opt/acm-signin")
from signin_core import http_login_for_code, exchange_tokens, fetch_signintask_catalog

session = requests.Session()
session.verify = False

t0 = time.time()
code = http_login_for_code(session, "202500205105", "3gedian.A")
t1 = time.time()
print(f"  Login: {(t1-t0)*1000:.0f}ms")

t0 = time.time()
tokens = exchange_tokens(session, code)
t1 = time.time()
print(f"  Exchange: {(t1-t0)*1000:.0f}ms")

t0 = time.time()
catalog = fetch_signintask_catalog(session, tokens["hwsys_token"])
t1 = time.time()
print(f"  Fetch tasks: {(t1-t0)*1000:.0f}ms")
