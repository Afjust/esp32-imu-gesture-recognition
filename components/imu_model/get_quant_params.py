import tensorflow as tf

interp = tf.lite.Interpreter("imu_model_int8.tflite")
interp.allocate_tensors()

inp = interp.get_input_details()[0]
out = interp.get_output_details()[0]

print("输入:")
print("  shape:", inp["shape"])
print("  dtype:", inp["dtype"])
print("  scale:", inp["quantization"][0])
print("  zero_point:", inp["quantization"][1])

print("\n输出:")
print("  shape:", out["shape"])
print("  dtype:", out["dtype"])
print("  scale:", out["quantization"][0])
print("  zero_point:", out["quantization"][1])