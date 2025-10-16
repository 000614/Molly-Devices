#ifndef MADA_DRIVER_H
#define MADA_DRIVER_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

// 定义马达的控制模式
typedef enum {
    MADA_CONTROL_DIGITAL, // 数字GPIO控制模式
    MADA_CONTROL_PWM      // PWM控制模式
} mada_driver_mode_t;

// 驱动配置结构体
typedef struct {
    gpio_num_t pin;                 // 使用的GPIO引脚
    mada_driver_mode_t mode;        // 控制模式 (PWM 或 Digital)
    ledc_channel_t ledc_channel;    // PWM模式下使用的LEDC通道
} mada_driver_config_t;

/**
 * @brief 初始化震动马达驱动
 *
 * @param config 指向驱动配置结构体的指针
 * @return esp_err_t ESP_OK 表示成功, 否则失败
 */
esp_err_t mada_driver_init(const mada_driver_config_t* config);

/**
 * @brief 反初始化震动马达驱动
 */
void mada_driver_deinit(void);

/**
 * @brief 开启马达震动 (以最大强度)
 */
void mada_driver_on(void);

/**
 * @brief 关闭马达震动
 */
void mada_driver_off(void);

/**
 * @brief 设置马达震动强度 (仅在PWM模式下有效)
 *
 * @param intensity 强度值 (0-100)
 */
void mada_driver_set_intensity(uint8_t intensity);

#endif // MADA_DRIVER_H