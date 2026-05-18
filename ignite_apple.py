#!/usr/bin/env python3
import os
import sys
import subprocess

def run_apple_script(cmd_string):
    """通过 AppleScript 在新终端窗口中执行命令"""
    # 告诉 Terminal.app 开启新窗口并运行指定命令
    script = f'tell application "Terminal" to do script "{cmd_string}"'
    subprocess.run(["osascript", "-e", script], check=True)

def main():
    # 1. 动态获取当前脚本所在目录的绝对路径，确保定位精准
    current_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 2. 定位 build 目录下的 AEGIS 二进制文件
    # 如果你的脚本放在项目根目录下，这里就是 os.path.join(current_dir, "build", "AEGIS")
    # 如果脚本本身就在 build 目录下，可以把 "build" 删掉
    target_binary = os.path.join(current_dir, "build", "AEGIS")
    
    # 3. 容错检查：防止还没编译就盲目点火
    if not os.path.exists(target_binary):
        print(f"[-] 错误: 未能在路径下找到 AEGIS 二进制文件: {target_binary}")
        print("[*] 请确保已经执行了 cmake --build build 编译项目。")
        sys.exit(1)
        
    # 4. 检查文件的可执行权限 (Mac 上的老传统了)
    if not os.access(target_binary, os.X_OK):
        print("[*] 检测到 AEGIS 尚无可执行权限，正在自动赋予权限 (chmod +x)...")
        os.chmod(target_binary, 0o755)

    print("[+] 正在唤醒 Terminal 部署 AEGIS 节点...")

    # 5. 分别传入 8080 和 8081 参数，在两个新窗口中点火
    # 使用 exec 可以让进程直接接管终端，关闭终端时进程会优雅退出
    run_apple_script(f"exec {target_binary} 9247")
    run_apple_script(f"exec {target_binary} 9471")

    print("[+] 双节点点火成功！请在弹出的终端窗口中查看 Aegis 的网络对账日志。")

if __name__ == "__main__":
    main()