"""
record.py
---------
作用：
    读串口 -> 录一段固定时长 -> 存一个 CSV 文件

用法：
    python record.py --port COM3 --label circle --count 50
"""

import argparse
import csv
import os
import time

import serial


def record_one(ser, label, out_dir, duration):
    """
    录一段，存一个 CSV。
    参数：
        ser      已打开的串口对象
        label    标签：idle / circle / wave
        out_dir  输出目录
        duration 录制时长（秒）
    """
    # 建目录（如果不存在）
    os.makedirs(out_dir, exist_ok=True)

    # 文件名：标签_时间戳.csv
    fname = f"{label}_{int(time.time() * 1000)}.csv"
    fpath = os.path.join(out_dir, fname)

    # 清缓冲，避免上一段的尾巴混进来
    ser.reset_input_buffer()

    # 打开 CSV 边读边写
    with open(fpath, "w", newline="") as f:
        writer = csv.writer(f)

        # 表头
        writer.writerow(["timestamp", "ax", "ay", "az", "gx", "gy", "gz", "label"])

        t0 = time.time()
        count = 0

        # 录到时间到
        while time.time() - t0 < duration:
            # 读一行
            line = ser.readline().decode("utf-8", errors="ignore").strip()

            # 过滤 1：必须 $ 开头
            if not line.startswith("$"):
                continue

            # 过滤 2：必须 6 个字段
            parts = line[1:].split(",")
            if len(parts) != 6:
                continue

            # 过滤 3：都能转 int
            try:
                values = [int(x) for x in parts]
            except ValueError:
                continue

            # 写一行
            writer.writerow([time.time(), *values, label])
            f.flush()
            count += 1

    print(f"saved {fname}  frames={count}")
    return fpath


def main():
    # 命令行参数
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="串口，如 COM3 或 /dev/ttyUSB0")
    parser.add_argument("--label", required=True,
                        choices=["idle", "circle", "wave", "shake", "figure8"],
                        help="动作标签")
    parser.add_argument("--out", default="data", help="输出目录")
    parser.add_argument("--duration", type=float, default=2.0,
                        help="每段时长（秒）")
    parser.add_argument("--count", type=int, default=50,
                        help="采集段数")
    args = parser.parse_args()

    # 打开串口
    ser = serial.Serial(args.port, 115200, timeout=1)
    time.sleep(1)
    ser.reset_input_buffer()

    print(f"opened {args.port}")
    print(f"label={args.label}  count={args.count}  duration={args.duration}s")
    print("-" * 40)

    try:
        for i in range(args.count):
            # 等你准备好
            input(f"[{i+1}/{args.count}] 准备 {args.label}，按回车开始...")
            # 录一段
            record_one(ser, args.label, args.out, args.duration)
            # 段间停半秒
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n中断")
    finally:
        ser.close()
        print("done")


if __name__ == "__main__":
    main()
