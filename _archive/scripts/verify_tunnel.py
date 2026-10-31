# -*- coding: utf-8 -*-
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('143.198.94.163', username='root', timeout=10)

# Test tunnel
stdin, stdout, stderr = ssh.exec_command(
    'python3 -c "import urllib.request; '
    'r=urllib.request.urlopen(chr(104)+chr(116)+chr(116)+chr(112)+chr(58)+chr(47)+chr(47)+chr(108)+chr(111)+chr(99)+chr(97)+chr(108)+chr(104)+chr(111)+chr(115)+chr(116)+chr(58)+chr(49)+chr(56)+chr(48)+chr(56)+chr(48)+chr(47)+chr(97)+chr(112)+chr(105)+chr(47)+chr(115)+chr(105)+chr(103)+chr(110)+chr(105)+chr(110), timeout=5); '
    'print(chr(79)+chr(75), r.status, len(r.read()))"'
)
out = stdout.read().decode().strip()
err = stderr.read().decode().strip()
print('Output:', out if out else '(empty)')
print('Error:', err if err else '(empty)')

# Also test if port is listening
stdin2, stdout2, stderr2 = ssh.exec_command(
    'python3 -c "import socket; s=socket.socket(); '
    's.settimeout(2); '
    'result=s.connect_ex((chr(108)+chr(111)+chr(99)+chr(97)+chr(108)+chr(104)+chr(111)+chr(115)+chr(116), 18080)); '
    'print(chr(80)+chr(79)+chr(82)+chr(84), chr(83)+chr(84)+chr(65)+chr(84)+chr(85)+chr(83)+chr(58), result); s.close()"'
)
out2 = stdout2.read().decode().strip()
print('Port check:', out2 if out2 else '(empty)')

ssh.close()
