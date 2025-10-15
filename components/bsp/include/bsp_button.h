#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include "esp_err.h"
#include <stdbool.h>
/**
 * @brief 初始化按钮的GPIO
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_button_init(void);

/**
 * @brief 获取按钮当前是否被按下
 * 
 * @return true 按钮被按下
 * @return false 按钮未被按下
 */
bool bsp_button_is_pressed(void);

#endif // BSP_BUTTON_Hv