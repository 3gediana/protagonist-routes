import json, urllib.request, time, os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
cfg = json.load(open("agent_config.json", encoding="utf-8"))
pid = cfg.get("active_provider")
p = [x for x in cfg.get("providers", []) if x.get("id") == pid][0]
print("provider=", p.get("name"), "model=", p.get("model"), "base=", p.get("base_url"))
body = json.dumps({"model": p["model"],
                   "messages": [{"role": "user", "content": "say OK"}],
                   "max_tokens": 5}).encode()
req = urllib.request.Request(p["base_url"].rstrip("/") + "/chat/completions",
                             data=body,
                             headers={"Authorization": "Bearer " + p["api_key"],
                                      "Content-Type": "application/json"})
t = time.time()
try:
    r = urllib.request.urlopen(req, timeout=40)
    d = json.loads(r.read())
    msg = d.get("choices", [{}])[0].get("message", {}).get("content")
    print("OK", round(time.time() - t, 1), "s ->", repr(msg))
except urllib.error.HTTPError as e:
    print("HTTP_ERR", e.code, round(time.time() - t, 1), "s ->", e.read().decode(errors="ignore")[:300])
except Exception as e:
    print("PROBE_ERR", round(time.time() - t, 1), "s ->", repr(e)[:200])
