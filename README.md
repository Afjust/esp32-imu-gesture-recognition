# ESP32 + MPU6050 Five-Class IMU Gesture Recognition(A student practice project)

An end-to-end gesture recognition project built around an ESP32 and an MPU6050. The project covers serial data collection, dataset preparation, 1D-CNN training, INT8 quantization, TensorFlow Lite Micro deployment, and on-device inference.

Supported classes: `idle`, `circle`, `wave`, `shake`, and `figure8`.

## Features

- Collects six-axis MPU6050 data over I2C using GPIO21 for SDA and GPIO22 for SCL at a nominal rate of 100 Hz.
- Resamples each gesture sequence to `200 x 6` and creates train, validation, and test splits with a fixed random seed.
- Exports a Keras model, an INT8 TFLite model, and a C array ready for firmware integration.
- Runs inference with TensorFlow Lite Micro on ESP32 and supports separate collection and inference modes.

## Hardware and Sampling

| Item | Configuration |
| --- | --- |
| MCU | ESP32-D0WD |
| Sensor | MPU6050 |
| I2C | SDA=GPIO21, SCL=GPIO22, 400 kHz |
| Accelerometer range | +/-8 g |
| Gyroscope range | +/-2000 deg/s |
| Digital low-pass filter | DLPF=3, approximately 44 Hz bandwidth |
| Target sampling rate | 100 Hz |

The firmware uses `vTaskDelay(pdMS_TO_TICKS(10))` to schedule samples. There is no hardware timer trigger, and the recorded data does not contain a device-side sampling timestamp. The 100 Hz rate should therefore be treated as an approximation.

## Project Structure

```text
hello_world/
|-- main/
|   |-- CMakeLists.txt          # APP_MODE=collect or infer
|   |-- config.h                # Labels, normalization, quantization, model selection
|   |-- main_collect.cpp        # Serial collection mode
|   `-- main_infer.cpp          # On-device inference mode
|-- components/
|   |-- mpu6050/                # MPU6050 driver
|   `-- imu_model/              # INT8 model header used by the firmware
|-- imu_project/
|   |-- config.py               # PC-side global configuration
|   |-- dataset.py              # CSV loading, resampling, splitting, and normalization
|   |-- model.py                # 4layer model definition
|   |-- train.py                # Training and test-set evaluation
|   |-- export_tflite.py        # PTQ INT8 conversion and C array export
|   |-- record.py               # Serial data recording
|   |-- serial_reader.py        # Serial data inspection
|   |-- verify.py               # CSV dataset statistics
|   |-- data/                   # Raw CSV files
|   `-- models/                 # Exported model versions
|-- docs/evidence/
|   `-- board_inference_log.txt # Measured arena usage and latency
`-- CMakeLists.txt
```

## Model

The final 4layer model has the following computation graph:

```text
Input(200, 6)
  -> Conv1D(8 filters, kernel=3, stride=2, ReLU)
  -> GlobalAveragePooling1D
  -> Dense(5)
