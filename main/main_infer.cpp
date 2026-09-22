/*
 * main_infer.cpp - 推理模式
 *
 * 换模型只改 config.h，本文件不用动。
 */

#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

extern "C" {
#include "mpu6050.h"
}

#include "config.h"      // ← 所有配置和模型都在这里

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char *TAG = "INFER";

static const int kTensorArenaSize = 16 * 1024;
static uint8_t tensor_arena[kTensorArenaSize];

static int16_t sample_buffer[SEQ_LEN * NUM_AXES];


static void perform_inference(tflite::MicroInterpreter* interpreter,
                              unsigned long* last_latency_us)
{
    TfLiteTensor* input  = interpreter->input(0);
    TfLiteTensor* output = interpreter->output(0);
    int8_t* input_data   = input->data.int8;

    for (int t = 0; t < SEQ_LEN; t++) {
        for (int c = 0; c < NUM_AXES; c++) {
            float x = ((float)sample_buffer[t * NUM_AXES + c] - MEAN[c]) / STD[c];
            int q = (int)roundf(x / INPUT_SCALE) + INPUT_ZERO;
            if (q > 127)  q = 127;
            if (q < -128) q = -128;
            input_data[t * NUM_AXES + c] = (int8_t)q;
        }
    }

    unsigned long t0 = esp_timer_get_time();
    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke failed");
        return;
    }
    unsigned long t1 = esp_timer_get_time();
    *last_latency_us = t1 - t0;

    int8_t* output_data = output->data.int8;
    float logits[NUM_CLASSES];
    for (int i = 0; i < NUM_CLASSES; i++) {
        logits[i] = (output_data[i] - OUTPUT_ZERO) * OUTPUT_SCALE;
    }

    int pred = 0;
    for (int i = 1; i < NUM_CLASSES; i++) {
        if (logits[i] > logits[pred]) pred = i;
    }

    ESP_LOGI(TAG, "pred=%s", LABELS[pred]);
    printf("  logits:");
    for (int i = 0; i < NUM_CLASSES; i++) {
        printf(" %s=%.2f", LABELS[i], logits[i]);
    }
    printf("  latency=%lu us\n", *last_latency_us);
}


extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "推理模式启动");

    esp_err_t ret = mpu6050_init(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22,
                                 MPU6050_ADDR_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mpu6050_init failed");
        return;
    }

    // 用宏，不用具体名字
    const tflite::Model* model = tflite::GetModel(MODEL_DATA);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "schema mismatch");
        return;
    }

    static tflite::MicroMutableOpResolver<20> resolver;
    resolver.AddConv2D();
    resolver.AddFullyConnected();
    resolver.AddMaxPool2D();
    resolver.AddAveragePool2D();
    resolver.AddReshape();
    resolver.AddExpandDims();
    resolver.AddSqueeze();
    resolver.AddAdd();
    resolver.AddMul();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddMean();

    static tflite::MicroInterpreter interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);

    if (interpreter.AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors failed");
        return;
    }
    ESP_LOGI(TAG, "arena used: %d bytes", interpreter.arena_used_bytes());
    ESP_LOGI(TAG, "model data len: %d bytes", MODEL_DATA_LEN);

    mpu6050_raw_t raw;
    int idx = 0;
    unsigned long latency = 0;

    while (1) {
        if (mpu6050_read_raw(&raw) == ESP_OK) {
            sample_buffer[idx * NUM_AXES + 0] = raw.ax;
            sample_buffer[idx * NUM_AXES + 1] = raw.ay;
            sample_buffer[idx * NUM_AXES + 2] = raw.az;
            sample_buffer[idx * NUM_AXES + 3] = raw.gx;
            sample_buffer[idx * NUM_AXES + 4] = raw.gy;
            sample_buffer[idx * NUM_AXES + 5] = raw.gz;

            idx++;
            if (idx >= SEQ_LEN) {
                perform_inference(&interpreter, &latency);
                idx = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}