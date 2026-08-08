import os
import shutil
import subprocess
import time
from pathlib import Path

# --- 配置区 ---
EXE_NAME = "AEGIS.exe"
PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build"
RUN_DIR = PROJECT_ROOT / "run"
PORT = "7890"


def setup_environment():
    exe_path = BUILD_DIR / EXE_NAME
    if not exe_path.exists():
        alt_path = BUILD_DIR / "Debug" / EXE_NAME
        if alt_path.exists():
            exe_path = alt_path
        else:
            print(f"❌ 找不到 {EXE_NAME}，请先确保编译成功。")
            return None

    if RUN_DIR.exists():
        shutil.rmtree(RUN_DIR)
    RUN_DIR.mkdir()

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

    print(f"🚀 正在点火：节点 A --listen {PORT}  /  节点 B --connect 127.0.0.1:{PORT} ...")

    process_a = subprocess.Popen(
        [str(node_a.absolute()), "--listen", PORT],
        creationflags=subprocess.CREATE_NEW_CONSOLE,
    )

    time.sleep(1)

    process_b = subprocess.Popen(
        [str(node_b.absolute()), "--connect", f"127.0.0.1:{PORT}"],
        creationflags=subprocess.CREATE_NEW_CONSOLE,
    )

    print("\n🔥 AEGIS 节点已在独立窗口运行。")
    print(f"   节点 A → --listen {PORT}（服务端）")
    print(f"   节点 B → --connect 127.0.0.1:{PORT}（客户端）")
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
