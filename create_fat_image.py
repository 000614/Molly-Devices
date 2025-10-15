import os
import subprocess
import sys

# --- 用户配置 ---

# Part 1: FAT文件系统生成配置
PARTITION_SIZE_KB = 7808          # 分区大小, 必须与 partitions.csv 中的大小完全一致
SOURCE_DIR = "storage"            # 源文件目录名 (项目根目录下的 'storage' 文件夹)
OUTPUT_BIN = "storage.bin"        # 输出的二进制镜像文件名
SECTOR_SIZE = 4096                # 分区扇区大小 (对于Flash通常是4096)

# Part 2: esptool.py 自动烧录配置
AUTO_FLASH = True                 # <--- 设置为 True 来自动烧录, 设置为 False 则只生成bin文件
ESPTOOL_PORT = "COM6"            # <--- 修改为您的设备端口号
ESPTOOL_BAUD = "921600"           # <--- 烧录波特率
ESPTOOL_CHIP = "esp32s3"          # <--- 您的芯片型号
FLASH_ADDRESS = "0x820000"        # <--- 目标分区的起始地址

# -----------------

def main():
    # --- 环境和路径设置 ---
    try:
        idf_path = os.environ["IDF_PATH"]
    except KeyError:
        print("错误: IDF_PATH 环境变量未设置。请先运行 'export.bat' 或 'export.ps1'。")
        sys.exit(1)

    fatfsgen_py = os.path.join(idf_path, "components", "fatfs", "fatfsgen.py")
    script_dir = os.path.dirname(os.path.abspath(__file__))
    source_path = os.path.join(script_dir, SOURCE_DIR)
    output_path = os.path.join(script_dir, OUTPUT_BIN)

    if not os.path.isdir(source_path):
        print(f"错误: 源目录 '{source_path}' 不存在。")
        sys.exit(1)

    # --- 步骤 1: 生成 FAT 文件系统镜像 ---
    print("--- 步骤 1: 开始生成 FAT 文件系统镜像 ---")
    partition_size_bytes = PARTITION_SIZE_KB * 1024
    gen_command = [
        sys.executable,
        fatfsgen_py,
        "--partition_size", str(partition_size_bytes),
        "--sector_size", str(SECTOR_SIZE),
        "--output_file", output_path,
        source_path,
    ]

    try:
        print(f"执行命令: {' '.join(gen_command)}")
        result = subprocess.run(gen_command, check=True, text=True, capture_output=True, shell=True)
        print(result.stdout)
        print(f"\n成功！镜像文件 '{OUTPUT_BIN}' 已生成。")
    except subprocess.CalledProcessError as e:
        print("\n错误: 生成镜像失败。")
        print("--- fatfsgen.py 输出 ---")
        print(e.stdout)
        print(e.stderr)
        print("------------------------")
        sys.exit(1)

    # --- 步骤 2: 烧录镜像到设备 (如果启用) ---
    if not AUTO_FLASH:
        print("\nAUTO_FLASH 设置为 False，跳过烧录步骤。")
        return

    print(f"\n--- 步骤 2: 开始烧录 '{OUTPUT_BIN}' 到设备 ---")
    
    # 构造 esptool.py 的完整路径，使其更可靠
    esptool_py = os.path.join(idf_path, "components", "esptool_py", "esptool", "esptool.py")

    flash_command = [
        sys.executable,
        esptool_py,
        "--chip", ESPTOOL_CHIP,
        "-p", ESPTOOL_PORT,
        "-b", ESPTOOL_BAUD,
        "write_flash",
        FLASH_ADDRESS,
        output_path, # 使用变量 output_path 指向生成的 bin 文件
    ]

    try:
        print(f"执行命令: {' '.join(flash_command)}")
        # 这里不使用 capture_output，以便 esptool.py 的进度条能实时显示在控制台
        subprocess.run(flash_command, check=True, shell=True)
        print(f"\n成功！镜像已烧录到地址 {FLASH_ADDRESS}。")
    except subprocess.CalledProcessError:
        print("\n错误: 烧录失败。请检查端口号、设备连接或 esptool.py 的输出。")
        sys.exit(1)
    except FileNotFoundError:
        print(f"\n错误: 找不到 esptool.py。路径 '{esptool_py}' 是否正确？")
        sys.exit(1)


if __name__ == "__main__":
    main()