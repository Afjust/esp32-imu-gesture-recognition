"""
export_tflite.py
----------------
量化导出 + 生成 C 数组 + 输出部署参数。
"""

import os
import numpy as np
import tensorflow as tf

from config import (
    LABELS, NUM_CLASSES, OUTPUT_NPZ,
    MODEL_PATH, INT8_PATH, C_ARRAY_NAME,
)


def load_data():
    data = np.load(OUTPUT_NPZ)
    return data["X_train"], data["X_test"], data["y_test"], data["mean"], data["std"]


def convert_int8(model, X_calib):
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    def rep_dataset():
        for i in range(len(X_calib)):
            yield [X_calib[i:i+1].astype(np.float32)]

    converter.representative_dataset = rep_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type  = tf.int8
    converter.inference_output_type = tf.int8
    return converter.convert()


def evaluate_tflite(tflite_path, X_test, y_test):
    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()
    inp = interpreter.get_input_details()[0]
    out = interpreter.get_output_details()[0]
    in_scale, in_zp = inp["quantization"]

    correct = 0
    for i in range(len(X_test)):
        x = X_test[i:i+1].astype(np.float32)
        # 必须与 ESP32 端 roundf() 的量化方式一致。直接 astype(np.int8)
        # 会向零截断，可能高估量化后的部署精度。
        x = np.rint(x / in_scale + in_zp)
        x = np.clip(x, -128, 127).astype(np.int8)
        interpreter.set_tensor(inp["index"], x)
        interpreter.invoke()
        output = interpreter.get_tensor(out["index"])
        out_scale, out_zp = out["quantization"]
        output = (output.astype(np.float32) - out_zp) * out_scale
        pred = int(np.argmax(output, axis=1)[0])
        if pred == y_test[i]:
            correct += 1
    return correct / len(y_test)


def export_c_array(tflite_bytes, out_path, array_name):
    with open(out_path, "w") as f:
        f.write(f"// auto-generated, size: {len(tflite_bytes)} bytes\n\n")
        f.write(f"const unsigned char {array_name}[] = {{\n")
        for i in range(0, len(tflite_bytes), 12):
            chunk = tflite_bytes[i:i+12]
            f.write("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        f.write("};\n\n")
        f.write(f"const unsigned int {array_name}_len = {len(tflite_bytes)};\n")


def main():
    print("=" * 60)
    print("量化导出")
    print("=" * 60)

    X_train, X_test, y_test, mean, std = load_data()

    print("\n[1] 加载 Keras 模型...")
    model = tf.keras.models.load_model(MODEL_PATH)
    print(f"    参数量: {model.count_params()}")

    loss, acc_f32 = model.evaluate(X_test, y_test, verbose=0)
    print(f"    float32 精度: {acc_f32:.4f}")

    print("\n[2] INT8 量化...")
    tflite_model = convert_int8(model, X_train)
    with open(INT8_PATH, "wb") as f:
        f.write(tflite_model)
    print(f"    大小: {len(tflite_model)} 字节 ({len(tflite_model)/1024:.1f} KB)")

    print("\n[3] 生成 C 数组...")
    c_array_path = os.path.splitext(INT8_PATH)[0] + ".h"
    export_c_array(tflite_model, c_array_path, C_ARRAY_NAME)
    print(f"    C 数组: {c_array_path}")

    print("\n[4] 评估 int8 精度...")
    acc_i8 = evaluate_tflite(INT8_PATH, X_test, y_test)
    print(f"    int8 精度: {acc_i8:.4f}")

    # 获取量化参数
    print("\n[5] 获取量化参数...")
    interp = tf.lite.Interpreter(model_path=INT8_PATH)
    interp.allocate_tensors()
    inp_det = interp.get_input_details()[0]
    out_det = interp.get_output_details()[0]

    print("\n" + "=" * 60)
    print("部署参数（复制到 ESP32 代码）")
    print("=" * 60)
    print(f"\n// 标签")
    print(f"static const char* LABELS[{NUM_CLASSES}] = {{" +
          ", ".join(f'"{l}"' for l in LABELS) + "};")
    print(f"\n// 标准化参数")
    print(f"static const float MEAN[{len(mean)}] = {{" +
          ", ".join(f"{v:.6f}f" for v in mean) + "};")
    print(f"static const float STD[{len(std)}] = {{" +
          ", ".join(f"{v:.6f}f" for v in std) + "};")
    print(f"\n// 量化参数")
    print(f"static const float INPUT_SCALE  = {inp_det['quantization'][0]:.10f}f;")
    print(f"static const int   INPUT_ZERO   = {inp_det['quantization'][1]};")
    print(f"static const float OUTPUT_SCALE = {out_det['quantization'][0]:.10f}f;")
    print(f"static const int   OUTPUT_ZERO  = {out_det['quantization'][1]};")
    print(f"\n// 模型")
    print(f'#include "{C_ARRAY_NAME}.h"')
    print(f"const tflite::Model* model = tflite::GetModel({C_ARRAY_NAME});")


if __name__ == "__main__":
    main()
