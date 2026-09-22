"""
model.py
--------
模型定义，从 config.py 读配置。

目前只保留 4layer 架构（最优）。
"""

import tensorflow as tf
from tensorflow.keras import layers, Model

from config import (
    NUM_CLASSES, SEQ_LEN, NUM_AXES,
    CONV1_FILTERS, CONV1_KERNEL, CONV1_STRIDES,
)


def build_model(input_shape=(SEQ_LEN, NUM_AXES), num_classes=NUM_CLASSES):
    """4layer 极限架构"""
    inputs = layers.Input(shape=input_shape, name="input")

    x = layers.Conv1D(
        filters=CONV1_FILTERS,
        kernel_size=CONV1_KERNEL,
        strides=CONV1_STRIDES,
        padding="same",
        activation="relu",
        name="conv1"
    )(inputs)

    x = layers.GlobalAveragePooling1D(name="gap")(x)
    outputs = layers.Dense(num_classes, name="logits")(x)

    model = Model(inputs=inputs, outputs=outputs, name="imu_4layer")
    return model


if __name__ == "__main__":
    model = build_model()
    model.summary()