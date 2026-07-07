import os
import shutil
import subprocess
import time
from pathlib import Path

# --- 配置区 ---
EXE_NAME = "AEGIS.exe"
BUILD_DIR = Path("./build")
RUN_DIR = Path("./run")
PORT_A = " --lan 8080"
PORT_B = " --lan 8081"

def setup_environment():
    # 1. 检查 build 目录下是否有编译好的可执行文件
    #
    exe_path = BUILD_DIR / EXE_NAME
    if not exe_path.exists():
        # 如果是 MinGW 可能会在 Debug/Release 子目录下
        alt_path = BUILD_DIR / "Debug" / EXE_NAME
        if alt_path.exists():
            exe_path = alt_path
        else:
            print(f"❌ 找不到 {EXE_NAME}，请先确保编译成功。")
            return None

    # 2. 准备运行目录
    if RUN_DIR.exists():
        shutil.rmtree(RUN_DIR)
    RUN_DIR.mkdir()

    # 3. 复制两个副本，防止文件占用冲突
    node_a = RUN_DIR / "node_A.exe"
    node_b = RUN_DIR / "node_B.exe"
    shutil.copy2(exe_path, node_a)
    shutil.copy2(exe_path, node_b)
    
    print(f"✅ 环境就绪：副本已分发至 {RUN_DIR}")
    return node_a, node_b

def launch():
    nodes = setup_environment()
    if not nodes:
        return

    node_a, node_b = nodes

    print(f"🚀 正在点火：启动节点 A (端口 {PORT_A}) 和节点 B (端口 {PORT_B})...")

    # 4. 同时启动两个进程
    # 使用 subprocess.Popen 异步启动，不阻塞脚本
    process_a = subprocess.Popen([str(node_a.absolute()), PORT_A], 
                               creationflags=subprocess.CREATE_NEW_CONSOLE)
    
    # 稍微等一下，让第一个节点把 UDP 广播发出来
    time.sleep(1) 
    
    process_b = subprocess.Popen([str(node_b.absolute()), PORT_B], 
                               creationflags=subprocess.CREATE_NEW_CONSOLE)

    print("\n🔥 AEGIS 节点已在独立窗口运行。")
    print("等待 AETP 握手日志 (Ed25519 Verified -> ACTIVE)...")
    
    try:
        process_a.wait()
        process_b.wait()
    except KeyboardInterrupt:
        print("\n🛑 正在关闭节点...")
        process_a.terminate()
        process_b.terminate()

if __name__ == "__main__":
    launch()