#include "bsp_lcd.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <esp_lcd_gc9a01.h>

#define TAG "BSP_LCD"

// --- 硬件配置 (私有) ---
#define LCD_HOST            SPI3_HOST
#define PIN_NUM_LCD_CS      (-1)
#define PIN_NUM_LCD_PCLK    (GPIO_NUM_8)
#define PIN_NUM_LCD_DATA0   (GPIO_NUM_9)
#define PIN_NUM_LCD_RST     (GPIO_NUM_10)
#define PIN_NUM_LCD_DC      (GPIO_NUM_11)

// --- 屏幕物理参数 (私有) ---
#define LCD_H_RES           (240)
#define LCD_V_RES           (240)
#define LCD_BIT_PER_PIXEL   (16)

// 模块内部静态变量，外部无法访问
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static SemaphoreHandle_t s_dma_done_sem = NULL;

// DMA 完成回调函数
static bool IRAM_ATTR lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    SemaphoreHandle_t dma_done_sem = (SemaphoreHandle_t)user_ctx;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(dma_done_sem, &task_woken);
    return (task_woken == pdTRUE);
}

// --- 公共 API 实现 ---

esp_err_t bsp_lcd_init(void)
{
    s_dma_done_sem = xSemaphoreCreateBinary();
    if (s_dma_done_sem == NULL) {
        ESP_LOGE(TAG, "创建DMA信号量失败!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SPI总线初始化中");
    const spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_LCD_DATA0,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_LCD_PCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (LCD_H_RES * LCD_V_RES * LCD_BIT_PER_PIXEL / 8),
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
        .user_ctx = s_dma_done_sem, // 传递内部信号量
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &s_io_handle));

    ESP_LOGI(TAG, "创建GC9A01面板驱动");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(s_io_handle, &panel_config, &s_panel_handle));
    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    ESP_LOGI(TAG, "GC9A01 LCD初始化完成");
    return ESP_OK;
}

uint16_t bsp_lcd_get_width(void)
{
    return LCD_H_RES;
}

uint16_t bsp_lcd_get_height(void)
{
    return LCD_V_RES;
}

esp_err_t bsp_lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    return esp_lcd_panel_draw_bitmap(s_panel_handle, x_start, y_start, x_end, y_end, color_data);
}

void bsp_lcd_wait_for_draw_done(void)
{
    xSemaphoreTake(s_dma_done_sem, portMAX_DELAY);
}

esp_err_t bsp_lcd_set_power(bool on)
{
    return esp_lcd_panel_disp_on_off(s_panel_handle, on);
}

esp_err_t bsp_lcd_set_brightness(uint8_t brightness)
{
    // TODO: 在此处实现通过PWM控制背光的硬件逻辑
    ESP_LOGI(TAG, "设置亮度为 %d (占位实现)", brightness);
    return ESP_OK;
}