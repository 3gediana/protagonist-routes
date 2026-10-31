import urllib.request, socket

# Check if port 18080 is listening
try:
    s = socket.socket()
    s.settimeout(2)
    s.connect(('127.0.0.1', 18080))
    s.close()
    print("PORT 18080: LISTENING")
except Exception as e:
    print("PORT 18080: NOT LISTENING -", e)
    exit(1)

# Test HTTP request through tunnel
try:
    r = urllib.request.urlopen('http://127.0.0.1:18080/api/signin', timeout=5)
    data = r.read()
    print("HTTP STATUS:", r.status)
    print("RESPONSE LEN:", len(data))
    print("FIRST 100:", data[:100].decode('utf-8', errors='replace'))
    print("TUNNEL OK")
except Exception as e:
    print("HTTP ERROR:", e)
    print("TUNNEL FAILED")
