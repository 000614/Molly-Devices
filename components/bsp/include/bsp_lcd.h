#ifndef BSP_LCD_H
#define BSP_LCD_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化LCD硬件及相关驱动
 * @return esp_err_t ESP_OK 成功, 其他值失败
 */
esp_err_t bsp_lcd_init(void);

/**
 * @brief 获取屏幕的宽度
 * @return uint16_t 屏幕宽度 (像素)
 */
uint16_t bsp_lcd_get_width(void);

/**
 * @brief 获取屏幕的高度
 * @return uint16_t 屏幕高度 (像素)
 */
uint16_t bsp_lcd_get_height(void);

/**
 * @brief 将缓冲区中的图像数据绘制到屏幕
 * 
 * 此函数会启动一次DMA传输。函数返回后，传输可能仍在后台进行。
 * 
 * @param x_start 起始点 x 坐标
 * @param y_start 起始点 y 坐标
 * @param x_end   结束点 x 坐标
 * @param y_end   结束点 y 坐标
 * @param color_data 指向包含图像数据的缓冲区的指针
 * @return esp_err_t ESP_OK 成功, 其他值失败
 */
esp_err_t bsp_lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);

/**
 * @brief 等待上一次的 `bsp_lcd_draw_bitmap` 操作完成
 * 
 * 这是一个阻塞函数，会等待DMA传输完成。
 */
void bsp_lcd_wait_for_draw_done(void);

/**
 * @brief 控制屏幕的电源（开/关）
 * @param on true: 打开屏幕, false: 关闭屏幕
 * @return esp_err_t ESP_OK 成功, 其他值失败
 */
esp_err_t bsp_lcd_set_power(bool on);

/**
 * @brief 设置屏幕背光亮度
 * @note  此功能需要硬件支持 (如PWM)。当前为占位实现。
 * @param brightness 亮度值 (0-255)
 * @return esp_err_t ESP_OK 成功, 其他值失败
 */
esp_err_t bsp_lcd_set_brightness(uint8_t brightness);

#endif // BSP_LCD_H