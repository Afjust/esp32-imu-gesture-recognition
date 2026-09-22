"""
verify.py
---------
作用：
    读取 data/ 下所有 CSV，统计每个标签的样本数、帧数，
    并用 Pandas 看 head / describe，确认数据完整。

用法：
    python verify.py
    python verify.py --data data
"""

import argparse
import glob
import os

import pandas as pd


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default="data", help="数据目录，默认 data/")
    args = parser.parse_args()

    # 找到 data/ 下所有 csv
    pattern = os.path.join(args.data, "*.csv")
    files = sorted(glob.glob(pattern))

    print(f"found {len(files)} csv files\n")

    if not files:
        print("没有找到 CSV，检查 --data 路径是否正确")
        return

    # 统计每个标签
    stats = {}

    for f in files:
        # 从文件名取标签：circle_1710000000000.csv -> circle
        label = os.path.basename(f).split("_")[0]

        # 读 CSV
        df = pd.read_csv(f)

        # 累加
        stats.setdefault(label, {"files": 0, "frames": 0})
        stats[label]["files"] += 1
        stats[label]["frames"] += len(df)

    # 打印统计
    print("样本统计：")
    for label, s in stats.items():
        avg = s["frames"] / s["files"] if s["files"] else 0
        print(f"  {label:8s}  files={s['files']:3d}  frames={s['frames']:6d}  avg={avg:.1f}")

    # 挑一个文件看内容
    sample = files[0]
    print(f"\n示例文件：{sample}")
    df = pd.read_csv(sample)
    print("\nhead:")
    print(df.head())
    print("\ndescribe:")
    print(df.describe())
    print(f"\nshape: {df.shape}")


if __name__ == "__main__":
    main()