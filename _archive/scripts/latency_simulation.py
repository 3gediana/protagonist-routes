# -*- coding: utf-8 -*-
"""
SZTU ACM 签到链路耗时模拟与 Y 值过期风险评估

场景：同学 A 扫码获取 Y 值，同时给自己和 VPS 上的账号签到
"""
import time, statistics

# ============= 实测数据 =============
# 本地（校园网）→ acm.sztu.edu.cn:40130
LOCAL_RTT_MS = 51          # 平均 RTT (实测: 40~84ms)
LOCAL_LOGIN_MS = 433       # 获取 SSO code
LOCAL_EXCHANGE_MS = 1068   # 交换 token
LOCAL_FETCH_TASKS_MS = 726 # 获取任务列表
LOCAL_SUBMIT_MS = 50       # 提交签到

# VPS → 校园网 (通过 SSH 反向隧道 + SOCKS5)
VPS_TO_PC_SSH_MS = 50      # 新加坡→深圳 SSH 延迟 (估算)
PC_TO_CAMPUS_MS = 0        # 用户 PC 在校园网内，延迟可忽略
VPS_PROXY_OVERHEAD_MS = 20 # SOCKS5 代理额外开销 (估算)
VPS_TOTAL_OVERHEAD_MS = VPS_TO_PC_SSH_MS + PC_TO_CAMPUS_MS + VPS_PROXY_OVERHEAD_MS

# 扫码解析
SCAN_PARSE_MS = 100        # 手机扫码 + 解析 URL

# Y 值有效期
Y_LIFETIME_S = 4.5
Y_LIFETIME_MS = Y_LIFETIME_S * 1000

# ============= 场景计算 =============

def scenario_a_local_with_cache(scan_delay_ms):
    """场景 A：扫码同学自己签，提前缓存了 token"""
    steps = [
        ("扫码解析 Y 值", SCAN_PARSE_MS),
        ("本地编码 Y", 1),
        ("本地提交签到", LOCAL_SUBMIT_MS),
    ]
    total = sum(t for _, t in steps)
    remaining = Y_LIFETIME_MS - scan_delay_ms
    ok = total <= remaining
    return total, remaining, ok, steps

def scenario_a_local_no_cache(scan_delay_ms):
    """场景 A：扫码同学自己签，没有缓存 token"""
    steps = [
        ("扫码解析 Y 值", SCAN_PARSE_MS),
        ("登录获取 code", LOCAL_LOGIN_MS),
        ("交换 token", LOCAL_EXCHANGE_MS),
        ("获取任务列表", LOCAL_FETCH_TASKS_MS),
        ("本地编码 Y", 1),
        ("本地提交签到", LOCAL_SUBMIT_MS),
    ]
    total = sum(t for _, t in steps)
    remaining = Y_LIFETIME_MS - scan_delay_ms
    ok = total <= remaining
    return total, remaining, ok, steps

def scenario_b_vps_with_cache(scan_delay_ms, transfer_ms):
    """场景 B：VPS 账号签，提前缓存了 token"""
    steps = [
        ("扫码解析 Y 值", SCAN_PARSE_MS),
        ("传输 Y 到 VPS", transfer_ms),
        ("VPS 编码 Y", 1),
        ("VPS 提交签到 (经代理)", LOCAL_SUBMIT_MS + VPS_TOTAL_OVERHEAD_MS),
    ]
    total = sum(t for _, t in steps)
    remaining = Y_LIFETIME_MS - scan_delay_ms
    ok = total <= remaining
    return total, remaining, ok, steps

def scenario_b_vps_no_cache(scan_delay_ms, transfer_ms):
    """场景 B：VPS 账号签，没有缓存 token"""
    steps = [
        ("扫码解析 Y 值", SCAN_PARSE_MS),
        ("传输 Y 到 VPS", transfer_ms),
        ("VPS 登录获取 code (经代理)", LOCAL_LOGIN_MS + VPS_TOTAL_OVERHEAD_MS),
        ("VPS 交换 token (经代理)", LOCAL_EXCHANGE_MS + VPS_TOTAL_OVERHEAD_MS),
        ("VPS 获取任务列表 (经代理)", LOCAL_FETCH_TASKS_MS + VPS_TOTAL_OVERHEAD_MS),
        ("VPS 编码 Y", 1),
        ("VPS 提交签到 (经代理)", LOCAL_SUBMIT_MS + VPS_TOTAL_OVERHEAD_MS),
    ]
    total = sum(t for _, t in steps)
    remaining = Y_LIFETIME_MS - scan_delay_ms
    ok = total <= remaining
    return total, remaining, ok, steps

