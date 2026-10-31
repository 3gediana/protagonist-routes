@echo off
chcp 65001 >nul
echo ========================================
echo  SZTU ACM 签到服务端启动器
echo ========================================
echo.
echo 正在启动服务端...
echo 访问地址: http://127.0.0.1:8645
echo 外部访问: http://vip3.xiaomiqiu123.top:4924
echo.
echo 按 Ctrl+C 停止服务
echo ========================================
echo.
python "%~dp0signin_server.py"
pause
