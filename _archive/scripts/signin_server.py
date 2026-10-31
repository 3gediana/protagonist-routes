# -*- coding: utf-8 -*-
"""
PC Campus Signin Server - 路由签到到扫码者手机
"""
import asyncio, json, os, time, traceback, hashlib
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, List, Optional, Set

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.responses import HTMLResponse
from pydantic import BaseModel

import sys
sys.path.insert(0, r'D:\claude-code\acm_signin_http')

from sztu_acm_signin_http import (
    http_login_for_code, exchange_tokens,
    fetch_signintask_catalog, fetch_signin_catalog, simple_encode,
    auth_headers, HWSYS_API_BASE,
    token_not_expired, jwt_payload,
    extract_list_items, signin_membership_summary,
)
import requests

requests.packages.urllib3.disable_warnings()

# ============ 配置 ============
DATA_DIR = Path(r'D:\claude-code\c++\signin_data')
DATA_DIR.mkdir(exist_ok=True)

ACCOUNTS_FILE = DATA_DIR / 'accounts.json'
TOKENS_FILE = DATA_DIR / 'tokens.json'
COURSES_FILE = DATA_DIR / 'courses.json'
SIGNIN_HISTORY_FILE = DATA_DIR / 'history.json'
SCHEDULE_FILE = DATA_DIR / 'schedule.json'

# ============ 数据模型 ============
class Account(BaseModel):
    userx_key: str
    username: str
    password: str
    name: str = ""
    device_id: str = ""  # 该账号绑定哪个手机扫码
    is_active: bool = True

class YValueRequest(BaseModel):
    y_value: str
    scanner_device: str  # 扫码者手机ID
    qr_url: str  # 二维码完整URL

class MobilePayload(BaseModel):
    userx_key: str
    name: str
    hwsys_token: str
    signintask_id: str
    server_now_validate: str  # 已编码的Y值
    hash_info: str

# ============ 全局状态 ============
accounts: Dict[str, Account] = {}
token_cache: Dict[str, dict] = {}
# 每个账号的课程列表: {userx_key: {signin_id, ...}}
account_courses: Dict[str, Set[str]] = {}
signin_history: List[dict] = []
# 已成功签到的(userx_key, task_id)组合，防止重复签到
signed_tasks: Set[str] = set()
# 课程表: [{day_of_week, start_minutes, end_minutes, name, signin_id?}]
schedule_courses: List[dict] = []
# 已检测到活跃签到并通知过的课程时间段，防止重复通知
notified_tasks: Set[str] = set()

# WebSocket 连接: {device_id: websocket}
mobile_ws: Dict[str, WebSocket] = {}
# 等待手机返回结果: {device_id: asyncio.Queue}
mobile_result_queues: Dict[str, asyncio.Queue] = {}

# ============ 持久化 ============
def load_accounts():
    global accounts
    if ACCOUNTS_FILE.exists():
        data = json.loads(ACCOUNTS_FILE.read_text('utf-8'))
        accounts = {k: Account(**v) for k, v in data.get('accounts', {}).items()}
    else:
        accounts = {
            '202500205105': Account(
                userx_key='202500205105',
                username='202500205105',
                password='3gedian.A',
                name='曾浩天',
                device_id='device_zht',
                is_active=True,
            )
        }
        save_accounts()

def save_accounts():
    ACCOUNTS_FILE.write_text(
        json.dumps({'accounts': {k: v.model_dump() for k, v in accounts.items()}},
                   ensure_ascii=False, indent=2), 'utf-8')

def load_tokens():
    global token_cache
    if TOKENS_FILE.exists():
        data = json.loads(TOKENS_FILE.read_text('utf-8'))
        token_cache = data.get('tokens', {})

def save_tokens():
    TOKENS_FILE.write_text(
        json.dumps({'tokens': token_cache}, ensure_ascii=False, indent=2), 'utf-8')

def load_courses():
    global account_courses
    if COURSES_FILE.exists():
        data = json.loads(COURSES_FILE.read_text('utf-8'))
        account_courses = {k: set(v) for k, v in data.get('courses', {}).items()}

def save_courses():
    COURSES_FILE.write_text(
        json.dumps({'courses': {k: list(v) for k, v in account_courses.items()}},
                   ensure_ascii=False, indent=2), 'utf-8')

