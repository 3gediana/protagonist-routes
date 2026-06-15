import json, urllib.request

url = "https://api.xiaomimimo.com/v1/chat/completions"
[REDACTED LOCAL SECRET]

# Test 1: Small body
body1 = {"model": "mimo-v2.5", "messages": [{"role": "user", "content": "say ok"}],
         "max_tokens": 10, "stream": True, "stream_options": {"include_usage": True}}
data1 = json.dumps(body1).encode("utf-8")
print(f"Test 1: {len(data1)} bytes")
req1 = urllib.request.Request(url, data=data1, method="POST",
    headers={"Content-Type": "application/json", "Authorization": "Bearer " + api_key})
try:
    with urllib.request.urlopen(req1, timeout=30) as resp:
        for raw in resp:
            line = raw.decode("utf-8","replace").strip()
            if "DONE" in line:
                print("  -> OK")
                break
except urllib.error.HTTPError as e:
    print(f"  -> HTTP {e.code}: {e.read().decode()[:200]}")
except Exception as e:
    print(f"  -> Error: {e}")

# Test 2: Large body (similar to agent baseline ~700KB)
big = "x" * 500000
body2 = {"model": "mimo-v2.5", "messages": [
    {"role": "system", "content": big},
    {"role": "user", "content": "say ok"}
], "max_tokens": 10, "stream": True, "stream_options": {"include_usage": True}}
data2 = json.dumps(body2).encode("utf-8")
print(f"Test 2: {len(data2)/1024:.0f} KB")
req2 = urllib.request.Request(url, data=data2, method="POST",
    headers={"Content-Type": "application/json", "Authorization": "Bearer " + api_key})
try:
    with urllib.request.urlopen(req2, timeout=120) as resp:
        for raw in resp:
            line = raw.decode("utf-8","replace").strip()
            if "DONE" in line:
                print("  -> OK")
                break
except urllib.error.HTTPError as e:
    print(f"  -> HTTP {e.code}: {e.read().decode()[:200]}")
except Exception as e:
    print(f"  -> Error: {e}")
