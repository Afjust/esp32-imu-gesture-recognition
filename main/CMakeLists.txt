# ============================================================
# 选择模式：
#   "collect"  → 编译 main_collect.cpp（采集）
#   "infer"    → 编译 main_infer.cpp（推理）
#
# 修改下面的 APP_MODE 即可切换。
# ============================================================
set(APP_MODE "infer")

if(APP_MODE STREQUAL "collect")
    set(MAIN_SRC "main_collect.cpp")
    message(STATUS "编译模式：采集 (collect)")
else()
    set(MAIN_SRC "main_infer.cpp")
    message(STATUS "编译模式：推理 (infer)")
endif()

idf_component_register(
    SRCS ${MAIN_SRC}
    INCLUDE_DIRS "."
    REQUIRES mpu6050 imu_model espressif__esp-tflite-micro esp_timer
)