def load_history():
    global signin_history, signed_tasks
    if SIGNIN_HISTORY_FILE.exists():
        data = json.loads(SIGNIN_HISTORY_FILE.read_text('utf-8'))
        signin_history = data.get('history', [])
    else:
        signin_history = []
    # 从历史记录重建已签到集合
    signed_tasks = set()
    for h in signin_history:
        if h.get('status') == 'success' and h.get('userx_key') and h.get('task_id'):
            signed_tasks.add(f"{h['userx_key']}:{h['task_id']}")
    print(f"[HISTORY] 加载{len(signin_history)}条历史，{len(signed_tasks)}条成功签到记录")

def save_history():
    SIGNIN_HISTORY_FILE.write_text(
        json.dumps({'history': signin_history[-500:]}, ensure_ascii=False, indent=2), 'utf-8')

def load_schedule():
    global schedule_courses
    if SCHEDULE_FILE.exists():
        data = json.loads(SCHEDULE_FILE.read_text('utf-8'))
        schedule_courses = data.get('courses', [])
    else:
        schedule_courses = []

def save_schedule():
    SCHEDULE_FILE.write_text(
        json.dumps({'courses': schedule_courses}, ensure_ascii=False, indent=2), 'utf-8')

# ============ Token 管理 ============
def _fmt_ts(ts: int) -> str:
    try:
        return datetime.fromtimestamp(ts).strftime('%m-%d %H:%M:%S')
    except:
        return str(ts)

def _log_token_state(userx_key: str, cached: dict):
    """记录当前token过期状态，方便排查"""
    now = time.time()
    htk = cached.get('hwsys_token', '')
    sso = cached.get('sso_token', '')
    h_exp = jwt_payload(htk).get('exp', 0) if htk else 0
    s_exp = jwt_payload(sso).get('exp', 0) if sso else 0
    h_valid = token_not_expired(htk)
    s_valid = token_not_expired(sso)
    if h_valid:
        print(f"[TOKEN] {userx_key}: hwsys有效(过期{_fmt_ts(h_exp)})")
    elif htk:
        print(f"[TOKEN] {userx_key}: hwsys已过期(过期{_fmt_ts(h_exp)})")
    if s_valid:
        print(f"[TOKEN] {userx_key}: sso有效(过期{_fmt_ts(s_exp)})")
    elif sso:
        print(f"[TOKEN] {userx_key}: sso已过期(过期{_fmt_ts(s_exp)})")
    if not htk and not sso:
        print(f"[TOKEN] {userx_key}: 无任何缓存token")

def ensure_token(userx_key: str, force_refresh: bool = False) -> Optional[str]:
    account = accounts.get(userx_key)
    if not account:
        print(f"[TOKEN] {userx_key}: 账号不存在")
        return None

    cached = token_cache.get(userx_key, {})
    htk = cached.get('hwsys_token', '')
    sso = cached.get('sso_token', '')

    if not force_refresh and cached and token_not_expired(htk):
        return htk

    _log_token_state(userx_key, cached)

    # L1: 尝试用SSO刷新hwsys
    if not force_refresh and cached and token_not_expired(sso):
        try:
            session = requests.Session()
            session.verify = False
            hwsys_resp = session.post(
                f'{HWSYS_API_BASE}/token',
                json={'access_token_sso': sso},
                timeout=60,
            )
            hwsys_data = hwsys_resp.json() if hwsys_resp.text else {}
            new_hwsys = hwsys_data.get('access_token', '')
            if new_hwsys:
                cached['hwsys_token'] = new_hwsys
                cached['hwsys_exp'] = jwt_payload(new_hwsys).get('exp', 0)
                cached['last_refresh'] = datetime.now().isoformat()
                token_cache[userx_key] = cached
                save_tokens()
                print(f"[TOKEN] {userx_key}: SSO刷新hwsys成功，新过期{_fmt_ts(cached['hwsys_exp'])}")
                return new_hwsys
            else:
                print(f"[TOKEN] {userx_key}: SSO刷新hwsys失败，响应无access_token: {hwsys_data}")
        except Exception as e:
            print(f"[TOKEN] {userx_key}: SSO刷新hwsys异常: {e}")

    # L2: 全量重新登录
    try:
        session = requests.Session()
        session.verify = False
        trace_dir = DATA_DIR / 'traces'
        trace_dir.mkdir(exist_ok=True)
        print(f"[TOKEN] {userx_key}: 触发全量重新登录...")
        code = http_login_for_code(session, account.username, account.password, trace_dir)
        tokens = exchange_tokens(session, code)
        token_cache[userx_key] = {
            'sso_token': tokens['sso_token'],
            'hwsys_token': tokens['hwsys_token'],
            'sso_exp': jwt_payload(tokens['sso_token']).get('exp', 0),
            'hwsys_exp': jwt_payload(tokens['hwsys_token']).get('exp', 0),
            'last_refresh': datetime.now().isoformat(),
        }
        save_tokens()
        print(f"[TOKEN] {userx_key}: 登录获取token成功，sso过期{_fmt_ts(token_cache[userx_key]['sso_exp'])}, "
              f"hwsys过期{_fmt_ts(token_cache[userx_key]['hwsys_exp'])}")
        return tokens['hwsys_token']
    except Exception as e:
        print(f"[TOKEN] {userx_key}: 全量登录失败: {e}")
        return None


