"""
train.py
--------
训练模型。
"""

import numpy as np
import tensorflow as tf

from config import (
    LABELS, NUM_CLASSES, OUTPUT_NPZ, MODEL_PATH,
    BATCH_SIZE, EPOCHS, LR,
)
from model import build_model


def load_data():
    data = np.load(OUTPUT_NPZ)
    return (
        data["X_train"], data["y_train"],
        data["X_val"],   data["y_val"],
        data["X_test"],  data["y_test"],
    )


def main():
    X_train, y_train, X_val, y_val, X_test, y_test = load_data()
    print(f"train: {X_train.shape}  val: {X_val.shape}  test: {X_test.shape}")
    print(f"类别数: {NUM_CLASSES}")
    print(f"标签: {LABELS}")

    model = build_model()
    model.summary()

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=LR),
        loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
        metrics=["accuracy"],
    )

    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_loss", patience=30,
            restore_best_weights=True, verbose=1),
        tf.keras.callbacks.ModelCheckpoint(
            MODEL_PATH, monitor="val_loss",
            save_best_only=True, verbose=1),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss", factor=0.5,
            patience=10, min_lr=1e-6, verbose=1),
    ]

    model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        batch_size=BATCH_SIZE,
        epochs=EPOCHS,
        callbacks=callbacks,
        verbose=1,
    )

    loss, acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"\n测试集：loss={loss:.4f}  accuracy={acc:.4f}")

    logits = model.predict(X_test, verbose=0)
    y_pred = np.argmax(logits, axis=1)

    print("\n混淆矩阵：")
    print("        ", "  ".join(f"{l:>8s}" for l in LABELS))
    for i, name in enumerate(LABELS):
        row = []
        for j in range(len(LABELS)):
            n = int(((y_test == i) & (y_pred == j)).sum())
            row.append(f"{n:>8d}")
        print(f"  {name:8s}", "  ".join(row))

    print(f"\n模型已保存到 {MODEL_PATH}")


if __name__ == "__main__":
    main()