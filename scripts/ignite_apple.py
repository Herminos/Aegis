#!/usr/bin/env python3
import os
import sys
import subprocess
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
EXE_PATH = PROJECT_ROOT / "build" / "AEGIS"
PORT = "7890"


def run_apple_script(cmd_string):
    script = f'tell application "Terminal" to do script "{cmd_string}"'
    subprocess.run(["osascript", "-e", script], check=True)


def main():
    if not EXE_PATH.exists():
        print(f"[-] 错误: 未能在路径下找到 AEGIS 二进制文件: {EXE_PATH}")
        print("[*] 请确保已经执行了 cmake --build build 编译项目。")
        sys.exit(1)

    if not os.access(EXE_PATH, os.X_OK):
        print("[*] 检测到 AEGIS 尚无可执行权限，正在自动赋予权限 (chmod +x)...")
        os.chmod(EXE_PATH, 0o755)

    print("[+] 正在唤醒 Terminal 部署 AEGIS 节点...")

    run_apple_script(f"exec {EXE_PATH} --listen {PORT}")
    run_apple_script(f"exec {EXE_PATH} --connect 127.0.0.1:{PORT}")

    print("[+] 双节点点火成功！请在弹出的终端窗口中查看 Aegis 的网络对账日志。")


if __name__ == "__main__":
    main()