def is_auth_error(resp_data: dict) -> bool:
    """判断API响应是否是认证失败/token过期"""
    if not isinstance(resp_data, dict):
        return False
    status = resp_data.get('status_code') or resp_data.get('_http_status')
    detail = str(resp_data.get('detail', ''))
    if status in (401, 403):
        return True
    if 'token' in detail.lower() and ('过期' in detail or '无效' in detail or 'expired' in detail.lower()):
        return True
    if '未登录' in detail or '认证' in detail:
        return True
    return False


def call_with_auth_retry(userx_key: str, api_func) -> Optional[dict]:
    """
    执行API调用，如果因token过期失败则自动刷新并重试一次。
    api_func 接收一个有效的 hwsys_token，返回 API 响应 dict。
    """
    htk = ensure_token(userx_key)
    if not htk:
        print(f"[RETRY] {userx_key}: 无法获取有效token")
        return None

    # 第一次尝试
    try:
        resp = api_func(htk)
        if not is_auth_error(resp):
            return resp
        print(f"[RETRY] {userx_key}: API返回认证失败，尝试刷新token... detail={resp.get('detail', '')}")
    except Exception as e:
        print(f"[RETRY] {userx_key}: 首次API调用异常: {e}")
        resp = None

    # 强制刷新后重试
    htk2 = ensure_token(userx_key, force_refresh=True)
    if not htk2:
        print(f"[RETRY] {userx_key}: 强制刷新后仍无有效token")
        return None

    try:
        resp2 = api_func(htk2)
        if is_auth_error(resp2):
            print(f"[RETRY] {userx_key}: 刷新后API仍认证失败，detail={resp2.get('detail', '')}")
        return resp2
    except Exception as e2:
        print(f"[RETRY] {userx_key}: 刷新后API调用异常: {e2}")
        return resp

# ============ 课程列表更新 ============
def refresh_account_courses(userx_key: str):
    """获取该账号的课程(signin)列表，只保留userx_key在白名单中的签到墙"""
    htk = ensure_token(userx_key)
    if not htk:
        print(f"[COURSES] {userx_key}: 无token，跳过")
        return
    try:
        session = requests.Session()
        session.verify = False
        payload = fetch_signin_catalog(session, htk)
        matched_signin_ids = set()
        for signin in extract_list_items(payload):
            if not isinstance(signin, dict):
                continue
            membership = signin_membership_summary(signin, userx_key)
            if membership["matched"]:
                signin_id = str(signin.get("id") or "")
                if signin_id:
                    matched_signin_ids.add(signin_id)
        account_courses[userx_key] = matched_signin_ids
        save_courses()
        print(f"[COURSES] {userx_key}: 更新 {len(matched_signin_ids)} 个匹配课程")
    except Exception as e:
        print(f"[COURSES] {userx_key}: 失败: {e}")

# ============ Y值处理 ============
# 注意：Y值是否有效由服务器端判断，PC端不做过期检查
# 二维码生成时的时间戳是固定的，PC不知道服务器端的具体校验规则

