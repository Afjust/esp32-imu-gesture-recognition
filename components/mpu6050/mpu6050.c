/*
 * ============================================================
 * mpu6050.c - MPU6050 驱动实现
 *
 * 分层思路：
 *   - 底层：mpu6050_write_byte / mpu6050_read_bytes 只负责 I2C 读写
 *   - 中层：mpu6050_init 负责配置寄存器
 *   - 上层：mpu6050_read_raw / mpu6050_read 负责给用户提供数据
 *
 * 为什么要这么分？
 *   以后换传感器、改量程、加 DMP，只改底层或中层，
 *   上层的 main.c 完全不用动。
 * ============================================================
 */

#include "mpu6050.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 日志 TAG，串口打印时用来区分模块 */
static const char *TAG = "MPU6050";

/* 保存初始化的 I2C 端口和地址，后面读写都要用 */
static i2c_port_t s_i2c_port;
static uint8_t    s_addr;

/*
 * ------------------------------------------------------------
 * 底层：向指定寄存器写一个字节
 * ------------------------------------------------------------
 * I2C 写时序：
 *   START -> 从机地址+写 -> 寄存器地址 -> 数据 -> STOP
 */
static esp_err_t mpu6050_write_byte(uint8_t reg, uint8_t data)
{
    // 创建一个 I2C 命令链，用来描述一次完整的 I2C 传输
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // 发送 START 信号
    i2c_master_start(cmd);

    // 发送从机地址 + 写位（地址左移 1 位，最低位 0 表示写）
    i2c_master_write_byte(cmd, (s_addr << 1) | I2C_MASTER_WRITE, true);

    // 发送要写的寄存器地址
    i2c_master_write_byte(cmd, reg, true);

    // 发送要写的数据
    i2c_master_write_byte(cmd, data, true);

    // 发送 STOP 信号
    i2c_master_stop(cmd);

    // 执行命令链，超时 1000ms
    esp_err_t ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(1000));

    // 删除命令链，释放内存
    i2c_cmd_link_delete(cmd);

    return ret;
}

/*
 * ------------------------------------------------------------
 * 底层：从指定寄存器连续读 len 个字节
 * ------------------------------------------------------------
 * I2C 读时序：
 *   START -> 从机地址+写 -> 寄存器地址
 *         -> START（重复起始） -> 从机地址+读 -> 读 N 字节 -> STOP
 *
 * 注意：
 *   前面 N-1 个字节读完后主机要回 ACK，
 *   最后一个字节主机要回 NACK，告诉从机"读完了"
 */
static esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // 第一次 START
    i2c_master_start(cmd);

    // 从机地址 + 写，表示接下来要发寄存器地址
    i2c_master_write_byte(cmd, (s_addr << 1) | I2C_MASTER_WRITE, true);

    // 要读的寄存器起始地址
    i2c_master_write_byte(cmd, reg, true);

    // 重复 START，切换到读模式
    i2c_master_start(cmd);

    // 从机地址 + 读
    i2c_master_write_byte(cmd, (s_addr << 1) | I2C_MASTER_READ, true);

    // 先读 len-1 个字节，每读完一个主机回 ACK
    if (len > 1) {
        i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
    }

    // 最后一个字节读完后主机回 NACK
    i2c_master_read_byte(cmd, buf + len - 1, I2C_MASTER_NACK);

    // STOP
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

/*
 * ------------------------------------------------------------
 * 初始化 MPU6050
 * ------------------------------------------------------------
 */