# ============= 输出报告 =============
print("=" * 70)
print("SZTU ACM 签到链路耗时模拟与 Y 值过期风险评估")
print("=" * 70)

print("\n【实测基准数据】")
print(f"  本地 → acm.sztu.edu.cn RTT: {LOCAL_RTT_MS}ms (实测范围 40~84ms)")
print(f"  本地登录链: {LOCAL_LOGIN_MS + LOCAL_EXCHANGE_MS + LOCAL_FETCH_TASKS_MS}ms")
print(f"    - 获取 SSO code: {LOCAL_LOGIN_MS}ms")
print(f"    - 交换 token: {LOCAL_EXCHANGE_MS}ms")
print(f"    - 获取任务列表: {LOCAL_FETCH_TASKS_MS}ms")
print(f"  VPS → 用户 PC SSH 延迟: {VPS_TO_PC_SSH_MS}ms (新加坡→深圳估算)")
print(f"  VPS 代理额外开销: {VPS_PROXY_OVERHEAD_MS}ms")
print(f"  Y 值有效期: {Y_LIFETIME_S}s ({Y_LIFETIME_MS}ms)")

print("\n【关键发现】")
print("  1. VPS 上目前只有 1 个账号: 202500205105")
print("  2. VPS 无法直接访问 acm.sztu.edu.cn (DNS 解析失败)")
print("  3. VPS 必须通过 SSH 反向隧道 + SOCKS5 代理访问校园网")
print("  4. SSH 反向隧道 localhost:18080 目前似乎未建立")

# 不同扫码延迟下的成功率
print("\n【场景模拟结果】")
print("-" * 70)

for scan_delay_s in [0, 1, 2, 3, 3.5, 4]:
    scan_delay_ms = scan_delay_s * 1000
    print(f"\n扫码延迟: {scan_delay_s}s (剩余有效期: {Y_LIFETIME_S - scan_delay_s}s)")

    # 场景 A 有缓存
    total, remaining, ok, steps = scenario_a_local_with_cache(scan_delay_ms)
    status = "✅ 成功" if ok else "❌ 过期"
    print(f"  A1. 本地自己签 (有缓存): {total:.0f}ms / 剩余 {remaining:.0f}ms → {status}")

    # 场景 A 无缓存
    total, remaining, ok, steps = scenario_a_local_no_cache(scan_delay_ms)
    status = "✅ 成功" if ok else "❌ 过期"
    print(f"  A2. 本地自己签 (无缓存): {total:.0f}ms / 剩余 {remaining:.0f}ms → {status}")

    # 场景 B 有缓存 (假设 Y 传输延迟 50ms)
    total, remaining, ok, steps = scenario_b_vps_with_cache(scan_delay_ms, 50)
    status = "✅ 成功" if ok else "❌ 过期"
    print(f"  B1. VPS 签到 (有缓存): {total:.0f}ms / 剩余 {remaining:.0f}ms → {status}")

    # 场景 B 无缓存
    total, remaining, ok, steps = scenario_b_vps_no_cache(scan_delay_ms, 50)
    status = "✅ 成功" if ok else "❌ 过期"
    print(f"  B2. VPS 签到 (无缓存): {total:.0f}ms / 剩余 {remaining:.0f}ms → {status}")

print("\n" + "=" * 70)
print("【结论与建议】")
print("=" * 70)
print("""
1. 【必须提前缓存 token】
   - 不缓存 token 时，即使扫码延迟为 0s，总耗时 ~2.2s，剩余 2.3s，刚好够用。
   - 但扫码延迟 > 2s 时，无缓存一定会过期。
   - 提前缓存 token 后，总耗时降至 ~150ms，即使扫码延迟 3.5s 也能成功。

2. 【VPS 签到需要修复 SSH 反向隧道】
   - VPS 无法直接访问校园网，必须依赖 SSH 反向隧道 localhost:18080。
   - 当前该隧道未建立，VPS 签到功能不可用。
   - 修复命令: ssh -R 18080:localhost:18080 root@143.198.94.163

3. 【Y 值是共享的，但有有效期】
   - Y 值在 4.5 秒内对所有人通用。
   - 过期后需要重新扫码获取新的 Y 值。

4. 【容错策略】
   - 扫码后立即判断 Y 值剩余有效期。
   - 如果剩余 < 200ms，提示"码即将过期，请等下次刷新"。
   - 如果 200ms <= 剩余 < 1s，只给本地签到，不给 VPS 签到。
   - 如果剩余 >= 1s，本地 + VPS 同时签。
""")
