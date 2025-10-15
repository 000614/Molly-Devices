#ifndef BSP_LCD_H
#define BSP_LCD_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_ops.h"

// 将硬件相关的句柄声明为外部变量，以便功能层可以访问
extern esp_lcd_panel_handle_t g_bsp_panel_handle;

/**
 * @brief 初始化LCD硬件
 * 
 * @param dma_done_sem DMA传输完成时要释放的信号量
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bsp_lcd_init(SemaphoreHandle_t dma_done_sem);

#endif // BSP_LCD_H