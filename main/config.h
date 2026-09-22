/*
 * config.h - ESP32 侧部署参数
 *
 * 换模型时只改下面"模型数据"那一段。
 */

#pragma once

// ============================================================
// 模型配置
// ============================================================
#define NUM_CLASSES    5
#define SEQ_LEN        200
#define NUM_AXES       6

static const char* LABELS[NUM_CLASSES] = {
    "idle", "circle", "wave", "shake", "figure8"
};

// ============================================================
// 标准化参数
// ============================================================
static const float MEAN[6] = {
    1215.0414f, 898.9021f, 3746.3613f,
    -52.66946f, 52.689724f, 35.127434f
};
static const float STD[6] = {
    4345.182f, 5751.598f, 5234.625f,
    2153.3938f, 2338.2886f, 1111.1595f
};

// ============================================================
// 量化参数
// ============================================================
static const float INPUT_SCALE  = 0.08132993429899216f;
static const int   INPUT_ZERO   = 36;
static const float OUTPUT_SCALE = 0.4219502806663513f;
static const int   OUTPUT_ZERO  = 19;

// ============================================================
// 模型数据（换模型只改这两行）
// ============================================================
#include "imu_model_4layer_int8.h"

#define MODEL_DATA      imu_model_4layer_int8
#define MODEL_DATA_LEN  imu_model_4layer_int8_len