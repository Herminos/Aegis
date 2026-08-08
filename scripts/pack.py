import os
from pathlib import Path

def create_review_snapshot():
    project_root = Path(__file__).resolve().parent.parent  # scripts/ → 项目根
    output_file = project_root / "aegis_review_snapshot.txt"

    target_extensions = {".cpp", ".hpp", ".txt", ".md"}
    ignored_dirs = {"build", ".git", ".vscode", "__pycache__", "third_party", "run"}
    separator = "\n" + "=" * 80 + "\n"

    print(f"🔍 正在扫描 Aegis 项目源码...")

    files_processed = 0

    with open(output_file, "w", encoding="utf-8") as f_out:
        f_out.write(f"AEGIS PROJECT SOURCE SNAPSHOT\nGenerated at: {project_root}\n")
        f_out.write("=" * 80 + "\n\n")

        for file_path in sorted(project_root.rglob("*")):
            if any(part in ignored_dirs for part in file_path.parts):
                continue

            if file_path.suffix in target_extensions and file_path.is_file():
                if file_path.name == Path(__file__).name or file_path.name == output_file.name:
                    continue

                relative_path = file_path.relative_to(project_root)
                print(f"  [+] 读取: {relative_path}")

                f_out.write(separator)
                f_out.write(f"FILE PATH: {relative_path}\n")
                f_out.write(separator)

                try:
                    with open(file_path, "r", encoding="utf-8") as f_in:
                        f_out.write(f_in.read())
                    f_out.write("\n")
                    files_processed += 1
                except Exception as e:
                    f_out.write(f"[ERROR] 无法读取文件: {e}\n")

    print(f"\n✅ 成功！全量快照已生成: {output_file.name}")
    print(f"📊 共整合 {files_processed} 个源文件。")


if __name__ == "__main__":
    create_review_snapshot()
