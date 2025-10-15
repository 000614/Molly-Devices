#include "bsp_lcd.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <esp_lcd_gc9a01.h>

#define TAG "BSP_LCD"

// --- 引脚定义 ---
#define LCD_HOST            SPI3_HOST
#define PIN_NUM_LCD_CS      (-1)
#define PIN_NUM_LCD_PCLK    (GPIO_NUM_8)
#define PIN_NUM_LCD_DATA0   (GPIO_NUM_9)
#define PIN_NUM_LCD_RST     (GPIO_NUM_10)
#define PIN_NUM_LCD_DC      (GPIO_NUM_11)

#define LCD_H_RES           (240)
#define LCD_V_RES           (240)
#define LCD_BIT_PER_PIXEL   (16)
#define FRAME_BUFFER_SIZE   (LCD_H_RES * LCD_V_RES * LCD_BIT_PER_PIXEL / 8)

// 硬件句柄的定义
esp_lcd_panel_io_handle_t g_bsp_io_handle = NULL;
esp_lcd_panel_handle_t g_bsp_panel_handle = NULL;

// DMA完成回调函数现在是BSP的一部分
static bool IRAM_ATTR lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    // user_ctx 现在被用来传递信号量句柄
    SemaphoreHandle_t dma_done_sem = (SemaphoreHandle_t)user_ctx;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(dma_done_sem, &task_woken);
    return (task_woken == pdTRUE);
}

esp_err_t bsp_lcd_init(SemaphoreHandle_t dma_done_sem)
{
    ESP_LOGI(TAG, "SPI总线初始化中");
    const spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_LCD_DATA0,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_LCD_PCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = FRAME_BUFFER_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "创建SPI面板IO句柄");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 60 * 1000 * 1000,
        .trans_queue_depth = 1,
        .on_color_trans_done = lcd_trans_done_cb,
        .user_ctx = dma_done_sem, // 将信号量句柄传递给回调函数
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &g_bsp_io_handle));

    ESP_LOGI(TAG, "创建GC9A01面板驱动");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(g_bsp_io_handle, &panel_config, &g_bsp_panel_handle));
    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_bsp_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_bsp_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(g_bsp_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(g_bsp_panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_bsp_panel_handle, true));

    ESP_LOGI(TAG, "GC9A01 LCD初始化完成");
    return ESP_OK;
}