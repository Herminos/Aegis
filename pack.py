import os
from pathlib import Path

def create_review_snapshot():
    # --- 1. 配置 (Configuration) ---
    project_root = Path(__file__).parent
    output_file = project_root / "aegis_review_snapshot.txt"
    
    # 你需要关注的文件类型
    target_extensions = {".cpp", ".hpp", ".txt", ".md"}
    # 必须排除的目录，防止把编译产物或巨大的依赖打进去
    ignored_dirs = {"build", ".git", ".vscode", "__pycache__", "third_party"}
    
    # 分隔符：让 AI 能清晰分辨文件边界
    separator = "\n" + "="*80 + "\n"

    print(f"🔍 正在扫描 Aegis 项目源码...")
    
    files_processed = 0

    # --- 2. 写入流程 (Execution) ---
    with open(output_file, "w", encoding="utf-8") as f_out:
        f_out.write(f"AEGIS PROJECT SOURCE SNAPSHOT\nGenerated at: {Path.cwd()}\n")
        f_out.write("="*80 + "\n\n")

        # 递归遍历项目
        for file_path in sorted(project_root.rglob("*")):
            
            # 过滤逻辑
            if any(part in ignored_dirs for part in file_path.parts):
                continue
            
            if file_path.suffix in target_extensions and file_path.is_file():
                # 跳过脚本自身，避免套娃
                if file_path.name == Path(__file__).name or file_path.name == output_file.name:
                    continue
                
                relative_path = file_path.relative_to(project_root)
                print(f"  [+] 读取: {relative_path}")
                
                # 写入页眉：文件路径
                f_out.write(separator)
                f_out.write(f"FILE PATH: {relative_path}\n")
                f_out.write(separator)
                
                # 写入内容
                try:
                    with open(file_path, "r", encoding="utf-8") as f_in:
                        f_out.write(f_in.read())
                    f_out.write("\n")
                    files_processed += 1
                except Exception as e:
                    f_out.write(f"[ERROR] 无法读取文件: {e}\n")

    print(f"\n✅ 成功！全量快照已生成: {output_file.name}")
    print(f"📊 共整合 {files_processed} 个源文件。你可以直接把这个文本文件的内容发给我。")

if __name__ == "__main__":
    create_review_snapshot()