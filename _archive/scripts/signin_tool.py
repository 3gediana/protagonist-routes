# -*- coding: utf-8 -*-
"""
SZTU ACM 签到工具
用法：
1. 扫描老师投屏上的二维码，获取 URL（如 https://acm.sztu.edu.cn/signinop?tid=xxx&server_now_validate=1780...）
2. 从 URL 中复制 server_now_validate= 后面的值（纯数字，如 1780021776084）
3. 运行此脚本，粘贴该值，按回车
4. 脚本自动完成编码并提交
"""
import sys
sys.path.insert(0, r'D:\claude-code\acm_signin_http')

import requests, hashlib
from pathlib import Path
from sztu_acm_signin_http import (
    http_login_for_code, exchange_tokens,
    fetch_signintask_catalog, auth_headers,
    HWSYS_API_BASE, simple_encode,
)

USERNAME = "202500205105"
PASSWORD = "3gedian.A"

def do_signin(raw_y_value: str) -> dict:
    session = requests.Session()
    session.verify = False
    requests.packages.urllib3.disable_warnings()

    trace_dir = Path(r'D:\claude-code\acm_signin_http\test_traces')

    # 1. Login
    code = http_login_for_code(session, USERNAME, PASSWORD, trace_dir)
    token_result = exchange_tokens(session, code)
    hwsys_token = token_result["hwsys_token"]

    # 2. Get active task
    task_catalog = fetch_signintask_catalog(session, hwsys_token)
    if not task_catalog or not task_catalog.get("data"):
        return {"error": "No active tasks"}

    task = task_catalog["data"][0]
    signintask_id = task["id"]
    print(f"[Task] {task.get('title')} | ID: {signintask_id}")

    # 3. Encode Y
    y_encoded = simple_encode(raw_y_value)
    print(f"[Y] Raw: {raw_y_value} -> Encoded: {y_encoded}")

    # 4. Build payload
    machine_hash = hashlib.md5(f"{USERNAME}_android_chrome120".encode("utf-8")).hexdigest()
    payload = {
        "userx_key": USERNAME,
        "signin_status": 10,
        "signintask_id": signintask_id,
        "server_now_validate": y_encoded,
        "hash_info": simple_encode(machine_hash),
    }

    # 5. Submit
    resp = session.post(
        f"{HWSYS_API_BASE}/signinstu_do",
        headers=auth_headers(hwsys_token),
        json=payload,
        timeout=60,
    )
    result = resp.json() if resp.text else {}
    return result

if __name__ == "__main__":
    print("=" * 50)
    print("SZTU ACM 签到工具")
    print("=" * 50)
    print("请扫描老师投屏上的二维码，从 URL 中提取")
    print("server_now_validate= 后面的纯数字值，粘贴到下面：")
    print("-" * 50)

    raw_y = input("> Y-Value: ").strip()
    if not raw_y:
        print("未输入 Y 值，退出")
        sys.exit(1)

    print("\n正在提交...")
    result = do_signin(raw_y)
    print(f"\n结果: {result}")

    status = result.get("status_code", 0)
    if status == 200:
        print("\n*** 签到成功 ***")
    else:
        detail = result.get("detail", "Unknown error")
        print(f"\n*** 签到失败: {detail} ***")
        if "过期" in detail or "失效" in detail:
            print("提示: Y 值仅在 4.5 秒内有效，请立即重新扫描二维码并再次运行")
