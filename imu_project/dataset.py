"""
dataset.py
----------
读取 CSV，重采样，划分，标准化，保存 data.npz。

所有配置从 config.py 读。
"""

import glob
import os
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split

from config import (
    LABELS, LABEL_MAP, NUM_CLASSES,
    SEQ_LEN, NUM_AXES, DATA_DIR, OUTPUT_NPZ,
    TEST_RATIO, VAL_RATIO, RANDOM_SEED,
)


def load_one_csv(path):
    df = pd.read_csv(path)
    seq = df[["ax", "ay", "az", "gx", "gy", "gz"]].values.astype(np.float32)
    return seq


def resample(seq, target_len):
    T, C = seq.shape
    if T == target_len:
        return seq
    old_idx = np.linspace(0, 1, T)
    new_idx = np.linspace(0, 1, target_len)
    out = np.zeros((target_len, C), dtype=np.float32)
    for c in range(C):
        out[:, c] = np.interp(new_idx, old_idx, seq[:, c])
    return out


def load_all():
    files = sorted(glob.glob(os.path.join(DATA_DIR, "*.csv")))
    print(f"找到 {len(files)} 个 CSV 文件")

    X_list, y_list = [], []

    for f in files:
        name = os.path.basename(f)
        label_str = name.split("_")[0]

        if label_str not in LABEL_MAP:
            print(f"  跳过：{name}（未知标签 {label_str}）")
            continue

        seq = load_one_csv(f)
        if len(seq) < 50:
            print(f"  跳过：{name}（只有 {len(seq)} 帧）")
            continue

        seq = resample(seq, SEQ_LEN)
        X_list.append(seq)
        y_list.append(LABEL_MAP[label_str])

    X = np.stack(X_list)
    y = np.array(y_list, dtype=np.int64)
    print(f"加载完成：X={X.shape}, y={y.shape}")
    return X, y


def split(X, y):
    X_tmp, X_test, y_tmp, y_test = train_test_split(
        X, y, test_size=TEST_RATIO, stratify=y, random_state=RANDOM_SEED,
    )
    val_ratio_adjusted = VAL_RATIO / (1 - TEST_RATIO)
    X_train, X_val, y_train, y_val = train_test_split(
        X_tmp, y_tmp, test_size=val_ratio_adjusted,
        stratify=y_tmp, random_state=RANDOM_SEED,
    )
    print(f"train={X_train.shape}  val={X_val.shape}  test={X_test.shape}")
    return X_train, y_train, X_val, y_val, X_test, y_test


def normalize(X_train, X_val, X_test):
    mean = X_train.mean(axis=(0, 1))
    std  = X_train.std(axis=(0, 1))
    std = np.where(std < 1e-6, 1.0, std)
    print(f"均值：{mean}")
    print(f"标准差：{std}")
    return (
        (X_train - mean) / std,
        (X_val   - mean) / std,
        (X_test  - mean) / std,
        mean, std,
    )


def main():
    X, y = load_all()
    X_train, y_train, X_val, y_val, X_test, y_test = split(X, y)
    X_train, X_val, X_test, mean, std = normalize(X_train, X_val, X_test)

    np.savez(
        OUTPUT_NPZ,
        X_train=X_train, y_train=y_train,
        X_val=X_val,     y_val=y_val,
        X_test=X_test,   y_test=y_test,
        mean=mean,       std=std,
        labels=np.array(LABELS),
    )
    print(f"保存到 {OUTPUT_NPZ}")

    print("\n类别分布：")
    for i, name in enumerate(LABELS):
        n_train = int((y_train == i).sum())
        n_val   = int((y_val   == i).sum())
        n_test  = int((y_test  == i).sum())
        print(f"  {name:8s}  train={n_train:3d}  val={n_val:3d}  test={n_test:3d}")


if __name__ == "__main__":
    main()