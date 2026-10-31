# -*- coding: utf-8 -*-
"""
模拟手机客户端，测试批量签到链路
PC统一下发所有payload，手机批量执行后一次性返回结果
"""
import asyncio, json, websockets

PC_URL = "ws://127.0.0.1:8645/ws/mobile/test_phone_zht"

async def mock_mobile():
    print("[MOBILE] 连接PC...")
    async with websockets.connect(PC_URL) as ws:
        print("[MOBILE] WebSocket已连接")

        # 1. 注册设备
        await ws.send(json.dumps({'type': 'register', 'device_id': 'test_phone_zht'}))
        resp = await ws.recv()
        print(f"[MOBILE] 注册响应: {resp}")

        import time

        # === 测试A: 过期二维码 ===
        y_value = str(int(time.time() * 1000))
        expired_qr = ("https://acm.sztu.edu.cn:40130/hwsys/signin/signin_wall/"
                      "2c15ae0a-9d0d-4abd-907b-de7508a827db?server_now_validate=" + y_value)
        print(f"\n[MOBILE] 测试A: 过期二维码")
        await ws.send(json.dumps({
            'type': 'scan_y', 'y_value': y_value, 'qr_url': expired_qr,
            'device_id': 'test_phone_zht'
        }))
        msg = json.loads(await ws.recv())
        print(f"[MOBILE] 测试A结果: {json.dumps(msg, ensure_ascii=False, indent=2)}")

        # === 测试B: 匹配课程但无活跃签到 ===
        matched_signin_id = "5c7a6273-c8db-424f-8955-709b2938049b"
        y_value2 = str(int(time.time() * 1000))
        matched_qr = (f"https://acm.sztu.edu.cn:40130/hwsys/signin/signin_wall/"
                      f"{matched_signin_id}?server_now_validate={y_value2}")
        print(f"\n[MOBILE] 测试B: 匹配课程二维码")
        await ws.send(json.dumps({
            'type': 'scan_y', 'y_value': y_value2, 'qr_url': matched_qr,
            'device_id': 'test_phone_zht'
        }))
        msg2 = json.loads(await ws.recv())
        print(f"[MOBILE] 测试B结果: {json.dumps(msg2, ensure_ascii=False, indent=2)[:500]}")

        # === 测试C: 模拟有活跃任务时的批量执行流程 ===
        print("\n[MOBILE] 测试C: 模拟批量执行流程")
        # 伪造PC直接发送 signin_payloads（绕过扫码和匹配，直接测试批量执行）
        fake_payloads = [
            {
                'userx_key': '202500205105',
                'name': '曾浩天',
                'hwsys_token': 'fake_token_1',
                'signintask_id': 'fake_task_001',
                'server_now_validate': 'HWABCDEFG',
                'hash_info': 'HW1234567',
            },
            {
                'userx_key': '202500205106',
                'name': '同学B',
                'hwsys_token': 'fake_token_2',
                'signintask_id': 'fake_task_001',
                'server_now_validate': 'HWABCDEFG',
                'hash_info': 'HW1234568',
            },
        ]
        print(f"[MOBILE] 模拟收到 {len(fake_payloads)} 个payload，批量执行...")

        # 模拟并行执行所有签到（实际会失败因为token是假的）
        batch_results = []
        for p in fake_payloads:
            # 模拟执行（假装成功）
            batch_results.append({
                'userx_key': p['userx_key'],
                'name': p['name'],
                'status': 'success',
                'detail': '模拟批量签到成功',
            })
            print(f"  -> {p['name']}: success")

        # 一次性发送所有结果回PC
        await ws.send(json.dumps({
            'type': 'signin_results',
            'results': batch_results,
        }))
        print(f"[MOBILE] 已发送批量结果: {len(batch_results)} 条")

        await asyncio.sleep(1)

if __name__ == "__main__":
    try:
        asyncio.run(mock_mobile())
    except Exception as e:
        print(f"[MOBILE] 错误: {e}")