```

The converted TensorFlow Lite Micro model uses operators including `CONV_2D`, `MEAN`, and `FULLY_CONNECTED`.

The repository contains the following model artifacts:

| Version | Parameters | INT8 Size | Reported Accuracy | Status |
| --- | ---: | ---: | ---: | --- |
| pruned70 | 1,240 | 11,784 B | Historical maximum: 100% | Best historical experiment; the matching training script and logs are incomplete |
| tiny1 | 837 | 5,880 B | 95.83% | Historical training result; the training script was not retained |
| 4layer | 197 | 3,616 B | Historical maximum: 92.11% | Final deployment architecture; accuracy must be confirmed by retraining |

The model is approximately 6 times smaller than the original 14-layer version. The reported pure inference latency decreased from 63 ms to 3.7 ms, approximately a 17 times improvement. The original 14-layer model and its training logs are not included in this repository.

The 4layer model reached a historical maximum of 92.11%, demonstrating that the architecture can reach this level with a high-quality training set. This is a historical result and does not correspond to the current `data.npz`. Before final submission, retrain, quantize, and evaluate with a fixed dataset version.

The measured board-side arena usage and inference latency are available in [board_inference_log.txt](docs/evidence/board_inference_log.txt).

Size, latency, and accuracy values from external projects are often measured with different hardware, datasets, quantization methods, and evaluation protocols. They cannot be used directly to claim that this model is universally optimal in size or speed.

## Model Evolution Comparison

The tables below summarize the development history from the original model to the final 4layer version. They are historical measurements from different training and evaluation runs, not results from one controlled benchmark.

### Float32 and INT8 Comparison

| Version | Layers | Parameters | Float32 Size | INT8 Size | Accuracy | Latency | Arena |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Original | 14 | 10,069 | 45.2 KB | 20.9 KB | 100% | 63 ms | 9.2 KB |
| pruned70 | 14 | 1,240 | approximately 15 KB | 11.5 KB | 97.22%, historical maximum 100% | - | - |
| tiny1 | 7 | 837 | approximately 8 KB | 5.7 KB | 95.83% | 21 ms | 7.7 KB |
| **4layer** | **4** | **197** | **approximately 4 KB** | **3.5 KB** | **92.11%, approximately 92%** | **3.7 ms** | **3.4 KB** |

### Compression and Speedup

| Version | Float32 Compression vs Original | INT8 Compression vs Float32 | Speedup vs Original |
| --- | ---: | ---: | ---: |
| Original | 1.0x | 2.2x | 1.0x |
| pruned70 | 3.0x | 1.3x | - |
| tiny1 | 5.7x | 1.4x | 3.0x |
| **4layer** | **11.3x** | **1.1x** | **17x** |

The INT8 compression values are calculated from the sizes listed in the previous table. They are 2.2x, 1.3x, 1.4x, and 1.1x, respectively.

### Key Findings

| Finding | Historical Data |
| --- | --- |
| Layer reduction versus parameter pruning | 90% pruning: 14 layers, 234 parameters, 10.0 KB. 4layer: 4 layers, 197 parameters, 3.5 KB. |
| Layer count and accuracy tradeoff | 14 layers: 100%. 7 layers: 95.83%. 4 layers: 92.11%. |
| Approximate fixed overhead | 14 layers: approximately 9.5 KB. 7 layers: approximately 4.5 KB. 4 layers: approximately 3 KB. |
| Estimated saving per removed layer | Approximately 0.5-0.7 KB. |

The fixed-overhead estimates use the simplified approximation:

```text
float32_size = int8_size - int8_weight_size + float32_weight_size
             = int8_size + 3 * int8_weight_size
```

This is only an estimate. Actual model sizes also depend on metadata, quantization parameters, biases, and other non-weight data.

The main observation is that layer count is a major contributor to fixed model overhead. The 90% pruned model still had 14 layers and a 10.0 KB INT8 model, while the 4layer model had 197 parameters and a 3.5 KB INT8 model. Parameter count alone therefore does not determine the final TensorFlow Lite model size.

## Data Format

The recording script records 2 seconds per sample by default. Each CSV file uses the following header:

```text
timestamp,ax,ay,az,gx,gy,gz,label
```

The six axes contain raw MPU6050 integers, not physical units. Training data is resampled to 200 frames and normalized with the mean and standard deviation calculated from the training split.

The current `data/` directory contains 778 CSV files, and the class distribution is not fully balanced. The latest generated `data.npz` contains 547 windows after short-sequence filtering, split into 382 training, 82 validation, and 83 test windows.

**Note: the current `data/` directory and `data.npz` are for pipeline reference only.** The dataset has been modified repeatedly, its quality is poor, and its versions are inconsistent. Do not use it directly for final training or as the basis for reported accuracy. Recollect or clean the data, then run `dataset.py`, `train.py`, and `export_tflite.py` again.

## Training Data Quality

This project is highly sensitive to training data quality. The same model can reach high accuracy when the data has clear gesture boundaries and a balanced class distribution. Validation and test accuracy can drop significantly when data contains incorrect labels, truncated gestures, mixed classes, inconsistent device orientation, duplicate samples, or train/test samples recorded in adjacent parts of the same session.

Check the following points when building the dataset:

- Each CSV should contain only the target gesture. Exclude preparation movements, pauses, and hand changes.
- Keep the start point, end point, motion amplitude, and grip direction consistent within each class.
- Record `idle` under several realistic stationary poses instead of using only one pose.
- Balance the class sample counts and check for duplicate or highly similar recordings.
- Split data by recording session, subject, or device whenever possible to reduce leakage.
- After regenerating `data.npz`, retrain, requantize, and remeasure. Do not reuse accuracy from an older dataset version.

In a historical experiment, the **70% pruned version reached a maximum accuracy of 100%**. This shows that a smaller model can achieve very high accuracy on a high-quality, distribution-consistent dataset. However, 100% often also indicates limited test-set difficulty and should not be presented as cross-user generalization without a separate independent test. Include the matching confusion matrix, dataset split, and training log when reporting this result.

## PC-Side Training and Export

Python 3.11 or newer is recommended. Install the dependencies:

```bash
cd imu_project
python -m pip install -r requirements.txt
```

Run the pipeline in order:

```bash
python dataset.py
python train.py
python export_tflite.py
```

The output paths are configured in `imu_project/config.py`:

```text
models/4layer/imu_model_4layer.keras
models/4layer/imu_model_4layer_int8.tflite
models/4layer/imu_model_4layer_int8.h
```

`export_tflite.py` prints `MEAN`, `STD`, `INPUT_SCALE`, `INPUT_ZERO`, `OUTPUT_SCALE`, and `OUTPUT_ZERO`. After changing the model, copy these values into `main/config.h` and place the generated `.h` file in `components/imu_model/include/`.

## ESP32 Build and Run

Requirements: ESP-IDF 5.1 or newer. ESP-IDF 5.5.5 is the version recorded for this project.

```bash
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