def get_active_task_for_signin(signin_id: str, userx_key: str) -> Optional[dict]:
    """查询某个signin_id下当前活跃的task，带token自动刷新重试"""
    def _query(htk: str):
        session = requests.Session()
        session.verify = False
        r = session.get(f'{HWSYS_API_BASE}/signintask', headers=auth_headers(htk), timeout=10)
        return r.json() if r.text else {}

    resp = call_with_auth_retry(userx_key, _query)
    if resp is None:
        print(f"[TASK] {userx_key}: 查询活跃任务失败，无法获取有效token")
        return None

    tasks = resp.get('data', [])
    now = datetime.now().astimezone()
    for t in tasks:
        if t.get('signin_id') != signin_id:
            continue
        start_str = t.get('time_start', '')
        end_str = t.get('time_end', '')
        if start_str and end_str:
            try:
                st = datetime.fromisoformat(start_str.replace('Z', '+00:00'))
                et = datetime.fromisoformat(end_str.replace('Z', '+00:00'))
                if st <= now <= et and t.get('available') is not False:
                    return t
            except:
                pass
    return None


# ============ 核心: 构建payload并路由到扫码手机 ============
async def route_signin_to_mobile(y_value: str, scanner_device: str, qr_url: str) -> dict:
    """
    核心逻辑:
    1. 从二维码URL提取signin_id
    2. 验证signin_id是否有效(过期课程会404)
    3. 用signin_id匹配账号课程列表
    4. 获取该signin下的活跃task
    5. 为每个匹配账号构建payload并推送到手机
    6. 等待手机返回执行结果
    
    注意：Y值是否有效由服务器端判断，PC端不做过期检查
    """
    t_start = time.time()

    # 1. 从二维码URL提取signin_id（路径最后一段，去掉query string）
    signin_id = None
    try:
        url_without_query = qr_url.split('?')[0]
        path = url_without_query.strip('/')
        parts = path.split('/')
        for part in reversed(parts):
            if len(part) > 30 and part.count('-') >= 3:
                signin_id = part
                break
    except Exception as e:
        print(f"[ROUTE] 从URL提取signin_id失败: {e}")

    if not signin_id:
        return {"status": "invalid_url", "message": "无法从二维码URL提取课程ID", "results": []}

    # 2. 验证signin_id是否有效（过期课程会404），带token自动刷新重试
    first_uk = list(accounts.keys())[0] if accounts else None
    if first_uk:
        def _validate(htk: str):
            session = requests.Session()
            session.verify = False
            r = session.get(f'{HWSYS_API_BASE}/signin/{signin_id}', headers=auth_headers(htk), timeout=10)
            return r.json() if r.text else {}
        vresp = call_with_auth_retry(first_uk, _validate)
        if vresp is not None:
            if vresp.get('status_code') == 404:
                return {"status": "expired_course", "message": "该课程签到已过期或不存在", "results": []}
            task_title = vresp.get('data', {}).get('title', '')

    # 3. 用signin_id匹配账号课程列表
    matched_accounts = []
    for userx_key, account in accounts.items():
        if not account.is_active:
            continue
        my_signin_ids = account_courses.get(userx_key, set())
        if signin_id in my_signin_ids:
            matched_accounts.append((userx_key, account))
            print(f"[ROUTE] 账号 {account.name} 匹配课程 {signin_id[:20]}")

    if not matched_accounts:
        return {"status": "no_match", "message": f"没有账号匹配课程 {signin_id[:20]}", "results": []}

    # 4. 获取该signin下的活跃task（用第一个匹配账号，带自动刷新重试）
    active_task = None
    token_failures = []
    for userx_key, _ in matched_accounts:
        active_task = get_active_task_for_signin(signin_id, userx_key)
        if active_task:
            task_title = active_task.get('title', '')
            break

    if not active_task:
        for userx_key, _ in matched_accounts:
            htk = ensure_token(userx_key)
            if not htk:
                token_failures.append(userx_key)
        if token_failures:
            return {"status": "token_failed", "message": f"以下账号token过期/失效: {', '.join(token_failures)}", "results": []}
        return {"status": "no_active_task", "message": "该课程当前没有活跃签到", "results": []}

    task_id = active_task.get('id', '')

    # 5. 构建payload列表（跳过已签到的账号）
    payloads = []
    skipped = []
    already_signed = []
    encoded_y = simple_encode(y_value)
    for userx_key, account in matched_accounts:
        # 检查是否已签到
        task_key = f"{userx_key}:{task_id}"
        if task_key in signed_tasks:
            print(f"[ROUTE] 跳过账号 {account.name}: 该任务已签到")
            already_signed.append({
                'userx_key': userx_key,
                'name': account.name,
                'status': 'already_signed',
                'detail': '该签到任务已签到，无需重复',
            })
            continue

        htk = ensure_token(userx_key)
        if not htk:
            skipped.append({
                'userx_key': userx_key,
                'name': account.name,
                'status': 'token_failed',
                'detail': 'PC端token过期/失效',
            })
            continue
        machine_hash = hashlib.md5(f"{userx_key}_android_chrome120".encode('utf-8')).hexdigest()
        payloads.append({
            'userx_key': userx_key,
            'name': account.name,
            'hwsys_token': htk,
            'signintask_id': task_id,
            'server_now_validate': encoded_y,
            'hash_info': simple_encode(machine_hash),
        })

    print(f"[ROUTE] 构建{len(payloads)}个payload, 已签到{len(already_signed)}个, 跳过{len(skipped)}个(token问题)")

    # 6. 一次性打包所有payload推送到扫码手机
    ws = mobile_ws.get(scanner_device)
    if not ws:
        return {"status": "offline", "message": f"扫码手机 {scanner_device} 不在线", "results": []}

    result_queue = asyncio.Queue()
    mobile_result_queues[scanner_device] = result_queue

    results = []
    try:
        await ws.send_json({
            'type': 'signin_payloads',
            'payloads': payloads,
            'task_title': task_title,
            'count': len(payloads),
        })
        print(f"[ROUTE] 已推送 {len(payloads)} 个payload到 {scanner_device}，等待批量结果...")
        # 等待手机批量执行后一次性返回（最长30秒）
        try:
            resp = await asyncio.wait_for(result_queue.get(), timeout=30.0)
            batch_results = resp.get('results', [])
            for r in batch_results:
                results.append({
                    'userx_key': r.get('userx_key', ''),
                    'name': r.get('name', ''),
                    'status': r.get('status', 'unknown'),
                    'detail': r.get('detail', ''),
                })
        except asyncio.TimeoutError:
            print(f"[ROUTE] 手机批量执行超时")
            for p in payloads:
                results.append({
                    'userx_key': p['userx_key'],
                    'name': p['name'],
                    'status': 'timeout',
                    'detail': '手机批量响应超时',
                })
    except Exception as e:
        print(f"[ROUTE] WebSocket推送失败: {e}")
        return {"status": "error", "message": str(e), "results": results}
    finally:
        if scanner_device in mobile_result_queues:
            del mobile_result_queues[scanner_device]

    # 把已签到的和因token问题跳过的账号也加入结果
    results.extend(already_signed)
    results.extend(skipped)

    # 记录历史（只记录新执行的，不重复记录already_signed）
    now = datetime.now().isoformat()
    for r in results:
        if r['status'] == 'already_signed':
            continue
        signin_history.append({
            'userx_key': r['userx_key'],
            'task_id': task_id,
            'task_title': task_title,
            'y_value': y_value,
            'status': r['status'],
            'detail': r['detail'],
            'timestamp': now,
            'device_id': scanner_device,
        })
        # 成功的话加入去重集合
        if r['status'] == 'success' and r.get('userx_key') and task_id:
            signed_tasks.add(f"{r['userx_key']}:{task_id}")
    save_history()

    total_elapsed = int((time.time() - t_start) * 1000)
    success = sum(1 for r in results if r['status'] == 'success')
    already = sum(1 for r in results if r['status'] == 'already_signed')
    failed = sum(1 for r in results if r['status'] in ('failed', 'token_failed', 'timeout'))
    print(f"[ROUTE] 完成: 耗时{total_elapsed}ms, {success}成功, {already}已签到, {failed}失败")
    return {
        "status": "ok",
        "message": f"路由完成: {success}成功, {already}已签到, {failed}失败/跳过",
        "results": results,
        "elapsed_ms": total_elapsed,
    }

