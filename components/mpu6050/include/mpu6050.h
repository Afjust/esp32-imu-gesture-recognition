/*
 * ============================================================
 * mpu6050.h - MPU6050 六轴传感器驱动对外接口
 *
 * 说明：
 *   本文件只暴露"用户需要调用的函数"和"数据结构"，
 *   具体的 I2C 时序、寄存器操作都藏在 mpu6050.c 里。
 *
 * 使用方式：
 *   1. 调用 mpu6050_init() 初始化
 *   2. 循环调用 mpu6050_read_raw() 读取原始数据
 *      （或 mpu6050_read() 读取转换后的物理量）
 * ============================================================
 */

#pragma once   // 防止头文件被重复包含

#include <stdint.h>              // 提供 int16_t、uint8_t 等类型
#include "esp_err.h"             // 提供 esp_err_t、ESP_OK 等
#include "driver/i2c.h"          // I2C 驱动（legacy API，v5.x 需在 menuconfig 启用）
#include "driver/gpio.h"         // GPIO 编号定义

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 默认 I2C 地址 ----------
 * MPU6050 的 7 位地址由 AD0 引脚决定：
 *   AD0 接地 -> 0x68
 *   AD0 接 VCC -> 0x69
 * 大多数模块默认 AD0 接地，所以用 0x68
 */
#define MPU6050_ADDR_DEFAULT  0x68

/* ---------- 寄存器地址 ----------
 * 只列出本项目用到的，完整寄存器见 MPU6050 数据手册
 */
#define MPU6050_REG_SMPLRT_DIV    0x19   // 采样率分频
#define MPU6050_REG_CONFIG        0x1A   // 配置（DLPF）
#define MPU6050_REG_GYRO_CONFIG   0x1B   // 陀螺仪量程
#define MPU6050_REG_ACCEL_CONFIG  0x1C   // 加速度计量程
#define MPU6050_REG_ACCEL_XOUT_H  0x3B   // 加速度 X 高字节，后面 14 字节是 6 轴 + 温度
#define MPU6050_REG_PWR_MGMT_1    0x6B   // 电源管理 1（用来唤醒）
#define MPU6050_REG_WHO_AM_I      0x75   // 器件 ID，正常返回 0x68

/* ---------- 原始数据结构 ----------
 * MPU6050 输出的是 16 位有符号整数，直接读出来就是这些值
 * 量程不同，对应的物理量换算比例也不同
 */
typedef struct {
    int16_t ax;   // 加速度 X 原始值
    int16_t ay;   // 加速度 Y 原始值
    int16_t az;   // 加速度 Z 原始值
    int16_t gx;   // 角速度 X 原始值
    int16_t gy;   // 角速度 Y 原始值
    int16_t gz;   // 角速度 Z 原始值
} mpu6050_raw_t;

/* ---------- 转换后的物理量 ----------
 * 加速度单位：g（重力加速度，1g ≈ 9.8 m/s²）
 * 角速度单位：°/s（度每秒）
 */
typedef struct {
    float ax;   // 单位 g
    float ay;
    float az;
    float gx;   // 单位 °/s
    float gy;
    float gz;
} mpu6050_data_t;

/*
 * 初始化 MPU6050
 * 参数：
 *   port      I2C 端口号，如 I2C_NUM_0
 *   sda_gpio  SDA 引脚，如 GPIO_NUM_21
 *   scl_gpio  SCL 引脚，如 GPIO_NUM_22
 *   addr      7 位 I2C 地址，AD0 接地用 0x68
 * 返回：
 *   ESP_OK 成功，其他为错误码
 *
 * 内部会做：
 *   1. 配置 I2C 主机
 *   2. 安装 I2C 驱动
 *   3. 唤醒 MPU6050
 *   4. 设置采样率、DLPF、量程
 *   5. 读一次 WHO_AM_I 打印日志
 */
esp_err_t mpu6050_init(i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio, uint8_t addr);

/*
 * 读取原始 6 轴数据
 * 参数：
 *   raw  输出参数，读到的数据会写到这里
 * 返回：
 *   ESP_OK 成功
 */
esp_err_t mpu6050_read_raw(mpu6050_raw_t *raw);

/*
 * 读取转换后的物理量（加速度 g，角速度 °/s）
 * 依赖 mpu6050_read_raw() 之后再除以量程系数
 */
esp_err_t mpu6050_read(mpu6050_data_t *data);

/*
 * 读取 WHO_AM_I 寄存器
 * 正常应该返回 0x68，可以用来判断 I2C 是否通
 */
esp_err_t mpu6050_who_am_i(uint8_t *who);

#ifdef __cplusplus
}
#endif