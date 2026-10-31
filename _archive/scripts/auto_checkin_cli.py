import configparser
import os.path
import re
from datetime import datetime
import requests
import urllib3
import random
import time
import sys

class Course:
    id = '0'
    class_id = '0'
    flag = True
    check_list = []
    class_list = []

host = "https://www.duifene.com"
UA = 'Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Mobile/15E148 MicroMessenger/8.0.40(0x1800282a) NetType/WIFI Language/zh_CN'
urllib3.disable_warnings()
x = requests.Session()
x.headers['User-Agent'] = UA
x.verify = False
filename = 'duifene_config.ini'
config = configparser.ConfigParser()

def extract_html_values(html, element_id):
    pattern = rf'id="{element_id}"[^>]*value="([^"]*)"'
    match = re.search(pattern, html)
    if match:
        return match.group(1)
    pattern = rf'id="{element_id}"[^>]*>([^<]*)<'
    match = re.search(pattern, html)
    if match:
        return match.group(1).strip()
    return None

def save_cookie(_x):
    config['INFO'] = {'cookie': _x.request.headers.get("cookie")}
    with open(filename, 'w') as f:
        config.write(f)

def login_link():
    link = input("Enter WeChat link: ")
    code = re.search(r"(?<=code=)\S{32}", link)
    if code is not None:
        x.cookies.clear()
        code = code.group()
        print(f"Extracted code: {code}")
        _r = x.get(url=host + f"/P.aspx?authtype=1&code={code}&state=1")
        print(f"Login response status: {_r.status_code}")
        get_class_list()
        save_cookie(_r)
    else:
        print("Link error: could not extract code")

def login():
    username_input = input("Enter username: ")
    password_input = input("Enter password: ")
    headers = {"Content-Type": "application/x-www-form-urlencoded; charset=UTF-8", "Referer": host}
    params = f'action=loginmb&loginname={username_input}&password={password_input}'
    x.cookies.clear()
    x.get(host)
    _r = x.post(url=host + "/AppCode/LoginInfo.ashx", data=params, headers=headers)
    if _r.status_code == 200:
        msg = _r.json()["msgbox"]
        print(f"Login result: {msg}")
        if "Login successful" in msg or "登录成功" in msg:
            get_class_list()
            save_cookie(_r)
            return True
    return False

def get_user_id():
    _r = x.get(url=host + "/_UserCenter/MB/index.aspx")
    if _r.status_code == 200:
        stu_id = extract_html_values(_r.text, "hidUID")
        return stu_id
    return None

def sign(sign_code):
    if len(sign_code) == 4:
        headers = {"Content-Type": "application/x-www-form-urlencoded; charset=UTF-8", "Referer": host}
        params = f"action=studentcheckin&studentid={get_user_id()}&checkincode={sign_code}"
        _r = x.post(url=host + "/_CheckIn/CheckIn.ashx", data=params, headers=headers)
        if _r.status_code == 200:
            msg = _r.json()["msgbox"]
            print(f"\tCheck-in result: {msg}")
            if "Check-in successful" in msg or "签到成功" in msg or msg == "签到成功！":
                return True
    return False

def main():
    print("=" * 50)
    print("Duifene Auto Check-in Tool")
    print("=" * 50)
    print("Select login method:")
    print("1. WeChat link login")
    print("2. Account password login")
    choice = input("Enter choice (1-2): ").strip()
    if choice == '1':
        login_link()
    elif choice == '2':
        login()
    print("Tool setup complete!")

if __name__ == '__main__':
    main()
