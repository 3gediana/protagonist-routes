# -*- coding: utf-8 -*-
import requests, re
requests.packages.urllib3.disable_warnings()

r = requests.get('https://acm.sztu.edu.cn/', verify=False, timeout=10)
print('Status:', r.status_code)
text = r.text

# Search for any JS files or API references
js_files = re.findall(r'src=["\']([^"\']+\.js)["\']', text)
print('JS files found:', js_files[:10])

# Search for any URLs containing api or validate
api_refs = re.findall(r'https?://[^\s"\'<>]+', text)
for ref in api_refs[:20]:
    if 'api' in ref or 'validate' in ref or 'signin' in ref:
        print('API ref:', ref)