# ============ 课程表轮询 ============
SCHEDULE_POLL_INTERVAL = 60  # 秒
SCHEDULE_BACKOFF_ERRORS = 3    # 连续失败几次后退避
SCHEDULE_BACKOFF_SLEEP = 300  # 退避后睡300秒(5分钟)

async def schedule_poll_scheduler():
    """
    课程表轮询：在当前课程时间段内，每分钟检查一次活跃签到。
    
    流量防御策略：
    1. 只在课程表配置的时间段内轮询
    2. 同一课程的活跃任务只通知一次（notified_tasks去重）
    3. 连续API失败3次后退避5分钟
    4. 一天内总轮询次数 = sum(每节课时长分钟数)，自然受限
    5. 课程表为空时不产生任何请求
    """
    consecutive_errors = 0
    while True:
        try:
            await asyncio.sleep(SCHEDULE_POLL_INTERVAL)

            if not schedule_courses:
                continue

            # 退避检查
            if consecutive_errors >= SCHEDULE_BACKOFF_ERRORS:
                print(f"[SCHEDULE] 连续失败{consecutive_errors}次，退避{SCHEDULE_BACKOFF_SLEEP}秒")
                await asyncio.sleep(SCHEDULE_BACKOFF_SLEEP)
                consecutive_errors = 0
                continue

            now = datetime.now()
            today_weekday = now.isoweekday()  # 1=周一
            current_minutes = now.hour * 60 + now.minute

            # 找出当前正在进行的课程
            active_courses = [
                c for c in schedule_courses
                if c.get('day_of_week', 1) == today_weekday
                and c.get('start_minutes', 0) <= current_minutes <= c.get('end_minutes', 0)
            ]

            if not active_courses:
                continue

            print(f"[SCHEDULE] 当前有{len(active_courses)}节课进行中，开始轮询...")

            for course in active_courses:
                course_name = course.get('name', '未知')
                signin_id = course.get('signin_id', '')
                
                # 如果没有signin_id，无法精确匹配，跳过
                if not signin_id:
                    print(f"[SCHEDULE] {course_name}: 无signin_id，跳过")
                    continue

                # 检查这节课是否已经通知过了（同一课程同一时间段）
                notify_key = f"{signin_id}:{today_weekday}:{course.get('start_minutes',0)}"
                if notify_key in notified_tasks:
                    continue

                # 找一个有token的账号来查活跃task
                htk = None
                for userx_key in list(accounts.keys()):
                    htk = ensure_token(userx_key)
                    if htk:
                        break

                if not htk:
                    print(f"[SCHEDULE] {course_name}: 无可用token")
                    consecutive_errors += 1
                    continue

                try:
                    session = requests.Session()
                    session.verify = False
                    r = session.get(
                        f'{HWSYS_API_BASE}/signintask',
                        headers=auth_headers(htk),
                        timeout=10,
                    )
                    data = r.json() if r.text else {}
                    tasks = data.get('data', [])
                    
                    for t in tasks:
                        if t.get('signin_id') != signin_id:
                            continue
                        start_str = t.get('time_start', '')
                        end_str = t.get('time_end', '')
                        if not start_str or not end_str:
                            continue
                        try:
                            st = datetime.fromisoformat(start_str.replace('Z', '+00:00'))
                            et = datetime.fromisoformat(end_str.replace('Z', '+00:00'))
                            if st <= now <= et and t.get('available') is not False:
                                # 找到活跃任务！
                                task_id = t.get('id', '')
                                task_title = t.get('title', course_name)
                                print(f"[SCHEDULE] 🔔 {course_name} 发现活跃任务: {task_title[:30]}")
                                
                                # 推送到绑定的手机
                                notified_tasks.add(notify_key)
                                await _push_notification_to_device(course, t)
                                break
                        except:
                            pass
                    
                    consecutive_errors = 0  # 成功，重置错误计数
                    
                except Exception as e:
                    print(f"[SCHEDULE] {course_name}: 轮询异常: {e}")
                    consecutive_errors += 1

        except asyncio.CancelledError:
            break
        except Exception as e:
            print(f"[SCHEDULE] scheduler异常: {e}")
            await asyncio.sleep(60)

