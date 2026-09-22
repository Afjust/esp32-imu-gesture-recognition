/*
 * main_collect.cpp - 采集模式
 *
 * 串口输出 6 轴原始数据，Python 端用 record.py 接收。
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

extern "C" {
#include "mpu6050.h"
}

static const char *TAG = "COLLECT";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "采集模式启动");

    esp_err_t ret = mpu6050_init(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22,
                                 MPU6050_ADDR_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mpu6050_init failed");
        return;
    }

    mpu6050_raw_t raw;
    while (1) {
        if (mpu6050_read_raw(&raw) == ESP_OK) {
            printf("$%d,%d,%d,%d,%d,%d\n",
                   raw.ax, raw.ay, raw.az,
                   raw.gx, raw.gy, raw.gz);
        }
        vTaskDelay(pdMS_TO_TICKS(10));   // 100Hz
    }
}