To switch between collection and inference modes, edit `main/CMakeLists.txt`:

```cmake
set(APP_MODE "infer")   # or "collect"
```

Collection mode outputs:

```text
$ax,ay,az,gx,gy,gz
```

Read or record data from the PC:

```bash
python serial_reader.py --port COM3
python record.py --port COM3 --label circle --count 50 --duration 2.0
```

`<PORT>`, `COM3`, and `/dev/ttyUSB0` are examples. Do not commit a hard-coded serial port from a personal machine.

## Current Metrics and Reproduction Status

| Metric | Value | Evidence Status |
| --- | ---: | --- |
| Model parameters | 197 | Verified from the Keras model |
| INT8 model size | 3,616 B | Verified |
| Reserved tensor arena | 16,384 B | Defined in `main_infer.cpp` |
| Reported tensor arena usage | 3,360 B | Board-side serial log supplied by the author |
| Accuracy on the current test set | Retraining required | The dataset changed; old logs cannot be reused |
| Pure inference latency | Approximately 3.70 ms steady state | Board log range: 3.695-3.716 ms |
| Flash `.flash.text` | 149,506 B | Verified from the ELF before cleaning `build/` |
| DRAM data+bss | 32,284 B | Verified from the ELF before cleaning `build/` |
| IRAM text+vectors | 57,515 B | Verified from the ELF before cleaning `build/` |

The following limitations affect how these metrics should be interpreted:

1. Document the origin of `data.npz` in the final submission. If it is not the exact snapshot used by the final model, regenerate it and record its SHA-256.
2. The board-side `latency` value measures only `MicroInterpreter::Invoke()`. It excludes normalization, INT8 quantization, sampling time, serial output, and main-loop overhead.
3. The current `sdkconfig` uses the Debug optimization level. For performance comparisons, use an explicit Release or Performance configuration and preserve the serial log, firmware revision, and sample count.
4. `export_tflite.py` now uses `np.rint()` to match the device-side `roundf()` behavior. After retraining, evaluate with the corrected script instead of reusing accuracy obtained through truncation.

## Reproducibility Checklist

Keep the following artifacts for the final submission or release:

- The raw CSV files or `data.npz` that exactly match the final model, including its SHA-256.
- The complete `train.py` output, including the dataset split, class distribution, confusion matrix, and test accuracy.
- The complete `export_tflite.py` output, including float32 and INT8 accuracy and all quantization parameters.
- The original ESP32 serial log, including `arena used`, `model data len`, and multiple `latency` samples.
- The ESP-IDF, TensorFlow, Python, and dependency versions used for the final build.

## Cleanup Before Submission

The following items should not be submitted as source code:

- `build/`
- `managed_components/`
- `imu_project/__pycache__/`
- `sdkconfig.old`
- `pytest_hello_world.py`, which still tests the original ESP-IDF Hello World example
- `.vscode/` settings containing local paths or serial ports

The raw dataset is relatively large. If the course or reviewer does not require it, submit only the model and training code. If data is required, submit a confirmed final `data.npz` instead of the complete `data/` directory plus multiple abandoned training versions.

## Known Limitations

- The dataset is small and imbalanced.
- The split is not grouped by recording session, subject, or device, so random window splitting may overestimate cross-session generalization.
- Sampling relies on FreeRTOS delays, and the actual sampling period is not verified.
- The driver reads `WHO_AM_I` but only prints the result; it does not return an error when the device ID does not match.
- The MPU6050 data does not include temperature compensation, bias calibration, or a unified mounting orientation.
