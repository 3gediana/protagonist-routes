# -*- coding: utf-8 -*-
"""
SZTU ACM 签到工具 v2
用法：
1. 扫描老师投屏上的二维码，获取 URL
2. 从 URL 中复制 server_now_validate= 后面的纯数字值
3. 运行此脚本，粘贴该值，按回车
4. 脚本列出当前所有活跃任务，你选择对应的课程
5. 脚本自动完成编码并提交
"""
import sys
sys.path.insert(0, r'D:\claude-code\acm_signin_http')

import requests, hashlib, datetime
from pathlib import Path
from sztu_acm_signin_http import (
    http_login_for_code, exchange_tokens,
    fetch_signintask_catalog, auth_headers,
    HWSYS_API_BASE, simple_encode,
)

USERNAME = "202500205105"
PASSWORD = "3gedian.A"

def get_active_tasks():
    session = requests.Session()
    session.verify = False
    requests.packages.urllib3.disable_warnings()

    trace_dir = Path(r'D:\claude-code\acm_signin_http\test_traces')
    code = http_login_for_code(session, USERNAME, PASSWORD, trace_dir)
    token_result = exchange_tokens(session, code)
    hwsys_token = token_result["hwsys_token"]

    task_catalog = fetch_signintask_catalog(session, hwsys_token)
    now = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8)))

    active_tasks = []
    for t in task_catalog.get("data", []):
        start_str = t.get("time_start", "")
        end_str = t.get("time_end", "")
        if start_str and end_str:
            try:
                start = datetime.datetime.fromisoformat(start_str.replace("Z", "+00:00"))
                end = datetime.datetime.fromisoformat(end_str.replace("Z", "+00:00"))
                if start <= now <= end:
                    active_tasks.append(t)
            except Exception:
                pass

    return session, hwsys_token, active_tasks

def do_signin(session, hwsys_token, signintask_id, raw_y_value):
    y_encoded = simple_encode(raw_y_value)
    machine_hash = hashlib.md5(f"{USERNAME}_android_chrome120".encode("utf-8")).hexdigest()
    payload = {
        "userx_key": USERNAME,
        "signin_status": 10,
        "signintask_id": signintask_id,
        "server_now_validate": y_encoded,
        "hash_info": simple_encode(machine_hash),
    }
    resp = session.post(
        f"{HWSYS_API_BASE}/signinstu_do",
        headers=auth_headers(hwsys_token),
        json=payload,
        timeout=60,
    )
    return resp.json() if resp.text else {}

if __name__ == "__main__":
    print("=" * 60)
    print("SZTU ACM 签到工具 v2")
    print("=" * 60)

    print("\n正在获取当前活跃任务列表...")
    session, hwsys_token, active_tasks = get_active_tasks()

    if not active_tasks:
        print("当前没有活跃的课程任务")
        sys.exit(1)

    now = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8)))
    print(f"\n当前时间: {now.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"找到 {len(active_tasks)} 个当前活跃任务:\n")

    for i, t in enumerate(active_tasks):
        start = datetime.datetime.fromisoformat(t.get("time_start", "").replace("Z", "+00:00"))
        end = datetime.datetime.fromisoformat(t.get("time_end", "").replace("Z", "+00:00"))
        print(f"[{i}] {t.get('title')}")
        print(f"    时间: {start.strftime('%H:%M')} - {end.strftime('%H:%M')}")
        print()

    task_idx = input("请选择你的课程编号: ").strip()
    try:
        task_idx = int(task_idx)
        selected_task = active_tasks[task_idx]
    except (ValueError, IndexError):
        print("无效选择")
        sys.exit(1)

    signintask_id = selected_task["id"]
    print(f"\n已选择: {selected_task.get('title')}")

    print("-" * 60)
    print("请扫描老师投屏上的二维码，从 URL 中提取")
    print("server_now_validate= 后面的纯数字值，粘贴到下面：")
    print("-" * 60)

    raw_y = input("> Y-Value: ").strip()
    if not raw_y:
        print("未输入 Y 值，退出")
        sys.exit(1)

    print(f"\n正在提交签到...")
    result = do_signin(session, hwsys_token, signintask_id, raw_y)
    print(f"结果: {result}")

    status = result.get("status_code", 0)
    if status == 200:
        print("\n*** 签到成功 ***")
    else:
        detail = result.get("detail", "Unknown error")
        print(f"\n*** 签到失败: {detail} ***")
        if "过期" in detail or "失效" in detail:
            print("提示: Y 值仅在 4.5 秒内有效，请立即重新扫描二维码并再次运行")