async def _push_notification_to_device(course: dict, task: dict):
    """通过WebSocket推送签到通知到绑定的手机"""
    # 找绑定到这门课的手机（通过device_id字段）
    # 如果没有明确绑定，推送到所有在线手机
    target_devices = []
    for userx_key, account in accounts.items():
        if account.device_id:
            target_devices.append(account.device_id)
    
    # 去重
    target_devices = list(set(target_devices))
    
    if not target_devices:
        print(f"[SCHEDULE] 没有绑定手机，无法推送通知")
        return
    
    # 构建通知消息
    msg = {
        'type': 'schedule_alert',
        'course_name': course.get('name', ''),
        'task_title': task.get('title', ''),
        'task_id': task.get('id', ''),
        'signin_id': course.get('signin_id', ''),
        'message': f"【{course.get('name', '')}】签到已开始，请打开扫码页面",
    }
    
    sent = 0
    for device_id in target_devices:
        ws = mobile_ws.get(device_id)
        if ws:
            try:
                await ws.send_json(msg)
                sent += 1
                print(f"[SCHEDULE] 已推送通知到 {device_id}")
            except Exception as e:
                print(f"[SCHEDULE] 推送通知到 {device_id} 失败: {e}")
    
    if sent == 0:
        print(f"[SCHEDULE] 没有在线手机可以接收通知")

