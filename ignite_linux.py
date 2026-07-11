#!/usr/bin/env python3
import os
import shutil
import subprocess
import time
import sys
from pathlib import Path


# --- 配置区 ---
EXE_NAME = "AEGIS"
BUILD_DIR = Path("./build")
RUN_DIR = Path("./run")
PORT_A = "7890"
PORT_B = "7890"

# Linux 下常见的终端模拟器及其启动命令
TERMINALS = [
    # (命令名, 启动参数: 在新窗口中执行 cmd)
    ("gnome-terminal",  ["--", "bash", "-c"]),
    ("konsole",         ["-e", "bash", "-c"]),
    ("xfce4-terminal",  ["-e", "bash", "-c"]),
    ("lxterminal",      ["-e", "bash", "-c"]),
    ("terminator",      ["-x", "bash", "-c"]),
    ("kitty",           ["bash", "-c"]),
    ("alacritty",       ["-e", "bash", "-c"]),
    ("xterm",           ["-e", "bash", "-c"]),
]


def find_terminal():
    """按优先级查找系统中可用的终端模拟器"""
    for term_name, term_args in TERMINALS:
        if shutil.which(term_name):
            return term_name, term_args
    return None, None


def setup_environment():
    """准备运行环境：检查可执行文件，复制副本到 run/ 目录"""
    script_dir = Path(__file__).resolve().parent
    build_dir = script_dir / BUILD_DIR

    # 1. 在 build 目录下查找可执行文件
    exe_path = build_dir / EXE_NAME
    if not exe_path.exists():
        # 某些构建系统会把产物放在 Debug / Release 子目录
        for sub in ("Debug", "Release", "RelWithDebInfo", "MinSizeRel"):
            alt_path = build_dir / sub / EXE_NAME
            if alt_path.exists():
                exe_path = alt_path
                break
        else:
            print(f"❌ 找不到 {EXE_NAME}，请先确保编译成功。")
            print(f"   已搜索: {build_dir} 及其 Debug/Release 子目录")
            return None

    # 2. 确保有可执行权限
    if not os.access(exe_path, os.X_OK):
        print("[*] 添加可执行权限 (chmod +x) ...")
        os.chmod(exe_path, 0o755)

    # 3. 准备运行目录（避免文件占用冲突）
    run_dir = script_dir / RUN_DIR
    if run_dir.exists():
        shutil.rmtree(run_dir)
    run_dir.mkdir()

    # 4. 复制两个副本
    node_a = run_dir / f"node_A_{EXE_NAME}"
    node_b = run_dir / f"node_B_{EXE_NAME}"
    shutil.copy2(exe_path, node_a)
    shutil.copy2(exe_path, node_b)

    print(f"✅ 环境就绪：副本已分发至 {run_dir}")
    return node_a, node_b


def launch():
    """点火：在独立终端窗口中启动两个 AEGIS 节点"""
    term_name, term_args = find_terminal()
    if not term_name:
        print("❌ 未找到可用的终端模拟器 (gnome-terminal / konsole / xterm ...)")
        print("   请手动执行:")
        print(f"     ./build/{EXE_NAME} {PORT_A}")
        print(f"     ./build/{EXE_NAME} {PORT_B}")
        sys.exit(1)

    print(f"[*] 使用终端: {term_name}")

    nodes = setup_environment()
    if not nodes:
        sys.exit(1)

    node_a, node_b = nodes

    print(f"🚀 正在点火：节点 A --listen {PORT_A}  /  节点 B --connect 127.0.0.1:{PORT_B} ...")

    # 启动节点 A：在新终端中保持进程存活
    cmd_a = f"\"{node_a.absolute()}\" --listen {PORT_A}; echo '节点 A 已退出，按 Enter 关闭...'; read"
    process_a = subprocess.Popen(
        [term_name] + term_args + [cmd_a],
        stdout=subprocess.DEVNULL,
    )

    # 稍等让第一个节点发出 UDP 广播
    time.sleep(1.5)

    # 启动节点 B
    cmd_b = f"\"{node_b.absolute()}\" --connect 127.0.0.1:{PORT_B}; echo '节点 B 已退出，按 Enter 关闭...'; read"
    process_b = subprocess.Popen(
        [term_name] + term_args + [cmd_b],
        stdout=subprocess.DEVNULL,
    )

    print("\n🔥 AEGIS 节点已在独立终端窗口中运行。")
    print(f"   节点 A → --listen {PORT_A}（服务端）")
    print(f"   节点 B → --connect 127.0.0.1:{PORT_B}（客户端）")
    print("等待 AETP 握手日志 (Ed25519 Verified -> ACTIVE) ...")
    print("\n按 Ctrl+C 关闭所有节点窗口。")

    try:
        process_a.wait()
        process_b.wait()
    except KeyboardInterrupt:
        print("\n🛑 正在关闭节点...")
        process_a.terminate()
        process_b.terminate()


if __name__ == "__main__":
    launch()
