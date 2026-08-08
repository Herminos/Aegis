import shutil
from pathlib import Path

def deploy_aegis():
    project_root = Path(__file__).resolve().parent.parent  # scripts/ → 项目根
    source_exe = project_root / "build" / "AEGIS.exe"
    target_dir = project_root / "run"

    print(f"[Aegis Deploy] 开始自动化部署...")
    print(f"[Aegis Deploy] 源文件: {source_exe}")

    if not source_exe.exists():
        print(f"[错误] 未找到 {source_exe.name}！请先确保 C++ 项目已成功 Build。")
        return

    if not target_dir.exists():
        print(f"[Aegis Deploy] 目标目录 {target_dir} 不存在，正在自动创建...")
        target_dir.mkdir(parents=True, exist_ok=True)

    destinations = [
        target_dir / "AEGIS_server.exe",
        target_dir / "AEGIS_client.exe",
    ]

    for dest in destinations:
        try:
            shutil.copy2(source_exe, dest)
            print(f"[成功] 已复制到: {dest}")
        except Exception as e:
            print(f"[错误] 复制到 {dest.name} 时失败: {e}")

    print("[Aegis Deploy] 部署完成！")


if __name__ == "__main__":
    deploy_aegis()