# ============ 定时刷新 ============
async def refresh_scheduler():
    """每小时刷新token和课程列表"""
    while True:
        await asyncio.sleep(3600)
        print(f"[SCHEDULER] 定时刷新...")
        for userx_key in list(accounts.keys()):
            ensure_token(userx_key)
            await asyncio.sleep(1)
            refresh_account_courses(userx_key)
            await asyncio.sleep(1)

# ============ FastAPI ============
from contextlib import asynccontextmanager

@asynccontextmanager
async def lifespan(app: FastAPI):
    load_accounts()
    load_tokens()
    load_courses()
    load_history()
    load_schedule()
    print(f"[STARTUP] {len(accounts)}账号, {len(token_cache)}token, {len(account_courses)}课程缓存, {len(schedule_courses)}课程表")
    # 启动时刷新一次课程列表
    for uk in list(accounts.keys()):
        refresh_account_courses(uk)
    task_refresh = asyncio.create_task(refresh_scheduler())
    task_poll = asyncio.create_task(schedule_poll_scheduler())
    yield
    task_refresh.cancel()
    task_poll.cancel()

app = FastAPI(title="SZTU ACM Signin Router", lifespan=lifespan)

# ============ API 路由 ============

@app.get("/")
async def index():
    return {
        "status": "ok",
        "accounts": len(accounts),
        "tokens": len(token_cache),
        "courses_cached": len(account_courses),
        "history": len(signin_history),
        "mobile_online": list(mobile_ws.keys()),
    }

@app.get("/accounts")
async def list_accounts():
    return {
        "accounts": [
            {
                "userx_key": a.userx_key,
                "name": a.name,
                "device_id": a.device_id,
                "is_active": a.is_active,
                "has_token": userx_key in token_cache and token_not_expired(token_cache[userx_key].get('hwsys_token', '')),
                "courses_count": len(account_courses.get(userx_key, set())),
            }
            for userx_key, a in accounts.items()
        ]
    }

@app.post("/accounts")
async def add_account(account: Account):
    accounts[account.userx_key] = account
    save_accounts()
    ensure_token(account.userx_key)
    refresh_account_courses(account.userx_key)
    return {"status": "ok", "message": f"账号 {account.name} 已添加"}

@app.delete("/accounts/{userx_key}")
async def delete_account(userx_key: str):
    if userx_key in accounts:
        del accounts[userx_key]
        if userx_key in token_cache: del token_cache[userx_key]; save_tokens()
        if userx_key in account_courses: del account_courses[userx_key]; save_courses()
        save_accounts()
        return {"status": "ok"}
    raise HTTPException(status_code=404, detail="账号不存在")

@app.post("/signin/submit")
async def submit_signin_api(request: YValueRequest):
    """接收Y值和二维码URL，匹配账号，路由payload到扫码手机"""
    print(f"[API] Y={request.y_value[:15]}..., device={request.scanner_device}")
    if not request.y_value or not request.y_value.isdigit():
        raise HTTPException(status_code=400, detail="Y值格式无效")
    if not request.qr_url:
        raise HTTPException(status_code=400, detail="缺少二维码URL")
    return await route_signin_to_mobile(request.y_value, request.scanner_device, request.qr_url)

