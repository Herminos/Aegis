import shutil
from pathlib import Path

def deploy_aegis():
    # 1. 定义源路径和目标路径（自动处理 Windows/Linux 的斜杠方向）
    current_dir = Path(__file__).parent.resolve()
    source_exe = current_dir / "build" / "AEGIS.exe"
    target_dir = current_dir / "run"

    print(f"[Aegis Deploy] 开始自动化部署...")
    print(f"[Aegis Deploy] 正在寻找源文件: {source_exe}")

    # 2. 安全检查：确保编译出的二进制文件确实存在
    if not source_exe.exists():
        print(f"[错误] 未找到 AEGIS.exe！请先确保 C++ 项目已成功 Build。")
        return

    # 3. 自动创建目标 /run 目录（如果不存在的话）
    if not target_dir.exists():
        print(f"[Aegis Deploy] 目标目录 {target_dir} 不存在，正在自动创建...")
        target_dir.mkdir(parents=True, exist_ok=True)

    # 4. 定义两份复制的目标文件名（以便多开调试状态机）
    destinations = [
        target_dir / "AEGIS_server.exe",
        target_dir / "AEGIS_client.exe"
    ]

    # 5. 执行物理复制
    for dest in destinations:
        try:
            # shutil.copy2 会连同文件的元数据（如修改时间）一起复制过去
            shutil.copy2(source_exe, dest)
            print(f"[成功] 已成功复制到: {dest}")
        except Exception as e:
            print(f"[错误] 复制到 {dest.name} 时失败: {e}")

    print("[Aegis Deploy] 部署完成！你现在可以去 /run 目录分别启动它们了。")

if __name__ == "__main__":
    deploy_aegis()