esp_err_t mpu6050_init(i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio, uint8_t addr)
{
    // 保存参数到静态变量，后面读写复用
    s_i2c_port = port;
    s_addr     = addr;

    /* ---- 第 1 步：配置 I2C 主机 ---- */
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,     // 主机模式
        .sda_io_num       = sda_gpio,            // SDA 引脚
        .scl_io_num       = scl_gpio,            // SCL 引脚
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,  // 使能内部上拉（模块外部一般也有）
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,              // 400kHz，MPU6050 支持
    };

    esp_err_t ret = i2c_param_config(s_i2c_port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ---- 第 2 步：安装 I2C 驱动 ---- */
    // 参数：端口、模式、接收缓冲、发送缓冲、中断标志
    // 主机模式不需要缓冲，都填 0
    ret = i2c_driver_install(s_i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ---- 第 3 步：配置 MPU6050 寄存器 ---- */

    // 电源管理 1：写 0x00 表示唤醒（默认是睡眠状态）
    ret = mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK) return ret;

    // 采样率分频：1kHz / (1 + 9) = 100Hz
    // 也就是说每秒输出 100 组 6 轴数据
    ret = mpu6050_write_byte(MPU6050_REG_SMPLRT_DIV, 0x09);
    if (ret != ESP_OK) return ret;

    // DLPF（数字低通滤波）：0x03 对应 44Hz 带宽
    // 可以滤掉一部分高频抖动，让数据更平滑
    ret = mpu6050_write_byte(MPU6050_REG_CONFIG, 0x03);
    if (ret != ESP_OK) return ret;

    // 陀螺仪量程：0x00 表示 ±250 °/s
    // 换算系数：32768 / 250 = 131 LSB/(°/s)
    ret = mpu6050_write_byte(MPU6050_REG_GYRO_CONFIG, 0x18);
    if (ret != ESP_OK) return ret;

    // 加速度计量程：0x10 表示 ±8g
    // 快速挥手时瞬时加速度可能超过 2g，改用 ±8g
    ret = mpu6050_write_byte(MPU6050_REG_ACCEL_CONFIG, 0x10);
    if (ret != ESP_OK) return ret;

    /* ---- 第 4 步：读 WHO_AM_I 验证 I2C 是否通 ---- */
    uint8_t who = 0;
    ret = mpu6050_who_am_i(&who);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WHO_AM_I = 0x%02X (expected 0x68)", who);
    } else {
        ESP_LOGE(TAG, "read WHO_AM_I failed: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

/*
 * ------------------------------------------------------------
 * 读取原始 6 轴数据
 * ------------------------------------------------------------
 * 从 0x3B 开始连续读 14 个字节：
 *   0x3B~0x3C: AX
 *   0x3D~0x3E: AY
 *   0x3F~0x40: AZ
 *   0x41~0x42: 温度（这里跳过）
 *   0x43~0x44: GX
 *   0x45~0x46: GY
 *   0x47~0x48: GZ
 */
esp_err_t mpu6050_read_raw(mpu6050_raw_t *raw)
{
    if (!raw) return ESP_ERR_INVALID_ARG;

    uint8_t buf[14];
    esp_err_t ret = mpu6050_read_bytes(MPU6050_REG_ACCEL_XOUT_H, buf, 14);
    if (ret != ESP_OK) return ret;

    // 高字节在前，低字节在后，拼成 16 位有符号整数
    raw->ax = (int16_t)((buf[0] << 8) | buf[1]);
    raw->ay = (int16_t)((buf[2] << 8) | buf[3]);
    raw->az = (int16_t)((buf[4] << 8) | buf[5]);
    // buf[6] 和 buf[7] 是温度，跳过
    raw->gx = (int16_t)((buf[8]  << 8) | buf[9]);
    raw->gy = (int16_t)((buf[10] << 8) | buf[11]);
    raw->gz = (int16_t)((buf[12] << 8) | buf[13]);

    return ESP_OK;
}

/*
 * ------------------------------------------------------------
 * 读取转换后的物理量
 * ------------------------------------------------------------
 * 换算：
 *   加速度 = 原始值 / 4096.0f    （±8g 量程）
 *   角速度 = 原始值 / 16.4f      （±2000 °/s 量程）
 */
esp_err_t mpu6050_read(mpu6050_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;

    mpu6050_raw_t raw;
    esp_err_t ret = mpu6050_read_raw(&raw);
    if (ret != ESP_OK) return ret;

   // ±8g 量程，系数 4096
    data->ax = raw.ax / 4096.0f;
    data->ay = raw.ay / 4096.0f;
    data->az = raw.az / 4096.0f;

    // ±2000 °/s 量程，系数 16.4
    data->gx = raw.gx / 16.4f;
    data->gy = raw.gy / 16.4f;
    data->gz = raw.gz / 16.4f;

    return ESP_OK;
}

/*
 * ------------------------------------------------------------
 * 读取 WHO_AM_I
 * ------------------------------------------------------------
 */
esp_err_t mpu6050_who_am_i(uint8_t *who)
{
    if (!who) return ESP_ERR_INVALID_ARG;
    return mpu6050_read_bytes(MPU6050_REG_WHO_AM_I, who, 1);
}