@app.get("/schedule")
async def get_schedule():
    """获取当前课程表"""
    return {"courses": schedule_courses}

@app.post("/schedule")
async def update_schedule(request: dict):
    """更新课程表"""
    global schedule_courses
    courses = request.get('courses', [])
    # 基本校验
    valid = []
    for c in courses:
        if not isinstance(c, dict):
            continue
        name = str(c.get('name', '')).strip()
        if not name:
            continue
        day = int(c.get('day_of_week', 1))
        start = int(c.get('start_minutes', 0))
        end = int(c.get('end_minutes', 0))
        if end <= start:
            continue
        valid.append({
            'day_of_week': max(1, min(7, day)),
            'start_minutes': max(0, min(1439, start)),
            'end_minutes': max(0, min(1439, end)),
            'name': name,
            'signin_id': str(c.get('signin_id', '')).strip(),
        })
    schedule_courses = valid
    # 课程表变更后清空已通知记录（新的课程时间段需要重新检测）
    notified_tasks.clear()
    save_schedule()
    print(f"[API] 课程表更新: {len(valid)} 门课")
    return {"status": "ok", "count": len(valid)}

@app.get("/history")
async def get_history(limit: int = 50):
    return {"history": signin_history[-limit:]}

@app.post("/refresh-all")
async def refresh_all():
    """手动刷新所有账号的token和课程"""
    results = {}
    for userx_key in list(accounts.keys()):
        token = ensure_token(userx_key)
        if token:
            refresh_account_courses(userx_key)
            results[userx_key] = {"token": "ok", "courses": len(account_courses.get(userx_key, set()))}
        else:
            results[userx_key] = {"token": "failed"}
    return results

# ============ WebSocket: 手机连接 ============
@app.websocket("/ws/mobile/{device_id}")
async def mobile_websocket(websocket: WebSocket, device_id: str):
    """扫码手机通过这个端点连接，接收签到payload并返回结果"""
    await websocket.accept()
    mobile_ws[device_id] = websocket
    print(f"[WS] 手机上线: {device_id}")
    try:
        while True:
            data = await websocket.receive_json()
            msg_type = data.get('type')
            
            if msg_type == 'heartbeat':
                await websocket.send_json({'type': 'pong', 'time': datetime.now().isoformat()})
            
            elif msg_type == 'register':
                await websocket.send_json({
                    'type': 'registered',
                    'device_id': device_id,
                })
            
            elif msg_type == 'scan_y':
                # 手机扫码后发送Y值和二维码URL，PC开始路由
                y_value = data.get('y_value', '')
                qr_url = data.get('qr_url', '')
                print(f"[WS] 收到scan_y: device={device_id}, y={y_value[:15]}")
                result = await route_signin_to_mobile(y_value, device_id, qr_url)
                await websocket.send_json({
                    'type': 'scan_result',
                    'result': result,
                })
            
            elif msg_type == 'signin_results':
                # 手机批量执行完所有签到后，一次性返回汇总结果
                queue = mobile_result_queues.get(device_id)
                if queue:
                    await queue.put({
                        'results': data.get('results', []),
                    })
                    print(f"[WS] {device_id} 收到批量结果: {len(data.get('results', []))} 条")
                else:
                    print(f"[WS] {device_id} 收到signin_results但无等待队列")
            
            elif msg_type == 'signin_result':
                # 兼容：手机单个签到结果（旧协议）
                queue = mobile_result_queues.get(device_id)
                if queue:
                    await queue.put({
                        'userx_key': data.get('userx_key'),
                        'status': data.get('status'),
                        'detail': data.get('detail'),
                    })
                else:
                    print(f"[WS] {device_id} 收到signin_result但无等待队列")
            
    except WebSocketDisconnect:
        pass
    except Exception as e:
        print(f"[WS] {device_id} 错误: {e}")
    finally:
        if device_id in mobile_ws:
            del mobile_ws[device_id]
        if device_id in mobile_result_queues:
            del mobile_result_queues[device_id]
        print(f"[WS] 手机离线: {device_id}")

# ============ 启动 ============
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8645)
