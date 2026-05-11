import os
from pathlib import Path
import datetime

def pack_aegis_code():
    # 配置区
    target_dir = Path('.')
    output_filename = 'aegis_full_context.txt'
    # 核心修改：在这里加上了 '.txt'
    target_extensions = {'.cpp', '.hpp', '.h', '.txt'} 
    # 自动过滤掉 CMake 生成的编译测试文件和 git 历史
    exclude_dirs = {'.git', 'build', 'out', 'bin', '.vscode'}

    files_to_pack = []
    
    # 遍历目录
    for file_path in target_dir.rglob('*'):
        # 过滤排除目录
        if any(exclude in file_path.parts for exclude in exclude_dirs):
            continue
            
        if file_path.is_file() and file_path.suffix in target_extensions:
            # 过滤掉脚本自己生成的那个 txt 产物，防止无限套娃
            if file_path.name == output_filename:
                continue
            files_to_pack.append(file_path)

    # 按文件路径字母顺序排序，让相关联的 cpp 和 hpp 挨在一起
    files_to_pack.sort()

    with open(output_filename, 'w', encoding='utf-8') as out:
        # 1. 写入全局元数据
        out.write("=== Aegis Codebase Context ===\n")
        out.write(f"Generated at: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        out.write(f"Total files: {len(files_to_pack)}\n\n")

        # 2. 写入项目结构索引
        out.write("--- Project Structure ---\n")
        for f in files_to_pack:
            out.write(f"- {f.relative_to(target_dir)}\n")
        out.write("\n========================================\n\n")

        # 3. 逐个写入文件内容
        for f in files_to_pack:
            rel_path = f.relative_to(target_dir)
            
            out.write(f"// " + "="*50 + "\n")
            out.write(f"// --- [ FILE: {rel_path} ] ---\n")
            out.write(f"// " + "="*50 + "\n")
            
            # 针对 txt 协议文档使用纯文本代码块，cpp 使用 cpp 代码块
            if f.suffix == '.txt':
                out.write("```text\n")
            else:
                out.write("```cpp\n")
                
            try:
                with open(f, 'r', encoding='utf-8') as infile:
                    out.write(infile.read())
            except UnicodeDecodeError:
                with open(f, 'r', encoding='gbk', errors='ignore') as infile:
                    out.write(infile.read())
            
            out.write("\n```\n\n\n")

    print(f"[SUCCESS] 已经成功将 {len(files_to_pack)} 个文件（包含协议 txt）打包至 -> {output_filename}")

if __name__ == '__main__':
    pack_aegis_code()