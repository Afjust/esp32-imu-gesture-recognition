"""
serial_reader.py
----------------
作用：
    打开串口，读一行，只保留格式正确的帧，打印出来。

先跑通这个，确认能稳定读到数据，再做后面的采集。
"""

import argparse
import serial
import time


def main():
    parser = argparse.ArgumentParser(description="读取并校验 ESP32 六轴串口数据")
    parser.add_argument("--port", required=True, help="串口，如 COM3 或 /dev/ttyUSB0")
    parser.add_argument("--baudrate", type=int, default=115200, help="波特率，默认 115200")
    args = parser.parse_args()

    # 1. 打开串口
    ser = serial.Serial(args.port, args.baudrate, timeout=1)

    # 2. 打开后等 1 秒，让 ESP32 稳定下来
    time.sleep(1)

    # 3. 清掉缓冲区里可能残留的旧数据
    ser.reset_input_buffer()

    print(f"opened {args.port}, reading... (Ctrl+C to stop)")

    # 4. 无限循环读
    while True:
        # 读一行，解码成字符串，去掉首尾空白
        line = ser.readline().decode("utf-8", errors="ignore").strip()

        # 只处理 $ 开头的行
        if not line.startswith("$"):
            continue

        # 去掉 $，按逗号分割
        parts = line[1:].split(",")

        # 必须正好 6 个字段
        if len(parts) != 6:
            continue

        # 6 个字段都必须能转成整数
        try:
            values = [int(x) for x in parts]
        except ValueError:
            continue

        # 打印
        print(values)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nstopped")
