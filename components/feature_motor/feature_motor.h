#ifndef MADA_H
#define MADA_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化震动马达模块 (使用内部预设配置)
 *
 * @return esp_err_t ESP_OK 表示成功, 否则失败
 */
esp_err_t mada_initialize(void);

/**
 * @brief 反初始化震动马达模块
 */
void mada_deinitialize(void);

/**
 * @brief 开启马达持续震动
 */
void mada_continuous_vibrate(void);

/**
 * @brief 停止马达震动
 */
void mada_stop(void);

/**
 * @brief 设置震动强度 (仅在PWM模式下有效)
 *
 * @param intensity 强度 (0-100)
 */
void mada_set_intensity(uint8_t intensity);

/**
 * @brief 使马达震动指定的毫秒数
 *
 * @param duration_ms 震动持续时间 (毫秒)
 */
void mada_vibrate(uint32_t duration_ms);

/**
 * @brief 执行一次短震动 (例如 100ms)
 */
void mada_short_vibrate(void);

/**
 * @brief 执行一次长震动 (例如 500ms)
 */
void mada_long_vibrate(void);

/**
 * @brief 检查马达当前是否正在震动
 *
 * @return true 如果正在震动
 * @return false 如果已停止
 */
bool mada_is_vibrating(void);

#endif